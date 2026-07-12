#include "SDLInputManager.hpp"

#include <QString>

#include <algorithm>
#include <optional>

SDLInputManager::SDLInputManager()
{
    SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK);
}

SDLInputManager::~SDLInputManager() = default;

void SDLInputManager::addDevice(int device_index)
{
    SDLInputDevice d;
    if (!d.open(device_index))
        return;

    if (d.hw_guid.empty())
    {
        d.slot = findFirstOpenIndex();
    }
    else
    {
        d.slot = getOrAssignSlot(d.hw_guid);
        if (d.slot < 0)
        {
            d.slot = findFirstOpenIndex();
            if (d.slot >= 0)
                hw_guid_to_slot[d.hw_guid] = d.slot;
        }
    }

    printf("Slot %d [GUID %s]: %s: ", d.slot, d.hw_guid.c_str(), SDL_GetJoystickName(d.joystick));
    printf("%zu axes, %d buttons, %zu hats, %s API\n", d.axes.size(), d.num_buttons, d.hats.size(), d.is_gamepad ? "Controller" : "Joystick");

    devices.insert({ d.instance_id, d });
}

void SDLInputManager::removeDevice(int instance_id)
{
    auto iter = devices.find(instance_id);
    if (iter == devices.end())
        return;

    auto &d = iter->second;

    if (d.is_gamepad)
        SDL_CloseGamepad(d.gamepad);
    else
        SDL_CloseJoystick(d.joystick);

    devices.erase(iter);
}

void SDLInputManager::clearEvents()
{
    std::optional<SDL_Event> event;

    while ((event = processEvent()))
    {
    }
}


std::vector<SDLInputManager::DiscreteHatEvent>
SDLInputManager::discretizeHatEvent(SDL_Event &event)
{
    auto &device = devices.at(event.jhat.which);
    auto &hat = event.jhat.hat;
    auto new_state = event.jhat.value;
    auto &old_state = device.hats[hat].state;

    if (old_state == new_state)
        return {};

    std::vector<DiscreteHatEvent> events;

    for (auto &s : { SDL_HAT_UP, SDL_HAT_DOWN, SDL_HAT_LEFT, SDL_HAT_RIGHT })
        if ((old_state & s) != (new_state & s))
        {
            events.emplace_back(device.hw_guid, hat, s, new_state & s);
        }

    old_state = new_state;
    return events;
}

std::vector<SDLInputManager::DiscreteAxisEvent>
SDLInputManager::discretizeJoyAxisEvent(SDL_Event &event, int threshold_percent)
{
    auto &device = devices.at(event.jaxis.which);
    auto &axis = event.jaxis.axis;
    auto now = event.jaxis.value;
    auto &then = device.axes[axis].last;
    auto center = device.axes[axis].initial;

    auto get_direction = [&](int pos) -> int {
        if (pos > (center + (32767 - center) * threshold_percent / 100))
            return 1;
        if (pos < (center - (center + 32768) * threshold_percent / 100))
            return -1;
        return 0;
    };

    auto previous_direction = get_direction(then);
    auto current_direction  = get_direction(now);

    if (previous_direction == current_direction)
    {
        then = now;
        return {};
    }

    std::vector<DiscreteAxisEvent> events{};

    if (previous_direction == -1 || current_direction == -1)
    {
        events.emplace_back(device.hw_guid, axis, -1, (current_direction == -1));
    }
    if (previous_direction == 1 || current_direction == 1)
    {
        events.emplace_back(device.hw_guid, axis, 1, (current_direction == 1));
    }

    then = now;

    return events;
}

std::optional<SDL_Event> SDLInputManager::processEvent()
{
    SDL_Event event{};

    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        case SDL_EVENT_JOYSTICK_HAT_MOTION:
        case SDL_EVENT_JOYSTICK_BUTTON_UP:
        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        {
            int btn = event.jbutton.button;
            if (event.type == SDL_EVENT_JOYSTICK_BUTTON_DOWN)
                pressed_buttons_.insert(btn);
            else
                pressed_buttons_.erase(btn);
            return event;
        }
        case SDL_EVENT_JOYSTICK_ADDED:
            addDevice(event.jdevice.which);
            return event;
        case SDL_EVENT_JOYSTICK_REMOVED:
            removeDevice(event.jdevice.which);
            return event;
        default:
            break;
        }
    }

    return std::nullopt;
}

void SDLInputManager::printDevices()
{
    for (auto &pair : devices)
    {
        auto &d = pair.second;
        printf("%s: \n", SDL_GetJoystickName(d.joystick));
        printf(" Index: %d\n"
               " Instance ID: %d\n"
               " Controller %s\n"
               " SDL Joystick Number: %d\n",
               d.slot,
               d.instance_id,
               d.is_gamepad ? "yes" : "no",
               d.sdl_joystick_number);
    }
}

