#include "ControllerPanel.hpp"
#include "SDL3/SDL_gamepad.h"
#include "SDLInputManager.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "SnesControllerWidget.hpp"
#include <QtEvents>
#include <QTimer>

ControllerPanel::ControllerPanel(EmuApplication *app_)
    : BindingPanel(app_)
{
    setupUi(this);
    QObject::connect(controllerComboBox, &QComboBox::currentIndexChanged, [&](int index) {
        updateBindingView(index);
    });

    // ─── Controller image + binding table ───
    controller_image_ = new SnesControllerWidget(this);
    controller_image_->setObjectName("controllerImage");

    // Fixed (not Expanding) vertically: the mouse view has two extra rows
    // below the table (mouseOptionsLayout, mouseShortcutHintLabel) that the
    // controller view doesn't, so if the image were left free to compete for
    // leftover vertical space with the table, that extra row height would
    // eat into the split differently per view and visibly shift the image
    // and the top of the table between modes. Fixing the image's height
    // makes it (and everything below it) start at the same Y in both views.
    controller_image_->setFixedHeight(260);
    controller_image_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    if (auto *vbox = qobject_cast<QVBoxLayout *>(tableWidget_controller->parentWidget()->layout()))
    {
        int idx = vbox->indexOf(tableWidget_controller);
        vbox->insertWidget(idx, controller_image_);
    }

    connect(controller_image_, &SnesControllerWidget::buttonClicked,
            this, [this](const QString &snes_name) { onImageButtonClicked(snes_name); });

    live_input_timer_.setInterval(50);
    live_input_timer_.setTimerType(Qt::CoarseTimer);
    connect(&live_input_timer_, &QTimer::timeout, this, [this, app_] {
        // The mouse click / Super Scope tables have no hotspots on the image
        // (see SnesControllerWidget::setMode), and their binding arrays
        // aren't laid out like the 12-button controller one, so there's
        // nothing useful to highlight while either is the active view.
        if (BindingPanel::binding == app_->config->binding.mouse_buttons ||
            BindingPanel::binding == app_->config->binding.superscope_buttons)
            return;

        if (awaiting_binding)
        {
            // Currently capturing a new input for cell_row: show exactly the
            // SNES button being assigned, straight from the same row index
            // the binding table is using. This can never drift out of sync
            // with what's actually being configured, unlike deriving it from
            // live device polling.
            // Rows 12-17 are the Turbo variants of A/B/X/Y/L/R (rows 4-9) -
            // map them back to their base button so the image still lights
            // up the button being (turbo-)assigned.
            int base_row = (cell_row < 12) ? cell_row : (cell_row - 12 + 4);
            controller_image_->setPressedNames({ snes_name_for_row(base_row) });
            return;
        }

        if (!app_->input_manager) return;
        QSet<QString> names = app_->input_manager->pressedSnesNames(
            BindingPanel::binding, EmuConfig::allowed_bindings);
        controller_image_->setPressedNames(names);
        controller_image_->setDebugRawState(app_->input_manager->debugRawState());
    });
    live_input_timer_.start();

    BindingPanel::setTableWidget(tableWidget_mouse,
                                 app->config->binding.mouse_buttons,
                                 EmuConfig::allowed_bindings,
                                 EmuConfig::num_mouse_buttons);
    tableWidget_mouse->setVisible(false);
    mouseDeviceLabel->setVisible(false);
    mouseDeviceComboBox->setVisible(false);
    mouseShortcutHintLabel->setVisible(false);

    BindingPanel::setTableWidget(tableWidget_superscope,
                                 app->config->binding.superscope_buttons,
                                 EmuConfig::allowed_bindings,
                                 EmuConfig::num_superscope_buttons);
    tableWidget_superscope->setVisible(false);

    BindingPanel::setTableWidget(tableWidget_controller,
                                 app->config->binding.controller[0].buttons,
                                 EmuConfig::allowed_bindings,
                                 EmuConfig::num_controller_bindings);

    auto action = edit_menu.addAction(QObject::tr("Clear Current Controller"));
    connect(action, &QAction::triggered, [&](bool checked) {
        clearCurrentController();
    });

    action = edit_menu.addAction(QObject::tr("Clear All Controllers"));
    connect(action, &QAction::triggered, [&](bool checked) {
        clearAllControllers();
    });

    editToolButton->setMenu(&edit_menu);
    editToolButton->setPopupMode(QToolButton::InstantPopup);

    QString iconset = app->iconPrefix();
    const char *icons[] = {
        "up", "down", "left", "right", "a", "b", "x", "y", "l", "r", "start", "select", "a", "b", "x", "y", "l", "r"
    };
    for (int i = 0; i < 18; i++)
        tableWidget_controller->verticalHeaderItem(i)->setIcon(QIcon(iconset + icons[i] + ".svg"));

    tableWidget_mouse->verticalHeaderItem(0)->setIcon(QIcon(iconset + "mouseclickl.svg"));
    tableWidget_mouse->verticalHeaderItem(1)->setIcon(QIcon(iconset + "mouseclickr.svg"));

    tableWidget_superscope->verticalHeaderItem(EmuConfig::eSuperscopeFire)->setIcon(QIcon(iconset + "scopefire.svg"));
    tableWidget_superscope->verticalHeaderItem(EmuConfig::eSuperscopePause)->setIcon(QIcon(iconset + "pause.svg"));
    tableWidget_superscope->verticalHeaderItem(EmuConfig::eSuperscopeAutoFire)->setIcon(QIcon(iconset + "scopeautofire.svg"));
    tableWidget_superscope->verticalHeaderItem(EmuConfig::eSuperscopeCursor)->setIcon(QIcon(iconset + "scopecursor.svg"));
    tableWidget_superscope->verticalHeaderItem(EmuConfig::eSuperscopeAimOffscreen)->setIcon(QIcon(iconset + "scopeoffscreen.svg"));

    recreateAutoAssignMenu();
    onJoypadsChanged([&]{ recreateAutoAssignMenu(); });

    connect(portComboBox, &QComboBox::currentIndexChanged, [&](int index) {
        this->app->config->port_configuration = index;
        controllerComboBox->setItemText(0, index == EmuConfig::eMousePlusController ?
                                             QObject::tr("Mouse") : QObject::tr("SNES Controller 1"));
        controllerComboBox->setItemText(1, index == EmuConfig::eSuperScopePlusController ?
                                             QObject::tr("Super Scope") : QObject::tr("SNES Controller 2"));

        // Jump straight to the repurposed slot so switching port mode
        // immediately shows its binding view, instead of leaving "Set" on
        // whatever it was last pointed at.
        if (index == EmuConfig::eMousePlusController)
            controllerComboBox->setCurrentIndex(0);
        else if (index == EmuConfig::eSuperScopePlusController)
            controllerComboBox->setCurrentIndex(1);

        updateBindingView(controllerComboBox->currentIndex());
        app->updateBindings();
    });
}

