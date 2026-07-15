#include "EmuGameList.hpp"

#include "EmuApplication.hpp"
#include "snes9x.h"
#include "memmap.h"

#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>

namespace {
constexpr int SNES_HEADER_LO = 0x7FC0;
constexpr int SNES_HEADER_HI = 0xFFC0;

// Mirrors CMemory::HeaderRemove()'s arithmetic (memmap.cpp) so the library
// scanner agrees with the core on whether a ROM has a 512-byte copier
// header. ROM dumps are always a multiple of 0x2000 (8KB); a copier header
// is detected by rounding the file size down to the nearest 0x2000 and
// checking whether exactly 512 bytes are left over. The previous
// `size % 512 == 0` check was true for essentially every ROM (headered or
// not, since 0x2000 is itself a multiple of 512), so the byte-heuristic
// that followed it ended up misfiring on plain headerless ROMs and shifting
// every header read by 512 bytes, producing garbage titles/region/serial.
bool hasCopierHeader(int size)
{
    int calc_size = (size / 0x2000) * 0x2000;
    return (size - calc_size) == 512;
}

bool isPrintableAscii(uint8_t c)
{
    return c >= 0x20 && c <= 0x7E;
}

bool allAscii(const uint8_t *b, int size)
{
    for (int i = 0; i < size; i++)
        if (!isPrintableAscii(b[i])) return false;
    return true;
}

// These mirror CMemory::ScoreHiROM()/ScoreLoROM() (memmap.cpp) exactly, just
// operating on a raw file buffer instead of the core's mapped ROM array.
// The previous heuristic ("whichever header has more printable ASCII
// bytes in the title field") frequently picked the wrong map for ROMs
// whose title happens to look valid at both header locations (very common,
// since 21 semi-random bytes have a decent chance of looking "printable"),
// which is why titles kept coming out garbled. Scoring on the actual
// cartridge-type/checksum/reset-vector fields is what snes9x itself uses
// to disambiguate LoROM vs HiROM, so reusing it here fixes the same cases
// the core gets right when actually loading the ROM.
int scoreHiRom(const uint8_t *rom, int rom_size, int header_offset)
{
    int base = header_offset + 0xFF00;
    if (rom_size < base + 0x100) return -1000;
    const uint8_t *buf = rom + base;
    int score = 0;

    if (buf[0xd7] == 13 && rom_size > 1024 * 1024 * 4)
        score += 3;
    if (buf[0xd5] & 0x1)
        score += 2;
    if (buf[0xd5] == 0x23)
        score -= 2;
    if (buf[0xd4] == 0x20)
        score += 2;
    if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff)
    {
        score += 2;
        if ((buf[0xde] + (buf[0xdf] << 8)) != 0) score++;
    }
    if (buf[0xda] == 0x33)
        score += 2;
    if ((buf[0xd5] & 0xf) < 4)
        score += 2;
    if (!(buf[0xfd] & 0x80))
        score -= 6;
    if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
        score -= 2;
    if (rom_size > 1024 * 1024 * 3)
        score += 4;
    if (buf[0xd7] > 12)
        score -= 1;
    if (!allAscii(&buf[0xb0], 6))
        score -= 1;
    if (!allAscii(&buf[0xc0], 22))
        score -= 1;

    return score;
}

int scoreLoRom(const uint8_t *rom, int rom_size, int header_offset)
{
    int base = header_offset + 0x7F00;
    if (rom_size < base + 0x100) return -1000;
    const uint8_t *buf = rom + base;
    int score = 0;

    if (!(buf[0xd5] & 0x1))
        score += 3;
    if (buf[0xd5] == 0x23)
        score += 2;
    if ((buf[0xdc] + (buf[0xdd] << 8)) + (buf[0xde] + (buf[0xdf] << 8)) == 0xffff)
    {
        score += 2;
        if ((buf[0xde] + (buf[0xdf] << 8)) != 0) score++;
    }
    if (buf[0xda] == 0x33)
        score += 2;
    if ((buf[0xd5] & 0xf) < 4)
        score += 2;
    if (!(buf[0xfd] & 0x80))
        score -= 6;
    if ((buf[0xfc] + (buf[0xfd] << 8)) > 0xffb0)
        score -= 2;
    if (rom_size <= 1024 * 1024 * 16)
        score += 2;
    int shift = buf[0xd7] - 7;
    if (shift >= 0 && shift < 31 && (1 << shift) > 48)
        score -= 1;
    if (!allAscii(&buf[0xb0], 6))
        score -= 1;
    if (!allAscii(&buf[0xc0], 22))
        score -= 1;

    return score;
}


