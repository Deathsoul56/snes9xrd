#include "GameListWidget.hpp"

#include "EmuGameList.hpp"

#include <QHeaderView>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QStyleOptionHeader>

namespace {

// QHeaderView doesn't repaint individual sections on hover through QSS
// (State_MouseOver is never fed into CE_HeaderSection per-section), so the
// hover highlight is painted manually here instead.
class GameListHeaderView : public QHeaderView
{
public:
    explicit GameListHeaderView(QWidget *parent) : QHeaderView(Qt::Horizontal, parent)
    {
        setMouseTracking(true);
    }

protected:
    void mouseMoveEvent(QMouseEvent *event) override
    {
        setHovered(logicalIndexAt(event->pos()));
        QHeaderView::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent *event) override
    {
        setHovered(-1);
        QHeaderView::leaveEvent(event);
    }

    void paintSection(QPainter *painter, const QRect &rect, int logicalIndex) const override
    {
        if (logicalIndex != hovered_ || !rect.isValid())
        {
            QHeaderView::paintSection(painter, rect, logicalIndex);
            return;
        }

        painter->save();
        painter->fillRect(rect, QColor("#23252c"));
        painter->setPen(QColor("#2a2c33"));
        painter->drawLine(rect.bottomLeft(), rect.bottomRight());

        QStyleOptionHeader opt;
        initStyleOption(&opt);
        opt.rect = rect;
        opt.sortIndicator = sortIndicatorSection() == logicalIndex
            ? (sortIndicatorOrder() == Qt::AscendingOrder ? QStyleOptionHeader::SortDown : QStyleOptionHeader::SortUp)
            : QStyleOptionHeader::None;

        if (opt.sortIndicator != QStyleOptionHeader::None)
        {
            QStyleOptionHeader arrow_opt = opt;
            arrow_opt.rect = style()->subElementRect(QStyle::SE_HeaderArrow, &opt, this);
            style()->drawPrimitive(QStyle::PE_IndicatorHeaderArrow, &arrow_opt, painter, this);
        }

        QRect label_rect = style()->subElementRect(QStyle::SE_HeaderLabel, &opt, this);
        if (!label_rect.isValid())
            label_rect = rect.adjusted(10, 0, -10, 0);

        QFont f = font();
        f.setWeight(QFont::DemiBold);
        f.setPointSize(9);
        painter->setFont(f);
        painter->setPen(QColor("#4a90ff"));
        QString text = model() ? model()->headerData(logicalIndex, Qt::Horizontal).toString() : QString();
        painter->drawText(label_rect, defaultAlignment(), text);
        painter->restore();
    }

private:
    void setHovered(int section)
    {
        if (section == hovered_)
            return;
        int old = hovered_;
        hovered_ = section;
        if (old >= 0) updateSection(old);
        if (hovered_ >= 0) updateSection(hovered_);
    }

    int hovered_ = -1;
};

} // namespace

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
    setHorizontalHeader(new GameListHeaderView(this));
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setShowGrid(false);
    setAlternatingRowColors(true);
    verticalHeader()->setVisible(false);
    verticalHeader()->setDefaultSectionSize(36);
    horizontalHeader()->setObjectName("gameHeader");
    horizontalHeader()->setStretchLastSection(true);
    horizontalHeader()->setHighlightSections(true);
    horizontalHeader()->setSectionsClickable(true);
    horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(horizontalHeader(), &QHeaderView::customContextMenuRequested,
            this, &GameListWidget::onHeaderContextMenuRequested);

    setColumnWidth(EmuGameList::Column_FileTitle, 450);
    setColumnWidth(EmuGameList::Column_Title, 250);
    setColumnWidth(EmuGameList::Column_Region, 90);
    setColumnWidth(EmuGameList::Column_Size, 80);
    setColumnWidth(EmuGameList::Column_Company, 140);
    setColumnWidth(EmuGameList::Column_Serial, 100);

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

void GameListWidget::onHeaderContextMenuRequested(const QPoint &pos)
{
    QMenu menu(this);

    for (int col = 0; col < model_->columnCount(); ++col)
    {
        QString name = model_->headerData(col, Qt::Horizontal, Qt::DisplayRole).toString();
        QAction *action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(!isColumnHidden(col));
        connect(action, &QAction::toggled, this, [this, col](bool checked) {
            setColumnHidden(col, !checked);
        });
    }

    menu.exec(horizontalHeader()->mapToGlobal(pos));
}

void GameListWidget::keyboardSearch(const QString &search)
{
    if (search.isEmpty())
        return;

    int rows = model()->rowCount();
    if (rows == 0)
        return;

    constexpr int kSearchTimeoutMs = 1000;
    if (!keyboard_search_timer_.isValid() || keyboard_search_timer_.elapsed() > kSearchTimeoutMs)
        keyboard_search_buffer_.clear();
    keyboard_search_timer_.restart();
    keyboard_search_buffer_ += search;

    bool repeat_same_char = true;
    for (const QChar &c : keyboard_search_buffer_)
    {
        if (c.toLower() != keyboard_search_buffer_.front().toLower())
        {
            repeat_same_char = false;
            break;
        }
    }

    // A run of the same letter cycles to the next match starting after the
    // current row; a growing run of different letters narrows a fresh
    // top-down search by the whole typed prefix (e.g. "SUPER").
    QString prefix = repeat_same_char ? QString(keyboard_search_buffer_.front()) : keyboard_search_buffer_;
    int current_row = currentIndex().isValid() ? currentIndex().row() : -1;
    int first_row = repeat_same_char ? (current_row + 1) : 0;

    for (int i = 0; i < rows; ++i)
    {
        int row = (first_row + i) % rows;
        QString text = model()->index(row, EmuGameList::Column_FileTitle).data(Qt::DisplayRole).toString();
        if (text.startsWith(prefix, Qt::CaseInsensitive))
        {
            QModelIndex idx = model()->index(row, currentIndex().isValid() ? currentIndex().column() : 0);
            selectionModel()->setCurrentIndex(idx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
            scrollTo(idx);
            return;
        }
    }
}
