#pragma once

#include <QSortFilterProxyModel>
#include <QTableView>
#include <QString>
#include <QElapsedTimer>
#include <QWidget>

class EmuGameList;

class GameListProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    explicit GameListProxyModel(QObject *parent = nullptr);
    void setFilterText(const QString &text);
    bool filterAcceptsRow(int source_row, const QModelIndex &source_parent) const override;

private:
    QString filter_;
};

class GameListWidget : public QTableView
{
    Q_OBJECT
public:
    explicit GameListWidget(EmuGameList *model, QWidget *parent = nullptr);

    void setFilter(const QString &text);
    void applyFilter();

signals:
    void entryActivated(const QString &path);

private slots:
    void onHeaderContextMenuRequested(const QPoint &pos);

protected:
    // Qt's default keyboardSearch() only receives one character per call, so
    // multi-letter typeahead (e.g. "SUPER") and instant cycling on repeated
    // identical letters are both reimplemented here from scratch.
    void keyboardSearch(const QString &search) override;

private:
    EmuGameList      *model_;
    GameListProxyModel *proxy_;
    QString          keyboard_search_buffer_;
    QElapsedTimer    keyboard_search_timer_;
};