uint32_t crc32_calculate(const uint8_t *data, size_t size)
{
    static uint32_t table[256];
    static bool initialised = false;
    if (!initialised)
    {
        for (uint32_t i = 0; i < 256; i++)
        {
            uint32_t c = i;
            for (int j = 0; j < 8; j++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        initialised = true;
    }

    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++)
        crc = table[(crc ^ data[i]) & 0xff] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}
}

EmuGameList::EmuGameList(QObject *parent)
    : QAbstractTableModel(parent)
{
}

EmuGameList::~EmuGameList() = default;

int EmuGameList::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return 0;
    return static_cast<int>(entries_.size());
}

int EmuGameList::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return Column_Count;
}

QVariant EmuGameList::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= static_cast<int>(entries_.size()))
        return {};

    const auto &e = entries_[index.row()];
    if (role == Qt::DisplayRole)
    {
        switch (index.column())
        {
        case Column_FileTitle: return QFileInfo(e.path).fileName();
        case Column_Region:    return e.region;
        case Column_Size:
            if (e.file_size < 1024)        return QString::number(e.file_size) + " B";
            if (e.file_size < 1024 * 1024) return QString::number(e.file_size / 1024.0, 'f', 1) + " KB";
            return QString::number(e.file_size / (1024.0 * 1024.0), 'f', 2) + " MB";
        case Column_Serial: return e.serial;
        }
    }
    if (role == PathRole)
        return e.path;

    if (role == Qt::ToolTipRole)
    {
        return e.path + QStringLiteral("\nCRC32: ") + QString::number(e.crc32, 16).toUpper();
    }

    return {};
}

QVariant EmuGameList::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QAbstractTableModel::headerData(section, orientation, role);

    switch (section)
    {
    case Column_FileTitle: return tr("File Title");
    case Column_Region:    return tr("Region");
    case Column_Size:      return tr("Size");
    case Column_Serial: return tr("Serial");
    }
    return {};
}

void EmuGameList::clear()
{
    if (entries_.empty()) return;
    beginResetModel();
    entries_.clear();
    endResetModel();
}

void EmuGameList::setFolders(const QStringList &f) { folders_ = f; }

const GameListEntry *EmuGameList::entryAt(int row) const
{
    if (row < 0 || row >= static_cast<int>(entries_.size())) return nullptr;
    return &entries_[row];
}

QModelIndex EmuGameList::indexForPath(const QString &path) const
{
    for (size_t i = 0; i < entries_.size(); ++i)
    {
        if (entries_[i].path == path)
            return index(static_cast<int>(i), 0);
    }
    return {};
}

QString EmuGameList::regionForCode(uint8_t code)
{
    switch (code)
    {
    case 0x00: return QStringLiteral("NTSC-J");
    case 0x01: return QStringLiteral("NTSC-U");
    case 0x02: return QStringLiteral("PAL");
    case 0x03: return QStringLiteral("NTSC-J (Sweden)");
    case 0x04: return QStringLiteral("NTSC-J (China)");
    case 0x05: return QStringLiteral("NTSC-U (Korea)");
    case 0x06: return QStringLiteral("PAL (Korea)");
    case 0x07: return QStringLiteral("PAL (China)");
    default:   return QStringLiteral("Unknown");
    }
}

