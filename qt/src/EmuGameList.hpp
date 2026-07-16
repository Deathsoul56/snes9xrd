#pragma once

#include <QAbstractTableModel>
#include <QDateTime>
#include <QHash>
#include <QString>
#include <QStringList>
#include <vector>

struct GameListEntry
{
    QString path;
    QString region;       // "NTSC", "PAL", "NTSC-J", etc.
    QString serial;       // e.g. "SHVC-ABCE"
    QString file_type;    // ".smc", ".sfc", ".fig", ...
    uint64_t file_size = 0;
    qint64 mtime_ms = 0;  // last-modified time, used to invalidate the scan cache.
    uint32_t crc32 = 0;
    bool valid = false;   // false if the file couldn't be read/decompressed.
};

class EmuGameList : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column
    {
        Column_FileTitle = 0,
        Column_Region,
        Column_Size,
        Column_Serial,
        Column_Count,
    };

    enum Roles
    {
        PathRole = Qt::UserRole + 1,
    };

    explicit EmuGameList(QObject *parent = nullptr);
    ~EmuGameList() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    void refresh();
    QStringList folders() const { return folders_; }
    void setFolders(const QStringList &folders);

    int entryCount() const { return static_cast<int>(entries_.size()); }
    const GameListEntry *entryAt(int row) const;
    QModelIndex indexForPath(const QString &path) const;

    void clear();

signals:
    void scanProgress(int done, int total);
    void scanFinished(int count);

private:
    static GameListEntry parseRom(const QString &path);
    static QString regionForCode(uint8_t code);
    static QHash<QString, GameListEntry> loadCache();
    static void saveCache(const std::vector<GameListEntry> &entries);

    std::vector<GameListEntry> entries_;
    QStringList folders_;
};
