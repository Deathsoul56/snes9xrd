#include "BindingPanel.hpp"
#include "EmuApplication.hpp"
#include <QStyleHints>
#include <QPushButton>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QColor>
#include <unordered_map>

BindingPanel::BindingPanel(EmuApplication *app)
    : app(app)
{
    binding_table_widget = nullptr;
    joypads_changed = nullptr;
}

void BindingPanel::setTableWidget(QTableWidget *bindingTableWidget, EmuBinding *binding, int width, int height)
{
    QString iconset = app->iconPrefix();
    keyboard_icon.addFile(iconset + "key.svg");
    joypad_icon.addFile(iconset + "joypad.svg");
    mouse_icon.addFile(iconset + "mouse.svg");
    this->binding_table_widget = bindingTableWidget;
    this->binding = binding;
    table_width = width;
    table_height = height;

    connect(bindingTableWidget, &QTableWidget::cellActivated, [&](int row, int column) {
        cellActivated(row, column);
    });
    connect(bindingTableWidget, &QTableWidget::cellPressed, [&](int row, int column) {
        cellActivated(row, column);
    });

    // One extra column at the end of each row holds a button that clears
    // just that row's bindings (all `width` slots), instead of requiring
    // "Clear Current/All Controllers" for the whole table. If a
    // default_binding_resolver was set (ShortcutsPanel), a second extra
    // column is inserted before it to reset just that row to its default.
    bool has_reset_column = static_cast<bool>(default_binding_resolver);
    int reset_column = width;
    int clear_column = has_reset_column ? width + 1 : width;
    bindingTableWidget->setColumnCount(clear_column + 1);
    if (has_reset_column)
        bindingTableWidget->setHorizontalHeaderItem(reset_column, new QTableWidgetItem(QObject::tr("Reset")));
    bindingTableWidget->setHorizontalHeaderItem(clear_column, new QTableWidgetItem(QObject::tr("Clear")));

    // Every binding table (controller, mouse, shortcuts) must line up
    // pixel-for-pixel, since switching between the controller and mouse
    // views swaps one binding table for another in the same spot -- if
    // their row-label/column widths and row heights were left to each
    // table's own content-based auto-sizing, the grid would visibly jump
    // when the view changes.
    bindingTableWidget->setIconSize(QSize(16, 16));
    bindingTableWidget->verticalHeader()->setFixedWidth(180);
    bindingTableWidget->verticalHeader()->setDefaultSectionSize(28);
    bindingTableWidget->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    bindingTableWidget->horizontalHeader()->setFixedHeight(24);
    bindingTableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    for (int col = 0; col < width; col++)
        bindingTableWidget->setColumnWidth(col, 110);
    if (has_reset_column)
        bindingTableWidget->setColumnWidth(reset_column, 60);
    bindingTableWidget->setColumnWidth(clear_column, 60);

    for (int row = 0; row < height; row++)
    {
        if (has_reset_column)
        {
            auto *reset_button = new QPushButton(QStringLiteral("\u21BA")); // ↺
            reset_button->setObjectName("resetBindingButton");
            reset_button->setToolTip(QObject::tr("Reset this shortcut to its default binding"));
            reset_button->setFixedSize(22, 22);
            reset_button->setCursor(Qt::PointingHandCursor);
            connect(reset_button, &QPushButton::clicked, [this, row](bool) {
                resetRowToDefault(row);
            });

            auto *reset_cell_widget = new QWidget;
            auto *reset_cell_layout = new QHBoxLayout(reset_cell_widget);
            reset_cell_layout->setContentsMargins(0, 0, 0, 0);
            reset_cell_layout->setAlignment(Qt::AlignCenter);
            reset_cell_layout->addWidget(reset_button);
            bindingTableWidget->setCellWidget(row, reset_column, reset_cell_widget);
        }

        auto *clear_button = new QPushButton(QStringLiteral("\u2212")); // −
        clear_button->setObjectName("clearBindingButton");
        clear_button->setToolTip(QObject::tr("Clear this binding"));
        clear_button->setFixedSize(22, 22);
        clear_button->setCursor(Qt::PointingHandCursor);
        connect(clear_button, &QPushButton::clicked, [this, row](bool) {
            clearRow(row);
        });

        auto *cell_widget = new QWidget;
        auto *cell_layout = new QHBoxLayout(cell_widget);
        cell_layout->setContentsMargins(0, 0, 0, 0);
        cell_layout->setAlignment(Qt::AlignCenter);
        cell_layout->addWidget(clear_button);
        bindingTableWidget->setCellWidget(row, clear_column, cell_widget);
    }

    fillTable();
    cell_column = -1;
    cell_row = -1;
    awaiting_binding = false;
}

BindingPanel::~BindingPanel()
{
    app->qtapp->removeEventFilter(this);
    timer.reset();
}

void BindingPanel::showEvent(QShowEvent *event)
{
    app->joypads_changed_callback = [&]
    {
        if (joypads_changed)
            joypads_changed();
    };

    QWidget::showEvent(event);
}