GameListEntry EmuGameList::parseRom(const QString &path)
{
    GameListEntry entry;
    entry.path = path;

    QFileInfo info(path);
    entry.file_size = info.size();
    entry.file_type = info.suffix().toLower();
    if (entry.file_type.startsWith('.')) entry.file_type = entry.file_type.mid(1).toUpper();

    // For ZIPs we round-trip through the core's LoadZip, which selects the
    // largest file inside the archive and returns the raw ROM bytes.
    std::vector<uint8_t> bytes_owned;
    const uint8_t *rom = nullptr;
    int rom_size = 0;

    if (entry.file_type.compare("zip", Qt::CaseInsensitive) == 0)
    {
        bytes_owned.resize(CMemory::MAX_ROM_SIZE);
        uint32_t total = 0;
        std::string path_str = path.toStdString();
        if (!LoadZip(path_str.c_str(), &total, bytes_owned.data()))
            return entry;
        rom = bytes_owned.data();
        rom_size = static_cast<int>(total);
    }
    else
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return entry;
        QByteArray file_bytes = file.readAll();
        file.close();
        if (file_bytes.isEmpty()) return entry;
        bytes_owned.assign(file_bytes.cbegin(), file_bytes.cend());
        rom = bytes_owned.data();
        rom_size = static_cast<int>(bytes_owned.size());
    }

    if (!rom || rom_size <= 0) return entry;

    entry.valid = true;

    int header_offset = hasCopierHeader(rom_size) ? 512 : 0;

    // Same tie-break rule as CMemory::InitROM(): LoROM wins ties.
    int hi_score = scoreHiRom(rom, rom_size, header_offset);
    int lo_score = scoreLoRom(rom, rom_size, header_offset);
    QString rom_map = (lo_score >= hi_score) ? QStringLiteral("LoROM") : QStringLiteral("HiROM");

    // Region at +0x19 of the SNES header.
    if (rom_map == QStringLiteral("HiROM")
        && rom_size - header_offset >= SNES_HEADER_HI + 0x19)
    {
        entry.region = regionForCode(rom[header_offset + SNES_HEADER_HI + 0x19]);
    }
    else if (rom_map == QStringLiteral("LoROM")
             && rom_size - header_offset >= SNES_HEADER_LO + 0x19)
    {
        entry.region = regionForCode(rom[header_offset + SNES_HEADER_LO + 0x19]);
    }

    // Serial: 4 bytes at +0x02 of the SNES header (often the publisher code).
    if (rom_size - header_offset >= SNES_HEADER_LO + 0x06)
    {
        const uint8_t *serial = (rom_map == QStringLiteral("HiROM"))
                                ? (rom + header_offset + SNES_HEADER_HI + 0x02)
                                : (rom + header_offset + SNES_HEADER_LO + 0x02);
        char serial_buf[5] = {};
        for (int i = 0; i < 4; i++) serial_buf[i] = static_cast<char>(serial[i] & 0x7F);
        QString s = QString::fromLatin1(serial_buf, 4).trimmed();
        if (!s.isEmpty()) entry.serial = s.toUpper();
    }

    entry.crc32 = crc32_calculate(rom, rom_size);
    return entry;
}

void EmuGameList::refresh()
{
    // Walk each folder once and collect the file list, instead of iterating
    // the whole tree twice (once to count, once to process) — the extra
    // directory walk was pure wasted I/O, worst on network drives or huge
    // libraries.
    QStringList files;
    for (const auto &folder : folders_)
    {
        QDirIterator it(folder, EmuApplication::supportedRomExtensions(),
                        QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext())
            files.append(it.next());
    }

    // Parse into a local vector and only touch the model once at the end.
    // Doing beginInsertRows()/endInsertRows() per file made the attached
    // sorted/filtered view re-run its sort on every single insertion, which
    // on top of the per-file disk read + CRC32 made the whole scan appear
    // to freeze the UI for large libraries.
    std::vector<GameListEntry> scanned;
    scanned.reserve(files.size());
    QElapsedTimer event_pump;
    event_pump.start();
    for (int done = 0; done < files.size(); ++done)
    {
        emit scanProgress(done + 1, files.size());
        GameListEntry entry = parseRom(files[done]);
        if (entry.valid) scanned.push_back(std::move(entry));

        // Keep the UI responsive during the scan without paying the cost
        // of processing events on every single file.
        if (event_pump.elapsed() >= 50)
        {
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            event_pump.restart();
        }
    }

    beginResetModel();
    entries_ = std::move(scanned);
    endResetModel();

    emit scanFinished(static_cast<int>(entries_.size()));
}
