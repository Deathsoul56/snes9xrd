#include "EmuBinding.hpp"
#include <QString>
#include <QKeySequence>
#include "SDL3/SDL.h"

uint32_t EmuBinding::hash() const
{
    if (type == Keyboard)
    {
        uint32_t h = (uint32_t)Keyboard << 30;
        h |= (alt    ? 1u : 0) << 29;
        h |= (ctrl   ? 1u : 0) << 28;
        h |= (super_mod ? 1u : 0) << 27;
        h |= (shift  ? 1u : 0) << 26;
        h |= (uint32_t)(keycode & 0x3ffffff);
        return h;
    }

    if (type == Joystick)
    {
        constexpr uint32_t FNV_OFFSET = 2166136261u;
        constexpr uint32_t FNV_PRIME  = 16777619u;
        uint32_t h = FNV_OFFSET;

        auto mix_u32 = [&](uint32_t v) {
            h = (h ^ (v & 0xff)) * FNV_PRIME;
            h = (h ^ ((v >> 8) & 0xff)) * FNV_PRIME;
            h = (h ^ ((v >> 16) & 0xff)) * FNV_PRIME;
            h = (h ^ ((v >> 24) & 0xff)) * FNV_PRIME;
        };

        mix_u32((uint32_t)type);
        mix_u32((uint32_t)input_type);
        for (char c : hw_guid)
            h = (h ^ (uint8_t)c) * FNV_PRIME;
        mix_u32((uint32_t)button);
        mix_u32((uint32_t)axis);
        mix_u32((uint32_t)hat);
        mix_u32((uint32_t)threshold);
        mix_u32((uint32_t)direction);

        return h;
    }

    return 0;
}

bool EmuBinding::operator==(const EmuBinding &other)
{
    return other.hash() == hash();
}

EmuBinding EmuBinding::joystick_axis(std::string hw_guid, int axis, int threshold)
{
    EmuBinding b{};
    b.type = Joystick;
    b.input_type = Axis;
    b.hw_guid = std::move(hw_guid);
    b.axis = axis;
    b.threshold = threshold;
    return b;
}

EmuBinding EmuBinding::joystick_hat(std::string hw_guid, int hat, uint8_t direction)
{
    EmuBinding b{};
    b.type = Joystick;
    b.input_type = Hat;
    b.hw_guid = std::move(hw_guid);
    b.hat = hat;
    b.direction = direction;
    return b;
}

EmuBinding EmuBinding::joystick_button(std::string hw_guid, int button)
{
    EmuBinding b{};
    b.type = Joystick;
    b.input_type = Button;
    b.hw_guid = std::move(hw_guid);
    b.button = button;
    return b;
}

EmuBinding EmuBinding::keyboard(int keycode, bool shift, bool alt, bool ctrl, bool super_mod)
{
    EmuBinding b{};
    b.type = Keyboard;
    b.alt = alt;
    b.ctrl = ctrl;
    b.super_mod = super_mod;
    b.shift = shift;
    b.keycode = keycode;
    return b;
}

static std::string ltrim(std::string s)
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t'))
        s.erase(s.begin());
    return s;
}

EmuBinding EmuBinding::from_config_string(std::string string)
{
    for (auto &c : string)
        if (c >= 'A' && c <= 'Z')
            c += 32;

    if (string.compare(0, 9, "keyboard ") == 0)
    {
        EmuBinding b{};
        b.type = Keyboard;

        QString qstr(string.substr(9).c_str());
        auto seq = QKeySequence::fromString(qstr);
        if (seq.count())
        {
            b.keycode = seq[0].key();
            b.alt = seq[0].keyboardModifiers().testAnyFlag(Qt::AltModifier);
            b.ctrl = seq[0].keyboardModifiers().testAnyFlag(Qt::ControlModifier);
            b.super_mod = seq[0].keyboardModifiers().testAnyFlag(Qt::MetaModifier);
            b.shift = seq[0].keyboardModifiers().testAnyFlag(Qt::ShiftModifier);
        }

        return b;
    }
    else if (string.compare(0, 8, "joystick") == 0)
    {
        std::string rest = ltrim(string.substr(8));

        auto next_word = [&]() -> std::string {
            auto sp = rest.find(' ');
            std::string word = (sp == std::string::npos) ? rest : rest.substr(0, sp);
            if (sp != std::string::npos)
            {
                rest = ltrim(rest.substr(sp + 1));
            }
            else
            {
                rest.clear();
            }
            return word;
        };

        std::string guid_str = next_word();
        std::string kind = next_word();

        auto safe_stoi = [](const std::string &s, int fallback) -> int {
            if (s.empty()) return fallback;
            try { return std::stoi(s); }
            catch (const std::exception &) { return fallback; }
        };

        if (kind == "axis")
        {
            int axis = safe_stoi(next_word(), 0);
            std::string sign_str = next_word();
            int sign = (sign_str == "-") ? -1 : 1;
            return joystick_axis(guid_str, axis, sign);
        }
        else if (kind == "button")
        {
            int button = safe_stoi(next_word(), 0);
            return joystick_button(guid_str, button);
        }
        else if (kind == "hat")
        {
            int hat_index = safe_stoi(next_word(), 0);
            std::string dir_str = next_word();
            uint8_t direction = 0;
            if (dir_str == "up")    direction = SDL_HAT_UP;
            else if (dir_str == "down")  direction = SDL_HAT_DOWN;
            else if (dir_str == "left")  direction = SDL_HAT_LEFT;
            else if (dir_str == "right") direction = SDL_HAT_RIGHT;
            return joystick_hat(guid_str, hat_index, direction);
        }
    }

    return {};
}

std::string EmuBinding::to_config_string()
{
    return to_string(true);
}

std::string EmuBinding::to_string(bool config)
{
    std::string rep;
    if (type == Keyboard)
    {
        if (config)
            rep += "Keyboard ";

        if (ctrl)
            rep += "Ctrl+";
        if (alt)
            rep += "Alt+";
        if (shift)
            rep += "Shift+";
        if (super_mod)
            rep += "Super+";

        QKeySequence seq(keycode);
        rep += seq.toString().toStdString();
    }
    else if (type == Joystick)
    {
        // The raw device GUID is only needed in the config-file format (to
        // disambiguate which physical device a binding belongs to); showing
        // it in the UI just buries the actually useful "Hat 0 Up" /
        // "Button 3" part under a giant, unreadable identifier.
        if (config)
            rep += "Joystick " + hw_guid + " ";

        if (input_type == Button)
        {
            rep += "Button ";
            rep += std::to_string(button);
        }
        if (input_type == Axis)
        {
            // The binding's identity only ever depends on which side of the
            // deadzone was crossed (threshold is always exactly +1 or -1 at
            // runtime, see EmuApplication::pollJoysticks) - there is no real
            // percent to preserve, so don't pretend to serialize one; doing
            // so previously produced a value that didn't survive the
            // save/load round-trip and broke matching after a restart.
            rep += "Axis ";
            rep += std::to_string(axis) + " ";
            rep += (threshold >= 0) ? "+" : "-";
        }
        if (input_type == Hat)
        {
            rep += "Hat ";
            rep += std::to_string(hat) + " ";
            if (direction == SDL_HAT_UP)
                rep += "Up";
            else if (direction == SDL_HAT_DOWN)
                rep += "Down";
            else if (direction == SDL_HAT_LEFT)
                rep += "Left";
            else if (direction == SDL_HAT_RIGHT)
                rep += "Right";
        }
    }
    else
    {
        rep = "None";
    }

    return rep;
}
