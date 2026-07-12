#pragma once
#include <cstdint>
#include <string>

struct EmuBinding
{
    uint32_t hash() const;
    std::string to_string(bool config = false);
    static EmuBinding joystick_axis(std::string hw_guid, int axis, int threshold);
    static EmuBinding joystick_hat(std::string hw_guid, int hat, uint8_t direction);
    static EmuBinding joystick_button(std::string hw_guid, int button);
    static EmuBinding keyboard(int keycode, bool shift = false, bool alt = false, bool ctrl = false, bool super_mod = false);
    static EmuBinding from_config_string(std::string str);
    std::string to_config_string();
    bool operator==(const EmuBinding &);

    static const uint32_t MOUSE_POINTER = 0x0f000000;
    static const uint32_t MOUSE_BUTTON1 = 0x0f000001;
    static const uint32_t MOUSE_BUTTON2 = 0x0f000002;
    static const uint32_t MOUSE_BUTTON3 = 0x0f000003;

    enum Type
    {
        None = 0,
        Keyboard = 1,
        Joystick = 2
    };
    Type type = None;

    enum JoystickInputType
    {
        Button = 0,
        Axis = 1,
        Hat = 2
    };

    bool alt = false;
    bool ctrl = false;
    bool super_mod = false;
    bool shift = false;
    int keycode = 0;

    JoystickInputType input_type = Button;
    std::string hw_guid;
    int button = 0;
    int axis = 0;
    int hat = 0;
    int threshold = 0;
    uint8_t direction = 0;
};
