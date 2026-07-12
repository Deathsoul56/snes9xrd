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
        BindingPanel::binding = this->app->config->binding.controller[index].buttons;
        fillTable();
        awaiting_binding = false;
    });

    // ─── Controller image + binding table ───
    controller_image_ = new SnesControllerWidget(this);
    controller_image_->setObjectName("controllerImage");
    controller_image_->setMinimumHeight(260);

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

    auto swap_menu = edit_menu.addMenu(QObject::tr("Swap With"));
    for (auto i = 0; i < 5; i++)
    {
        action = swap_menu->addAction(QObject::tr("Controller %1").arg(i + 1));
        connect(action, &QAction::triggered, [&, i](bool) {
            auto current_index = controllerComboBox->currentIndex();
            if (current_index == i)
                return;
            swapControllers(i, current_index);
            fillTable();
        });
    }

    editToolButton->setMenu(&edit_menu);
    editToolButton->setPopupMode(QToolButton::InstantPopup);

    QString iconset = app->iconPrefix();
    const char *icons[] = {
        "up", "down", "left", "right", "a", "b", "x", "y", "l", "r", "start", "select", "a", "b", "x", "y", "l", "r"
    };
    for (int i = 0; i < 18; i++)
        tableWidget_controller->verticalHeaderItem(i)->setIcon(QIcon(iconset + icons[i] + ".svg"));

    recreateAutoAssignMenu();
    onJoypadsChanged([&]{ recreateAutoAssignMenu(); });

    connect(automapGamepadsCheckbox, &QCheckBox::toggled, [&](bool checked) {
       this->app->config->automap_gamepads = checked;
        app->updateBindings();
    });

    connect(portComboBox, &QComboBox::currentIndexChanged, [&](int index) {
        this->app->config->port_configuration = index;
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

void ControllerPanel::swapControllers(int first, int second)
{
    auto &a = app->config->binding.controller[first].buttons;
    auto &b = app->config->binding.controller[second].buttons;

    int count = std::size(a);
    for (int i = 0; i < count; i++)
    {
        EmuBinding swap = b[i];
        b[i] = a[i];
        a[i] = swap;
    }

    app->updateBindings();
}

void ControllerPanel::clearCurrentController()
{
    auto &c = app->config->binding.controller[controllerComboBox->currentIndex()];
    for (auto &b : c.buttons)
        b = {};
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
    automapGamepadsCheckbox->setChecked(app->config->automap_gamepads);
}
