#pragma once

#include "SDL3/SDL.h"
#include "EmuBinding.hpp"
#include <QSet>
#include <QString>
#include <map>
#include <set>
#include <vector>
#include <string>
#include <optional>

struct SDLInputDevice
{
    bool open(int joystick_num);

    int slot;
    int sdl_joystick_number;
    bool is_gamepad;
    SDL_Gamepad *gamepad = nullptr;
    SDL_Joystick *joystick = nullptr;
    SDL_JoystickID instance_id = 0;
    std::string hw_guid;
    int num_buttons;
    int num_axes;
    int num_hats;

    struct Axis
    {
        int16_t initial;
        int last;
    };
    std::vector<Axis> axes;

    struct Hat
    {
        uint8_t state;
    };
    std::vector<Hat> hats;
};

struct SDLInputManager
{
    SDLInputManager();
    ~SDLInputManager();

    std::optional<SDL_Event> processEvent();
    std::vector<std::pair<int, std::string>> getXInputControllers();
    void clearEvents();
    void addDevice(int i);
    void removeDevice(int i);
    void printDevices();
    int findFirstOpenIndex();
    int getOrAssignSlot(const std::string &hw_guid);
    static std::map<std::pair<int, int>, SDL_GamepadBinding> getXInputButtonBindings(SDL_Gamepad *gamepad);

    // Returns true while any button on a connected device is currently held.
    // Cheap to poll from a UI repaint timer.
    bool anyButtonPressed() const;
    std::set<int> pressedButtons() const { return pressed_buttons_; }

    // Set of SNES-friendly button names (e.g. {"A", "Up", "Start"}) currently
    // held across all connected gamepads. Resolved through the user's bindings
    // (so the highlight matches what the SNES core will actually receive) and
    // falling back to the default SNES name when the physical button is
    // unbound.
    QSet<QString> pressedSnesNames(const EmuBinding *bindings, int binding_stride) const;

    // Per-device raw SDL state, for diagnostics. Returns a human-readable
    // summary like "btn[1,2,11] hat[0=UP] axis[1=-32768]".
    QString debugRawState() const;

    struct DiscreteAxisEvent
    {
        std::string hw_guid;
        int axis;
        int direction;
        int pressed;
    };
    std::vector<DiscreteAxisEvent> discretizeJoyAxisEvent(SDL_Event &event, int threshold_percent = 33);

    struct DiscreteHatEvent
    {
        std::string hw_guid;
        int hat;
        int direction;
        bool pressed;
    };
    std::vector<DiscreteHatEvent> discretizeHatEvent(SDL_Event &event);

    std::map<SDL_JoystickID, SDLInputDevice> devices;

    std::map<std::string, int> hw_guid_to_slot;

    std::set<int> pressed_buttons_;
};
