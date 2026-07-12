#pragma once

#include <QSortFilterProxyModel>
#include <QTableView>
#include <QString>
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

private:
    EmuGameList      *model_;
    GameListProxyModel *proxy_;
};