int SDLInputManager::findFirstOpenIndex()
{
    for (int i = 0; i < 10000; i++)
    {
        if (std::ranges::none_of(devices, [i](auto &d) -> bool { return d.second.slot == i; }))
        {
            return i;
        }
    }
    return -1;
}

int SDLInputManager::getOrAssignSlot(const std::string &hw_guid)
{
    auto it = hw_guid_to_slot.find(hw_guid);
    if (it == hw_guid_to_slot.end())
        return -1;

    int slot = it->second;
    for (auto &pair : devices)
    {
        if (pair.second.slot == slot)
            return -1;
    }
    return slot;
}

bool SDLInputManager::anyButtonPressed() const
{
    return !pressed_buttons_.empty();
}

QSet<QString> SDLInputManager::pressedSnesNames(const EmuBinding *bindings, int binding_stride) const
{
    // SNES button index → friendly name, matching the order in
    // EmuConfig::controller_t (rows of the binding table).
    static const QString snes_names[12] = {
        QStringLiteral("Up"),    QStringLiteral("Down"),
        QStringLiteral("Left"),  QStringLiteral("Right"),
        QStringLiteral("A"),     QStringLiteral("B"),
        QStringLiteral("X"),     QStringLiteral("Y"),
        QStringLiteral("L"),     QStringLiteral("R"),
        QStringLiteral("Start"), QStringLiteral("Select"),
    };

    QSet<QString> out;

    for (const auto &[instance_id, device] : devices)
    {
        if (!device.joystick) continue;

        // Build the bindings-driven map: raw joystick button index → SNES
        // name. Only filled for buttons the user has explicitly bound. We do
        // NOT add a default fallback — the highlight must reflect the user's
        // configuration exactly, so an unbound physical button stays dark.
        QHash<int, QString> button_to_snes;
        if (bindings && binding_stride > 0)
        {
            for (int row = 0; row < 12; ++row)
            {
                for (int col = 0; col < binding_stride; ++col)
                {
                    const EmuBinding &b = bindings[row * binding_stride + col];
                    if (b.type != EmuBinding::Joystick) continue;
                    if (b.input_type != EmuBinding::Button) continue;
                    if (b.hw_guid != device.hw_guid) continue;
                    button_to_snes.insert(b.button, snes_names[row]);
                }
            }
        }

        // Poll the same raw joystick button indices bindings were captured
        // from (SDL_EVENT_JOYSTICK_BUTTON_DOWN's event.jbutton.button). Using
        // SDL's higher-level SDL_Gamepad virtual button enum here instead
        // would compare against a different numbering scheme than the one
        // button_to_snes is keyed by, causing unrelated buttons to collide
        // and light up whichever SNES button happens to share that virtual
        // enum value.
        for (int raw_button = 0; raw_button < device.num_buttons; ++raw_button)
        {
            if (!SDL_GetJoystickButton(device.joystick, raw_button)) continue;
            if (button_to_snes.contains(raw_button))
                out.insert(button_to_snes.value(raw_button));
        }

        // Some XInput gamepads expose the D-pad as a hat rather than as
        // gamepad buttons 11-14. Mirror the bindings lookup for the hat:
        // if the user bound, say, physical D-pad Up to SNES Y, pressing
        // the physical D-pad Up still highlights Y even though SDL3 reported
        // it via the hat.
        if (device.joystick && bindings && binding_stride > 0)
        {
            QHash<int, QString> hat_to_snes;
            for (int row = 0; row < 12; ++row)
            {
                for (int col = 0; col < binding_stride; ++col)
                {
                    const EmuBinding &b = bindings[row * binding_stride + col];
                    if (b.type != EmuBinding::Joystick) continue;
                    if (b.input_type != EmuBinding::Hat) continue;
                    if (b.hw_guid != device.hw_guid) continue;
                    if (b.hat < 0 || b.hat >= 4) continue;
                    // Map hat 0..3 (SDL_HAT_UP/DOWN/LEFT/RIGHT) to (0,1,2,3) for indexing.
                    int mask = (b.direction == SDL_HAT_UP)    ? 0
                             : (b.direction == SDL_HAT_DOWN)  ? 1
                             : (b.direction == SDL_HAT_LEFT)  ? 2
                             : (b.direction == SDL_HAT_RIGHT) ? 3
                             : -1;
                    if (mask < 0) continue;
                    hat_to_snes.insert(b.hat * 4 + mask, snes_names[row]);
                }
            }

            int num_hats = SDL_GetNumJoystickHats(device.joystick);
            for (int h = 0; h < num_hats; ++h)
            {
                Uint8 value = SDL_GetJoystickHat(device.joystick, h);
                auto fire = [&](Uint8 mask, int dir_idx) {
                    if ((value & mask) == 0) return;
                    int key = h * 4 + dir_idx;
                    if (hat_to_snes.contains(key))
                        out.insert(hat_to_snes.value(key));
                };
                fire(SDL_HAT_UP,    0);
                fire(SDL_HAT_DOWN,  1);
                fire(SDL_HAT_LEFT,  2);
                fire(SDL_HAT_RIGHT, 3);
            }
        }
    }
    return out;
}

