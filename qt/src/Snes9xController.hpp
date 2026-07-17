#ifndef __SNES9X_CONTROLLER_HPP
#define __SNES9X_CONTROLLER_HPP
#include <functional>
#include <vector>
#include <cstdint>
#include <mutex>
#include <string>

#include "EmuConfig.hpp"

class Snes9xController
{
  public:
    static Snes9xController *get();

    void init();
    void deinit();
    void mainLoop();
    bool openFile(const std::string &filename);
    bool slotUsed(int slot);
    bool loadState(const std::string &filename);
    bool loadState(int slot);
    void loadUndoState();
    bool saveState(const std::string &filename);
    bool saveState(int slot);
    void updateSettings(const EmuConfig * const config);
    void updateBindings(const EmuConfig * const config);
    void reportBinding(EmuBinding b, bool active);
    void reportMouseButton(int button, bool pressed);
    void reportPointer(int x, int y);
    void reportAbsolutePointer(int x, int y);
    void updateSoundBufferLevel(int, int);
    bool acceptsCommand(const char *command);
    bool isAbnormalSpeed();
    void mute(bool muted);
    void toggleSoundChannel(int channel);
    void setSoundChannelEnabled(int channel, bool enabled);
    void reset();
    void softReset();
    bool loadMultiCart(const std::string &cart_a, const std::string &cart_b);
    bool saveGamePosition();
    bool loadGamePosition();
    bool startMovieRecord(const std::string &filename);
    bool openMovie(const std::string &filename);
    void stopMovie();
    std::string romInfo() const;
    bool dumpSpc();
    void setPaused(bool paused);

    // Briefly halt the emu thread while the GUI mutates core state. These
    // exist as member functions so call sites look natural; the actual
    // blocking goes through a shared mutex below.
    void suspend();
    void resume();
    void setMessage(const std::string &message);
    void clearSoundBuffer();
    std::vector<std::tuple<bool, std::string, std::string>> getCheatList();
    void disableAllCheats();
    void enableCheat(int index);
    void disableCheat(int index);
    bool addCheat(const std::string &description, const std::string &code);
    void deleteCheat(int index);
    void deleteAllCheats();
    int tryImportCheats(const std::string &filename);
    std::string validateCheat(const std::string &code);
    int modifyCheat(int index, const std::string &name,
                    const std::string &code);
    std::string getContentFolder();

    std::string getStateFolder();
    std::string config_folder;
    std::string sram_folder;
    std::string state_folder;
    std::string cheat_folder;
    std::string patch_folder;
    std::string export_folder;
    std::string bios_folder;
    int16_t mouse_x, mouse_y;
    int high_resolution_effect;
    int rewind_buffer_size;
    int rewind_frame_interval;
    bool rewinding = false;

    // Bitmask of enabled SNES APU voices (bit N = channel N, all 8 bits set
    // by default). Mirrors what S9xSetSoundControl was last called with, so
    // the UI can read back the current per-channel mute state.
    int sound_channel_switch = 255;

    std::function<void(uint16_t *, int, int, int, double)> screen_output_function = nullptr;
    std::function<void(int16_t *, int)> sound_output_function = nullptr;

    bool active = false;

  protected:
    Snes9xController();
    ~Snes9xController();

  private:
    void SamplesAvailable();

};

#endif