void BindingPanel::hideEvent(QHideEvent *event)
{
    if (awaiting_binding)
        updateCellFromBinding(cell_row, cell_column);
    awaiting_binding = false;
    setRedirectInput(false);
    app->joypads_changed_callback = nullptr;

    QWidget::hideEvent(event);
}

void BindingPanel::setRedirectInput(bool redirect)
{
    if (redirect)
    {
        app->binding_callback = [&](const EmuBinding &b)
        {
            finalizeCurrentBinding(b);
        };
        app->binding_allow_combos = allow_key_combos;
    }
    else
    {
        app->binding_callback = nullptr;
        app->binding_allow_combos = false;
    }
}

void BindingPanel::updateCellFromBinding(int row, int column)
{
    EmuBinding &b = binding[row * table_width + column];
    auto table_item = binding_table_widget->item(row, column);
    if (!table_item)
    {
        table_item = new QTableWidgetItem();
        binding_table_widget->setItem(row, column, table_item);
    }

    table_item->setText(b.to_string().c_str());;
    table_item->setIcon(b.type == EmuBinding::Keyboard ? keyboard_icon :
                        b.type == EmuBinding::Joystick ? joypad_icon :
                        b.type == EmuBinding::Mouse ? mouse_icon :
                        QIcon());
}

void BindingPanel::fillTable()
{
    for (int column = 0; column < table_width; column++)
        for (int row = 0; row < table_height; row++)
            updateCellFromBinding(row, column);
    updateConflictHighlights();
}

void BindingPanel::refreshIcons()
{
    QString iconset = app->iconPrefix();
    keyboard_icon = QIcon();
    joypad_icon = QIcon();
    mouse_icon = QIcon();
    keyboard_icon.addFile(iconset + "key.svg");
    joypad_icon.addFile(iconset + "joypad.svg");
    mouse_icon.addFile(iconset + "mouse.svg");
    if (binding_table_widget)
        fillTable();
}


void BindingPanel::updateConflictHighlights()
{
    std::unordered_map<uint32_t, int> counts;
    for (int i = 0; i < table_width * table_height; i++)
    {
        if (binding[i].type == EmuBinding::None)
            continue;
        counts[binding[i].hash()]++;
    }

    for (int row = 0; row < table_height; row++)
    {
        for (int column = 0; column < table_width; column++)
        {
            auto &b = binding[row * table_width + column];
            auto table_item = binding_table_widget->item(row, column);
            if (!table_item)
                continue;

            bool conflict = b.type != EmuBinding::None && counts[b.hash()] > 1;
            table_item->setBackground(conflict ? QBrush(QColor(230, 200, 40)) : QBrush());
            table_item->setForeground(conflict ? QBrush(QColor(0, 0, 0)) : QBrush());
        }
    }
}

void BindingPanel::cellActivated(int row, int column)
{
    // The trailing "clear this row" button column isn't a bindable cell.
    if (column >= table_width)
        return;

    if (awaiting_binding)
    {
        updateCellFromBinding(cell_row, cell_column);
    }
    cell_column = column;
    cell_row = row;

    auto table_item = binding_table_widget->item(row, column);

    if (!table_item)
    {
        table_item = new QTableWidgetItem();
        binding_table_widget->setItem(row, column, table_item);
    }

    table_item->setText("...");

    setRedirectInput(true);
    awaiting_binding = true;
    accept_return = false;
}

void BindingPanel::finalizeCurrentBinding(const EmuBinding &b)
{
    if (!awaiting_binding)
        return;
    auto &slot = binding[cell_row * this->table_width + cell_column];
    slot = b;
    if (b.type == EmuBinding::Keyboard && b.keycode == Qt::Key_Escape)
        slot = {};

    if (b.type == EmuBinding::Keyboard && b.keycode == Qt::Key_Return && !accept_return)
    {
        accept_return = true;
        return;
    }

    updateCellFromBinding(cell_row, cell_column);
    setRedirectInput(false);
    awaiting_binding = false;
    updateConflictHighlights();
    app->updateBindings();
}

void BindingPanel::onJoypadsChanged(const std::function<void()> &func)
{
    joypads_changed = func;
}

void BindingPanel::clearRow(int row)
{
    if (awaiting_binding && cell_row == row)
    {
        awaiting_binding = false;
        setRedirectInput(false);
    }

    for (int column = 0; column < table_width; column++)
    {
        binding[row * table_width + column] = {};
        updateCellFromBinding(row, column);
    }
    updateConflictHighlights();
    app->updateBindings();
}

void BindingPanel::resetRowToDefault(int row)
{
    if (!default_binding_resolver)
        return;

    if (awaiting_binding && cell_row == row)
    {
        awaiting_binding = false;
        setRedirectInput(false);
    }

    for (int column = 0; column < table_width; column++)
    {
        binding[row * table_width + column] = {};
        updateCellFromBinding(row, column);
    }

    std::string default_key = default_binding_resolver(row);
    if (!default_key.empty())
    {
        binding[row * table_width] = EmuBinding::from_config_string(default_key);
        updateCellFromBinding(row, 0);
    }

    updateConflictHighlights();
    app->updateBindings();
}
