#include "EmuGameList.hpp"

#include "EmuApplication.hpp"
#include "snes9x.h"
#include "memmap.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

#include <cstring>

namespace {
constexpr int SNES_HEADER_LO = 0x7FC0;
constexpr int SNES_HEADER_HI = 0xFFC0;

bool isLikelyCopierHeader(const uint8_t *data)
{
    // Headerless SMC dumps with a 512-byte copier header have these bytes
    // at fixed positions; this is the heuristic snes9x uses.
    if (data[0x1F] != 0x00) return false;
    return ((data[0x16] == 0x00 || data[0x16] == 0x80 || data[0x16] == 0x40)
            && data[0x17] == 0x00
            && (data[0x14] == 0x00 || data[0x14] == 0x80));
}

bool isPrintableAscii(uint8_t c)
{
    return c >= 0x20 && c <= 0x7E;
}

QString guessRomMap(uint64_t size, const uint8_t *rom, int header_offset)
{
    const uint8_t *header = rom + header_offset + SNES_HEADER_LO - 0x7FC0;
    int printable_lo = 0, printable_hi = 0;
    for (int i = 0; i < 21; i++)
    {
        if (isPrintableAscii(header[0x10 + i])) printable_lo++;
        if (isPrintableAscii(rom[SNES_HEADER_HI + 0x10 + i])) printable_hi++;
    }
    // Prefer whichever looks more like a real ASCII title. The reset vector
    // at offset 0x20 in the SNES header (low byte) is also a hint.
    if (printable_hi > printable_lo) return QStringLiteral("HiROM");
    return QStringLiteral("LoROM");
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
        case Column_Title:  return e.title;
        case Column_Region: return e.region;
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
    case Column_Title:  return tr("Title");
    case Column_Region: return tr("Region");
    case Column_Size:   return tr("Size");
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

void EmuGameList::addFolder(const QString &path) { if (!folders_.contains(path)) folders_.append(path); }
void EmuGameList::removeFolder(const QString &path) { folders_.removeAll(path); }
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

void EmuGameList::appendEntry(GameListEntry entry)
{
    beginInsertRows(QModelIndex(), static_cast<int>(entries_.size()), static_cast<int>(entries_.size()));
    entries_.push_back(std::move(entry));
    endInsertRows();
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

    int header_offset = 0;
    if (rom_size % 512 == 0 && isLikelyCopierHeader(rom))
    {
        header_offset = 512;
    }

    QString rom_map;
    if (rom_size - header_offset >= SNES_HEADER_LO + 21)
    {
        rom_map = guessRomMap(rom_size, rom, header_offset);
    }

    // Title at SNES header offset 0x10, length 21.
    char title_buf[22] = {};
    if (rom_map == QStringLiteral("HiROM"))
    {
        std::memcpy(title_buf, rom + header_offset + SNES_HEADER_HI + 0x10 - 0xFFC0, 21);
    }
    else if (rom_map == QStringLiteral("LoROM"))
    {
        if (rom_size - header_offset >= SNES_HEADER_LO + 0x10 + 21)
            std::memcpy(title_buf, rom + header_offset + SNES_HEADER_LO - 0x7FC0 + 0x10, 21);
    }
    title_buf[21] = 0;
    int title_len = 21;
    while (title_len > 0 && title_buf[title_len - 1] == ' ') title_len--;
    title_buf[title_len] = 0;
    entry.title = QString::fromLatin1(title_buf, title_len);
    if (entry.title.isEmpty()) entry.title = info.completeBaseName();

    // Region at +0x19 of the SNES header.
    if (rom_map == QStringLiteral("HiROM")
        && rom_size - header_offset >= SNES_HEADER_HI + 0x19)
    {
        entry.region = regionForCode(rom[header_offset + SNES_HEADER_HI + 0x19 - 0xFFC0]);
    }
    else if (rom_map == QStringLiteral("LoROM")
             && rom_size - header_offset >= SNES_HEADER_LO + 0x19)
    {
        entry.region = regionForCode(rom[header_offset + SNES_HEADER_LO - 0x7FC0 + 0x19]);
    }

    // Serial: 4 bytes at +0x02 of the SNES header (often the publisher code).
    if (rom_size - header_offset >= SNES_HEADER_LO + 0x06)
    {
        const uint8_t *serial = (rom_map == QStringLiteral("HiROM"))
                                ? (rom + header_offset + SNES_HEADER_HI + 0x02 - 0xFFC0)
                                : (rom + header_offset + SNES_HEADER_LO - 0x7FC0 + 0x02);
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
    clear();
    int total = 0;
    for (const auto &folder : folders_)
    {
        QDirIterator it(folder, EmuApplication::supportedRomExtensions(),
                        QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext()) { ++total; it.next(); }
    }

    int done = 0;
    for (const auto &folder : folders_)
    {
        QDirIterator it(folder, EmuApplication::supportedRomExtensions(),
                        QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
        while (it.hasNext())
        {
            it.next();
            ++done;
            emit scanProgress(done, total);
            GameListEntry entry = parseRom(it.filePath());
            if (!entry.title.isEmpty()) appendEntry(std::move(entry));
        }
    }

    emit scanFinished(static_cast<int>(entries_.size()));
}
