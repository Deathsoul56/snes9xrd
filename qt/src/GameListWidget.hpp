#pragma once

#include <QSortFilterProxyModel>
#include <QTableView>
#include <QString>
#include <QElapsedTimer>
#include <QHash>
#include <QWidget>

class QKeyEvent;
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
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    // Grows/shrinks visible columns (evenly) relative to their default_column_widths_
    // baseline so leftover viewport space is shared out, recomputed fresh each
    // time -- toggling a column's visibility can't compound into overflow.
    void redistributeColumnSpace();

    // Shared by double-click and Enter/Return so both activate a row the same way.
    void activateIndex(const QModelIndex &proxy_index);

    EmuGameList      *model_;
    GameListProxyModel *proxy_;
    QString          keyboard_search_buffer_;
    QElapsedTimer    keyboard_search_timer_;
    QHash<int, int>  default_column_widths_;
};