void ControllerPanel::recreateAutoAssignMenu()
{
    auto_assign_menu.clear();
    auto controller_list = app->input_manager->getXInputControllers();

    for (int i = 0; i < EmuConfig::allowed_bindings; i++)
    {
        auto slot_menu = auto_assign_menu.addMenu(tr("Binding Set #%1").arg(i + 1));
        auto default_keyboard = slot_menu->addAction(tr("Default Keyboard"));
        connect(default_keyboard, &QAction::triggered, [&, slot = i](bool) {
            autoPopulateWithKeyboard(slot);
        });

        for (const auto& c : controller_list)
        {
            auto controller_item = slot_menu->addAction(c.second.c_str());
            connect(controller_item, &QAction::triggered, [&, id = c.first, slot = i](bool) {
                autoPopulateWithJoystick(id, slot);
            });
        }
    }
    autoAssignToolButton->setMenu(&auto_assign_menu);
    autoAssignToolButton->setPopupMode(QToolButton::InstantPopup);
}

void ControllerPanel::autoPopulateWithKeyboard(int slot)
{
    auto &buttons = app->config->binding.controller[controllerComboBox->currentIndex()].buttons;
    const char *button_list[] = { "Up", "Down", "Left", "Right", "d", "c", "s", "x", "z", "a", "Return", "Space" };

    for (int i = 0; i < std::size(button_list); i++)
        buttons[EmuConfig::allowed_bindings * i + slot] = EmuBinding::keyboard(QKeySequence::fromString(button_list[i])[0].key());

    fillTable();
    app->updateBindings();
}

