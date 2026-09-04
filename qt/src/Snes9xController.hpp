#ifndef __SNES9X_CONTROLLER_HPP
#define __SNES9X_CONTROLLER_HPP
#include <functional>
#include <vector>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

#include "EmuConfig.hpp"
#include "Achievements/AchievementsTypes.hpp"

class AchievementsClient;

class Snes9xController
{
  public:
    static Snes9xController *get();

    void init();
    void deinit();
    void mainLoop();

    // RetroAchievements. See AchievementsClient's class comment for the
    // threading contract these all assume (called only while the emu
    // thread is suspended or is itself the caller).
    void achievementsLoginWithPassword(const std::string &username, const std::string &password);
    void achievementsLoginWithToken(const std::string &username, const std::string &token);
    void achievementsLogout();
    void achievementsUnloadGame();
    bool achievementsLoginPending() const;
    bool achievementsIsLoggedIn() const;
    Achievements::UserInfo achievementsUserInfo() const;
    std::string achievementsLastError() const;
    bool achievementsIsGameLoaded() const;
    Achievements::GameSummary achievementsGameSummary() const;
    std::vector<Achievements::AchievementEntry> achievementsList() const;
    void achievementsIdle();
    bool netplayConnect(const std::string &host, int port);
    bool netplayStartServer(int port);
    void netplayDisconnect();
    bool netplayConnected() const;
    bool netplayIsServer() const;
    void netplayResyncClients();
    void netplaySendRomToClients();
    void netplaySendJoypadSwap();
    void netplaySetSendRomOnConnect(bool enabled);
    void netplaySetSyncByReset(bool enabled);
    void netplaySetMaxFrameLoss(int frames);
    std::string netplayLastError();
    bool netplayPush();
    void netplayPop();
    int netplaySyncSpeed();
    bool openFile(const std::string &filename);
    bool slotUsed(int slot);
    bool loadState(const std::string &filename);
    bool loadState(int slot);
    bool statePreview(int slot, std::vector<uint16_t> &pixels, int &width, int &height);
    void loadUndoState();
    bool saveState(const std::string &filename);
    bool saveState(int slot);
    std::string resumeStatePath();
    bool resumeStateExists();
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
    bool takeScreenshot();
    bool saveSram();
    bool saveMemoryPack();
    bool canSaveMemoryPack() const;
    bool startMovieRecord(const std::string &filename);
    bool openMovie(const std::string &filename);
    void stopMovie();
    bool isMovieActive() const;
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
    bool cheatsEnabled() const;
    void setCheatsEnabled(bool enabled);
    void restoreCheats(const std::vector<std::tuple<bool, std::string, std::string>> &cheats,
               bool enabled);
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
    void resetCheatSearch();
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> searchCheats(int comparison,
                                       int data_size,
                                       int compare_to,
                                       uint32_t value,
                                       bool signed_value);
    std::string getContentFolder();

    std::string getStateFolder();
    std::string getMovieFolder();
    std::string config_folder;
    std::string sram_folder;
    std::string state_folder;
    std::string cheat_folder;
    std::string patch_folder;
    std::string export_folder;
    std::string screenshot_folder;
    std::string bios_folder;
    std::string movie_folder;
    std::string spc_folder;
    std::string satellaview_folder;
    int16_t mouse_x, mouse_y;
    int high_resolution_effect;
    int rewind_buffer_size;
    int rewind_frame_interval;
    bool rewinding = false;
    // Forces g_state_manager.init() to (re)run the next time updateSettings()
    // sees an active ROM, even if rewind_buffer_size hasn't changed -- needed
    // both for the very first ROM of a session and for S9xFreezeSize()
    // potentially differing between ROMs.
    bool rewind_needs_init = true;

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
    bool netplayConnectInternal(const std::string &host, int port);

    std::unique_ptr<AchievementsClient> achievements;
    bool achievements_enabled = true;

    uint32_t netplay_local_joypads[8] = {};
    uint32_t netplay_joypads[8] = {};
    std::string netplay_last_warning;
    // Tracks per-slot Connected state (size matches NP_MAX_CLIENTS in
    // netplay.h) so the host can announce new players without relying on
    // NetPlay.WarningMsg, which gets overwritten by rapid sync chatter
    // before the poll in mainLoop() ever sees the connect message.
    bool netplay_client_was_connected[8] = {};

};

#endif