bool SDLInputDevice::open(int joystick_num)
{
    sdl_joystick_number = joystick_num;
    is_gamepad = SDL_IsGamepad(joystick_num);

    if (is_gamepad)
    {
        gamepad = SDL_OpenGamepad(joystick_num);
        joystick = SDL_GetGamepadJoystick(gamepad);
    }
    else
    {
        joystick = SDL_OpenJoystick(joystick_num);
        gamepad = nullptr;
    }

    if (!joystick)
        return false;

    num_axes = SDL_GetNumJoystickAxes(joystick);
    axes.resize(num_axes);
    for (int i = 0; i < num_axes; i++)
    {
        SDL_GetJoystickAxisInitialState(joystick, i, &axes[i].initial);
        axes[i].last = axes[i].initial;
    }

    num_buttons = SDL_GetNumJoystickButtons(joystick);
    num_hats = SDL_GetNumJoystickHats(joystick);
    hats.resize(num_hats);
    instance_id = SDL_GetJoystickID(joystick);

    SDL_GUID raw = SDL_GetJoystickGUID(joystick);
    char guid_buf[33];
    SDL_GUIDToString(raw, guid_buf, sizeof(guid_buf));
    hw_guid = guid_buf;

    return true;
}

std::vector<std::pair<int, std::string>> SDLInputManager::getXInputControllers()
{
    std::vector<std::pair<int, std::string>> list;

    for (auto &d : devices)
    {
        if (!d.second.is_gamepad)
            continue;

        list.emplace_back(d.first, SDL_GetJoystickName(d.second.joystick));
    }

    return list;
}

std::map<std::pair<int, int>, SDL_GamepadBinding> SDLInputManager::getXInputButtonBindings(SDL_Gamepad *gamepad)
{
    int num_bindings = 0;
    auto sdl_bindings = SDL_GetGamepadBindings(gamepad, &num_bindings);

    std::map<std::pair<int, int>, SDL_GamepadBinding> binding_map;
    for (int i = 0; i < num_bindings; i++)
        binding_map.insert_or_assign({ sdl_bindings[i]->output_type, sdl_bindings[i]->output.button }, *sdl_bindings[i]);

    SDL_free(sdl_bindings);

    return binding_map;
}

QString SDLInputManager::debugRawState() const
{
    QStringList parts;
    for (const auto &[id, device] : devices)
    {
        QStringList device_parts;
        if (device.gamepad)
        {
            QStringList btn_names;
            for (int i = 0; i <= SDL_GAMEPAD_BUTTON_DPAD_RIGHT; ++i)
            {
                if (SDL_GetGamepadButton(device.gamepad, (SDL_GamepadButton)i))
                    btn_names << QString::number(i);
            }
            if (!btn_names.isEmpty())
                device_parts << QStringLiteral("btn[") + btn_names.join(",") + "]";

            if (device.joystick)
            {
                QStringList hat_parts;
                int num_hats = SDL_GetNumJoystickHats(device.joystick);
                for (int h = 0; h < num_hats; ++h)
                {
                    Uint8 v = SDL_GetJoystickHat(device.joystick, h);
                    const char *name = "?";
                    if      (v == SDL_HAT_UP)    name = "UP";
                    else if (v == SDL_HAT_DOWN)  name = "DOWN";
                    else if (v == SDL_HAT_LEFT)  name = "LEFT";
                    else if (v == SDL_HAT_RIGHT) name = "RIGHT";
                    hat_parts << QStringLiteral("%1=%2").arg(h).arg(QString::fromLatin1(name));
                }
                if (!hat_parts.isEmpty())
                    device_parts << QStringLiteral("hat[") + hat_parts.join(",") + "]";
            }
        }
        if (!device_parts.isEmpty())
            parts << QStringLiteral("dev%1:%2").arg(id).arg(device_parts.join(" "));
    }
    return parts.join("  |  ");
}