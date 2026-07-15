#include "GameListWidget.hpp"

#include "EmuGameList.hpp"

#include <QHeaderView>
#include <QMouseEvent>

GameListProxyModel::GameListProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void GameListProxyModel::setFilterText(const QString &text)
{
    filter_ = text.trimmed();
    invalidate();
}

bool GameListProxyModel::filterAcceptsRow(int source_row, const QModelIndex &source_parent) const
{
    if (filter_.isEmpty()) return true;
    auto idx = sourceModel()->index(source_row, EmuGameList::Column_FileTitle, source_parent);
    QString title = sourceModel()->data(idx, Qt::DisplayRole).toString();
    return title.contains(filter_, Qt::CaseInsensitive);
}

GameListWidget::GameListWidget(EmuGameList *model, QWidget *parent)
    : QTableView(parent), model_(model)
{
    setObjectName("gameTable");

    proxy_ = new GameListProxyModel(this);
    proxy_->setSourceModel(model_);

    setModel(proxy_);
    setSortingEnabled(true);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    setAlternatingRowColors(true);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(36);
    horizontalHeader()->setObjectName("gameHeader");
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setHighlightSections(false);

    setColumnWidth(EmuGameList::Column_FileTitle, 360);
    setColumnWidth(EmuGameList::Column_Region, 90);
    setColumnWidth(EmuGameList::Column_Size, 100);
    setColumnWidth(EmuGameList::Column_Serial, 100);
    resizeColumnToContents(EmuGameList::Column_Size);

    sortByColumn(EmuGameList::Column_FileTitle, Qt::AscendingOrder);
    setFocusPolicy(Qt::StrongFocus);

    connect(this, &QTableView::doubleClicked, this, [this](const QModelIndex &idx) {
        if (!idx.isValid()) return;
        QModelIndex source = proxy_->mapToSource(idx);
        if (auto *entry = model_->entryAt(source.row()))
        {
            emit entryActivated(entry->path);
        }
    });
}

void GameListWidget::setFilter(const QString &text)
{
    proxy_->setFilterText(text);
}

void GameListWidget::applyFilter()
{
    proxy_->invalidate();
}