void ControllerPanel::autoPopulateWithJoystick(int joystick_id, int slot)
{
    auto &device = app->input_manager->devices[joystick_id];
    auto sdl_controller = device.gamepad;
    auto &buttons = app->config->binding.controller[controllerComboBox->currentIndex()].buttons;
    const SDL_GamepadButton list[] = { SDL_GAMEPAD_BUTTON_DPAD_UP,
                                       SDL_GAMEPAD_BUTTON_DPAD_DOWN,
                                       SDL_GAMEPAD_BUTTON_DPAD_LEFT,
                                       SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
                                       // B, A and X, Y are inverted on XInput vs SNES
                                       SDL_GAMEPAD_BUTTON_EAST,
                                       SDL_GAMEPAD_BUTTON_SOUTH,
                                       SDL_GAMEPAD_BUTTON_NORTH,
                                       SDL_GAMEPAD_BUTTON_WEST,
                                       SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
                                       SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
                                       SDL_GAMEPAD_BUTTON_START,
                                       SDL_GAMEPAD_BUTTON_BACK };

    auto bindings = SDLInputManager::getXInputButtonBindings(sdl_controller);

    for (auto i = 0; i < std::size(list); i++)
    {
        if (!bindings.contains({ SDL_GAMEPAD_BINDTYPE_BUTTON, list[i] }))
            continue;

        auto &sdl_binding = bindings[{SDL_GAMEPAD_BINDTYPE_BUTTON, list[i]}];
        if (SDL_GAMEPAD_BINDTYPE_BUTTON == sdl_binding.input_type)
            buttons[4 * i + slot] = EmuBinding::joystick_button(device.hw_guid, sdl_binding.input.button);
        else if (SDL_GAMEPAD_BINDTYPE_HAT == sdl_binding.input_type)
            buttons[4 * i + slot] = EmuBinding::joystick_hat(device.hw_guid, sdl_binding.input.hat.hat, sdl_binding.input.hat.hat_mask);
        else if (SDL_GAMEPAD_BINDTYPE_AXIS == sdl_binding.input_type)
            buttons[4 * i + slot] = EmuBinding::joystick_axis(device.hw_guid, sdl_binding.input.axis.axis, sdl_binding.input.axis.axis);
    }

    fillTable();
    app->updateBindings();
}

void ControllerPanel::clearCurrentController()
{
    // Clears whatever is currently shown -- the mouse's click bindings or a
    // real controller's -- using the active binding/table state so this
    // can't accidentally clobber the wrong array.
    for (int i = 0; i < table_width * table_height; i++)
        binding[i] = {};

    // There's no sensible "does nothing" state for the SNES Mouse's clicks,
    // so clearing the mouse view restores the physical-click defaults
    // instead of leaving them unbound. A row's own "-" button still fully
    // unbinds that slot if that's actually what's wanted.
    if (binding == app->config->binding.mouse_buttons)
    {
        binding[0 * table_width] = EmuBinding::mouse_click(1);
        binding[1 * table_width] = EmuBinding::mouse_click(2);
    }

    // Same idea for the Super Scope: only Fire has a sensible "does
    // nothing" fallback (the physical Left click), everything else stays
    // fully unbound until the user assigns it.
    if (binding == app->config->binding.superscope_buttons)
        binding[0 * table_width] = EmuBinding::mouse_click(1);

    fillTable();
    app->updateBindings();
}

void ControllerPanel::clearAllControllers()
{
    for (auto &c : app->config->binding.controller)
        for (auto &b : c.buttons)
            b = {};
    fillTable();
    app->updateBindings();
}

