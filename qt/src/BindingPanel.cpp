#include "BindingPanel.hpp"
#include "EmuApplication.hpp"
#include <QStyleHints>
#include <QPushButton>
#include <QHBoxLayout>

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
    // "Clear Current/All Controllers" for the whole table.
    bindingTableWidget->setColumnCount(width + 1);
    bindingTableWidget->setHorizontalHeaderItem(width, new QTableWidgetItem(QObject::tr("Clear")));
    for (int row = 0; row < height; row++)
    {
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
        bindingTableWidget->setCellWidget(row, width, cell_widget);
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
    }
    else
    {
        app->binding_callback = nullptr;
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
                        QIcon());
}

void BindingPanel::fillTable()
{
    for (int column = 0; column < table_width; column++)
        for (int row = 0; row < table_height; row++)
            updateCellFromBinding(row, column);
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
    app->updateBindings();
}