// In Mouse + Controller mode, combo index 0 ("Mouse") is repurposed to show
// the SNES Mouse's click bindings instead of "SNES Controller 1", since Port
// 1 is the mouse in that mode. In SuperScope + Controller mode, Port 2 is the
// Super Scope, so combo index 1 ("SNES Controller 2") is repurposed instead.
// Either way, the real joypad bindings that combo slot normally shows are
// left completely untouched; they're just not reachable from the combo
// while that mode is active, and reappear exactly as they were in every
// other port mode.
void ControllerPanel::updateBindingView(int combo_index)
{
    bool show_mouse = app->config->port_configuration == EmuConfig::eMousePlusController &&
                      combo_index == 0;
    bool show_superscope = app->config->port_configuration == EmuConfig::eSuperScopePlusController &&
                            combo_index == 1;

    tableWidget_controller->setVisible(!show_mouse && !show_superscope);
    tableWidget_mouse->setVisible(show_mouse);
    tableWidget_superscope->setVisible(show_superscope);
    mouseDeviceLabel->setVisible(show_mouse);
    mouseDeviceComboBox->setVisible(show_mouse);
    mouseShortcutHintLabel->setVisible(show_mouse);

    if (show_mouse)
    {
        binding_table_widget = tableWidget_mouse;
        binding = app->config->binding.mouse_buttons;
        table_width = EmuConfig::allowed_bindings;
        table_height = EmuConfig::num_mouse_buttons;
        updateMouseShortcutHint();
    }
    else if (show_superscope)
    {
        binding_table_widget = tableWidget_superscope;
        binding = app->config->binding.superscope_buttons;
        table_width = EmuConfig::allowed_bindings;
        table_height = EmuConfig::num_superscope_buttons;
    }
    else
    {
        binding_table_widget = tableWidget_controller;
        binding = app->config->binding.controller[combo_index].buttons;
        table_width = EmuConfig::allowed_bindings;
        table_height = EmuConfig::num_controller_bindings;
    }

    fillTable();
    awaiting_binding = false;
    controller_image_->setMode(show_mouse ? SnesControllerWidget::Mode::Mouse :
                                show_superscope ? SnesControllerWidget::Mode::Superscope :
                                SnesControllerWidget::Mode::Gamepad);

    // Auto-Assign fills in a full 12-button SNES layout; it doesn't apply to
    // the mouse click or Super Scope tables.
    autoAssignToolButton->setEnabled(!show_mouse && !show_superscope);
}

void ControllerPanel::updateMouseShortcutHint()
{
    auto &b = app->config->binding.shortcuts[EmuConfig::eGrabMouse * EmuConfig::allowed_bindings];
    QString key = b.type == EmuBinding::None ? QObject::tr("(unassigned)") : QString::fromStdString(b.to_string());
    mouseShortcutHintLabel->setText(QObject::tr("Press %1 to toggle Mouse Capture mode").arg(key));
}

QString ControllerPanel::snes_name_for_row(int row)
{
    static const char *names[] = {
        "Up", "Down", "Left", "Right",
        "A", "B", "X", "Y", "L", "R",
        "Start", "Select",
    };
    if (row >= 0 && row < 12) return QString::fromLatin1(names[row]);
    return {};
}

void ControllerPanel::onImageButtonClicked(const QString &snes_name)
{
    int row = -1;
    for (int i = 0; i < 12; i++)
    {
        if (snes_name == snes_name_for_row(i))
        {
            row = i;
            break;
        }
    }
    if (row < 0) return;

    // Trigger the binding capture for slot 0 of that SNES button.
    cell_row = row;
    cell_column = 0;

    auto *item = tableWidget_controller->item(row, 0);
    if (!item)
    {
        item = new QTableWidgetItem();
        tableWidget_controller->setItem(row, 0, item);
    }
    item->setText("...");
    awaiting_binding = true;
    setRedirectInput(true);
    accept_return = false;
    tableWidget_controller->setCurrentCell(row, 0);
}

void ControllerPanel::showEvent(QShowEvent *event)
{
    BindingPanel::showEvent(event);
    recreateAutoAssignMenu();
    portComboBox->setCurrentIndex(app->config->port_configuration);
    controllerComboBox->setItemText(0, app->config->port_configuration == EmuConfig::eMousePlusController ?
                                        QObject::tr("Mouse") : QObject::tr("SNES Controller 1"));
    controllerComboBox->setItemText(1, app->config->port_configuration == EmuConfig::eSuperScopePlusController ?
                                        QObject::tr("Super Scope") : QObject::tr("SNES Controller 2"));

    // Same jump-to-the-repurposed-slot as the port combo handler, in case
    // the dialog was last closed on a different "Set" entry.
    if (app->config->port_configuration == EmuConfig::eMousePlusController)
        controllerComboBox->setCurrentIndex(0);
    else if (app->config->port_configuration == EmuConfig::eSuperScopePlusController)
        controllerComboBox->setCurrentIndex(1);

    updateBindingView(controllerComboBox->currentIndex());
}
