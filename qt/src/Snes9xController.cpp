#include "Snes9xController.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include "SoftwareScalers.hpp"
#include "fscompat.h"

#include <QMetaObject>
#include <QImage>

#include <QString>

#include <filesystem>
namespace fs = std::filesystem;

#include <cstring>

#include "snes9x.h"
#include "memmap.h"
#include "apu/apu.h"
#include "gfx.h"
#include "snapshot.h"
#include "controls.h"
#include "cheats.h"
#include "movie.h"
#include "display.h"
#include "conffile.h"
#include "statemanager.h"
#include "netplay.h"

#include <chrono>
#include <thread>

extern SNPServer NPServer;

Snes9xController *g_snes9xcontroller = nullptr;
StateManager g_state_manager;

Snes9xController::Snes9xController()
{
    init();
}

Snes9xController::~Snes9xController()
{
    deinit();
}

Snes9xController *Snes9xController::get()
{
    if (!g_snes9xcontroller)
    {
        g_snes9xcontroller = new Snes9xController();
    }

    return g_snes9xcontroller;
}

void Snes9xController::init()
{
    Settings.MouseMaster = true;
    Settings.SuperScopeMaster = true;
    Settings.JustifierMaster = true;
    Settings.MultiPlayer5Master = true;
    Settings.Stereo = true;
    Settings.ReverseStereo = false;
    Settings.SixteenBitSound = true;
    Settings.StopEmulation = true;
    Settings.HDMATimingHack = 100;
    Settings.SkipFrames = 0;
    Settings.TurboSkipFrames = 9;
    Settings.NetPlay = false;

    Settings.UpAndDown = false;
    Settings.InterpolationMethod = DSP_INTERPOLATION_GAUSSIAN;
    Settings.FrameTime = 16639;
    Settings.FrameTimeNTSC = 16639;
    Settings.FrameTimePAL = 20000;
    Settings.DisplayFrameRate = false;
    Settings.DisplayTime = false;
    Settings.DisplayPressedKeys = false;
    Settings.DisplayIndicators = true;
    Settings.SnapshotScreenshots = true;
    Settings.SoundPlaybackRate = 48000;
    Settings.SoundInputRate = 32040;
    Settings.BlockInvalidVRAMAccess = true;
    Settings.SoundSync = false;
    Settings.Mute = false;
    Settings.DynamicRateControl = false;
    Settings.DynamicRateLimit = 5;
    Settings.SuperFXClockMultiplier = 100;
    Settings.MaxSpriteTilesPerLine = 34;
    Settings.OneClockCycle = 6;
    Settings.OneSlowClockCycle = 8;
    Settings.TwoClockCycles = 12;
    Settings.ShowOverscan = false;
    Settings.InitialInfoStringTimeout = 120;

    CPU.Flags = 0;

    rewind_buffer_size = 0;
    rewind_frame_interval = 5;

    Memory.Init();
    S9xInitAPU();
    S9xInitSound(0);
    S9xSetSamplesAvailableCallback([](void *data) {
        ((Snes9xController *)data)->SamplesAvailable();
    }, this);

    S9xGraphicsInit();
    S9xInitInputDevices();
    S9xUnmapAllControls();
    S9xCheatsEnable();

    active = false;
}

void Snes9xController::deinit()
{
    if (active)
        S9xAutoSaveSRAM();
    S9xGraphicsDeinit();
    S9xDeinitAPU();
}

void Snes9xController::updateSettings(const EmuConfig * const config)
{
    setCheatsEnabled(config->apply_cheats);

    NetPlay.MaxBehindFrameCount = config->netplay_max_frame_loss;
    NPServer.SyncByReset = config->netplay_sync_reset;
    NPServer.SendROMImageOnConnect = config->netplay_send_rom;

    if (Settings.AutoSaveDelay != config->sram_save_interval)
    {
        Settings.AutoSaveDelay = config->sram_save_interval;
        CPU.AutoSaveTimer = 0;
    }

    Settings.UpAndDown = config->allow_opposing_dpad_directions;

    Settings.Transparency = config->transparency;

    Settings.InterpolationMethod = config->sound_filter;

    if (config->fixed_frame_rate == 0.0)
    {
        Settings.FrameTimeNTSC = 16639;
        Settings.FrameTimePAL = 20000;
        Settings.FrameTime = Settings.FrameTimeNTSC;
    }
    else
    {
        Settings.FrameTimeNTSC = Settings.FrameTimePAL = Settings.FrameTime =
            1000000 / config->fixed_frame_rate;
    }

    Settings.TurboSkipFrames = config->fast_forward_skip_frames;

    Settings.SkipFrames = config->fixed_frame_skip;

    Settings.DisplayTime = config->show_time;

    if (config->display_messages == EmuConfig::eInscreen)
        Settings.AutoDisplayMessages = true;
    else
        Settings.AutoDisplayMessages = false;

    Settings.DisplayFrameRate = config->show_frame_rate;

    Settings.DisplayPressedKeys = config->show_pressed_keys;

    Settings.DisplayIndicators = config->show_indicators;

    if (Settings.SoundPlaybackRate != config->playback_rate || Settings.SoundInputRate != config->input_rate)
    {
        Settings.SoundInputRate = config->input_rate;
        Settings.SoundPlaybackRate = config->playback_rate;
        S9xUpdateDynamicRate();
    }

    Settings.BlockInvalidVRAMAccess = !config->allow_invalid_vram_access;
    Settings.SeparateEchoBuffer = config->enable_shadow_buffer;

    Settings.SoundSync = config->speed_sync_method == EmuConfig::eSoundSync;

    Settings.Mute = config->mute_audio;

    Settings.DynamicRateControl = config->dynamic_rate_control;

    Settings.DynamicRateLimit = config->dynamic_rate_limit * 1000;

    Settings.SuperFXClockMultiplier  = config->superfx_clock_multiplier;

    if (active && (rewind_needs_init || rewind_buffer_size != config->rewind_buffer_size))
    {
        g_state_manager.init(config->rewind_buffer_size * 1048576);
        rewind_needs_init = false;
    }
    rewind_buffer_size = config->rewind_buffer_size;
    // guard against 0 from stale config files; used as a modulo divisor below
    rewind_frame_interval = config->rewind_frame_interval > 0 ? config->rewind_frame_interval : 1;

    if (config->remove_sprite_limit)
        Settings.MaxSpriteTilesPerLine = 128;
    else
        Settings.MaxSpriteTilesPerLine = 34;

    const int overclock_cycles[4][2] = { { 6, 8 }, { 6, 6 }, { 3, 4 }, { 1, 1 } };
    Settings.OneClockCycle = overclock_cycles[config->overclock][0];
    Settings.OneSlowClockCycle = overclock_cycles[config->overclock][1];
    Settings.TwoClockCycles = overclock_cycles[config->overclock][0] * 2;

    Settings.ShowOverscan = config->show_overscan;

    high_resolution_effect = config->high_resolution_effect;

    config_folder = EmuConfig::findConfigDir();

    auto doFolder = [&](int location, std::string &dest, const std::string &src, const char *subfolder_name)
    {
        if (location == EmuConfig::eROMDirectory)
            dest = "";
        else if (location == EmuConfig::eConfigDirectory)
            dest = config_folder + "/" + subfolder_name;
        else
            dest = src;
    };

    doFolder(config->sram_location, sram_folder, config->sram_folder, "sram");
    doFolder(config->state_location, state_folder, config->state_folder, "state");
    doFolder(config->cheat_location, cheat_folder, config->cheat_folder, "cheat");
    doFolder(config->patch_location, patch_folder, config->patch_folder, "patch");
    doFolder(config->export_location, export_folder, config->export_folder, "export");
    doFolder(config->screenshot_location, screenshot_folder, config->screenshot_folder, "screenshots");
    doFolder(config->bios_location, bios_folder, config->bios_folder, "bios");
    doFolder(config->movie_location, movie_folder, config->movie_folder, "movies");
    doFolder(config->spc_location, spc_folder, config->spc_folder, "spc");
    doFolder(config->satellaview_location, satellaview_folder, config->satellaview_folder, "satellaview");
}

bool Snes9xController::openFile(const std::string &filename)
{
    if (active)
        S9xAutoSaveSRAM();
    active = false;
    rewind_needs_init = true;
    auto result = Memory.LoadROM(filename.c_str());
    if (result)
    {
        active = true;
        Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
    }

    // server.cpp's netplay heartbeat pacer refuses to send heartbeats while
    // this is true (see S9xNPServerLoop); GTK/win32 clear it on successful
    // ROM load, Qt never did, so hosting could never send a single
    // heartbeat and the client would freeze immediately, forever.
    Settings.StopEmulation = !active;

    return active;
}

void Snes9xController::suspend() { /* placeholder; EmuApplication gates the emu thread */ }
void Snes9xController::resume()  { /* placeholder */ }

void Snes9xController::mainLoop()
{
    if (!active)
        return;

    if (Settings.ForcedPause)
        return;

    // The netplay server/client threads report connects, disconnects and
    // sync warnings by writing NetPlay.WarningMsg (see S9xNPSetWarning in
    // netplay.cpp/server.cpp) -- Qt has no WM_USER-style hook to catch that
    // like the legacy win32 GUI does, so poll for changes here instead.
    // "has paused."/"has resumed." are routine per-frame flow-control
    // chatter from the lockstep pacing (see S9xNPClientSyncSpeed), not
    // connect/disconnect events, and can repeat many times a second -- skip
    // those so the HUD only shows actionable connect/disconnect messages.
    if ((Settings.NetPlay || Settings.NetPlayServer) && NetPlay.WarningMsg[0] &&
        netplay_last_warning != NetPlay.WarningMsg &&
        !strstr(NetPlay.WarningMsg, "has paused") &&
        !strstr(NetPlay.WarningMsg, "has resumed"))
    {
        netplay_last_warning = NetPlay.WarningMsg;
        S9xSetInfoString(NetPlay.WarningMsg);
    }

    // NPServer.Clients[].Connected is a stable flag (unlike NetPlay.WarningMsg,
    // which the rapid post-connect resync chatter overwrites before this poll
    // ever runs), so use it directly to announce new players to the host.
    if (Settings.NetPlayServer)
    {
        for (int i = 0; i < 8; i++)
        {
            if (NPServer.Clients[i].Connected && !netplay_client_was_connected[i])
            {
                if (NPServer.Clients[i].HostName)
                {
                    std::string msg = "Player " + std::to_string(i + 1) + " on " +
                                       NPServer.Clients[i].HostName + " has connected.";
                    S9xSetInfoString(msg.c_str());
                    netplay_client_was_connected[i] = true;

                    // "Sync By Reset" alone isn't reliable enough to keep a
                    // newly-joined client from silently drifting out of sync
                    // (see S9xNPAcceptClient's NP_SERVER_RESET_ALL path in
                    // server.cpp) -- always follow up with a full savestate
                    // resync too, regardless of that setting.
                    if (NPServer.NumClients > 1)
                        S9xNPServerQueueSyncAll();
                }
            }
            else
            {
                netplay_client_was_connected[i] = NPServer.Clients[i].Connected;
            }
        }
    }

    if (netplayPush())
        return;

    // Rewinding pops local save-state history without telling the other
    // peers, which would desync them immediately -- block it while netplay
    // is connected, same as fast-forward is already blocked above.
    if (rewind_buffer_size > 0 && !Settings.NetPlay)
    {
        if (rewinding)
        {
            uint16 joypads[8];
            for (int i = 0; i < 8; i++)
                joypads[i] = MovieGetJoypad(i);

            rewinding = g_state_manager.pop();

            for (int i = 0; i < 8; i++)
                MovieSetJoypad(i, joypads[i]);
        }
        else if (IPPU.TotalEmulatedFrames % rewind_frame_interval == 0)
            g_state_manager.push();

        if (rewinding)
            Settings.Mute |= 0x80;
        else
            Settings.Mute &= ~0x80;
    }

    S9xMainLoop();

    netplayPop();
}

void Snes9xController::setPaused(bool paused)
{
    Settings.Paused = paused;
}

void Snes9xController::updateSoundBufferLevel(int empty, int total)
{
    S9xUpdateDynamicRate(empty, total);
}

bool8 S9xDeinitUpdate(int width, int height)
{
    static int last_height = 0;
    int yoffset = 0;

    auto &display = Snes9xController::get()->screen_output_function;
    if (display == nullptr)
        return true;

    if (width < 256 || height < 224)
        return false;

    if (last_height > height)
        memset(GFX.Screen + GFX.RealPPL * height, 0, GFX.Pitch * (last_height - height));

    last_height = height;

    if (Settings.ShowOverscan)
    {
        if (height == SNES_HEIGHT)
        {
            yoffset = -8;
            height = SNES_HEIGHT_EXTENDED;
        }
        if (height == SNES_HEIGHT * 2)
        {
            yoffset = -16;
            height = SNES_HEIGHT_EXTENDED * 2;
        }
    }
    else
    {
        if (height == SNES_HEIGHT_EXTENDED)
        {
            yoffset = 7;
            height = SNES_HEIGHT;
        }
        if (height == SNES_HEIGHT_EXTENDED * 2)
        {
            yoffset = 14;
            height = SNES_HEIGHT * 2;
        }
    }

    uint16_t *screen_view = GFX.Screen + (yoffset * (int)GFX.RealPPL);

    auto hires_effect = Snes9xController::get()->high_resolution_effect;
    if (!Settings.Paused)
    {
        if (hires_effect == EmuConfig::eScaleUp)
        {
            S9xForceHires(screen_view, GFX.Pitch, width, height);
        }
        else if (hires_effect == EmuConfig::eScaleDown)
        {
            S9xMergeHires(screen_view, GFX.Pitch, width, height);
        }
    }

    display(screen_view, width, height, GFX.Pitch, Settings.PAL ? 50.0 : 60.098813);

    return true;
}

bool8 S9xContinueUpdate(int width, int height)
{
    return S9xDeinitUpdate(width, height);
}

void S9xSyncSpeed()
{
    if (Snes9xController::get()->netplaySyncSpeed())
        return;

    if (Settings.TurboMode)
    {
        IPPU.FrameSkip++;
        if ((IPPU.FrameSkip > Settings.TurboSkipFrames) && !Settings.HighSpeedSeek)
        {
            IPPU.FrameSkip = 0;
            IPPU.SkippedFrames = 0;
            IPPU.RenderThisFrame = true;
        }
        else
        {
            IPPU.SkippedFrames++;
            IPPU.RenderThisFrame = false;
        }

        return;
    }

    // Settings.SkipFrames doubles as the "Fixed, always skip N frames"
    // option from the display settings (0 = disabled). This is the same
    // core field unix/win32/macOS use for the equivalent feature; the
    // "Automatic" mode lives separately in EmuConfig::speed_sync_method /
    // EmuCanvas's timer-based throttle, which measures real elapsed time
    // instead of counting frames.
    if (Settings.SkipFrames > 0)
    {
        IPPU.FrameSkip++;
        if (IPPU.FrameSkip > Settings.SkipFrames)
        {
            IPPU.FrameSkip = 0;
            IPPU.SkippedFrames = 0;
            IPPU.RenderThisFrame = true;
        }
        else
        {
            IPPU.SkippedFrames++;
            IPPU.RenderThisFrame = false;
        }

        return;
    }

    IPPU.RenderThisFrame = true;
}

void S9xParsePortConfig(ConfigFile&, int)
{
}

std::string S9xGetDirectory(s9x_getdirtype dirtype)
{
    std::string dirname;
    auto c = Snes9xController::get();

    switch (dirtype)
    {
    case HOME_DIR:
        dirname = c->config_folder;
        break;

    case SNAPSHOT_DIR:
        dirname = c->state_folder;
        break;

    case PATCH_DIR:
        dirname = c->patch_folder;
        break;

    case CHEAT_DIR:
        dirname = c->cheat_folder;
        break;

    case SRAM_DIR:
        dirname = c->sram_folder;
        break;

    case SPC_DIR:
        dirname = c->spc_folder;
        break;

    case SCREENSHOT_DIR:
        dirname = c->screenshot_folder;
        break;

    case BIOS_DIR:
        dirname = c->bios_folder;
        break;

    case SAT_DIR:
        dirname = c->satellaview_folder;
        break;

    default:
        dirname = "";
    }

    /* Check if directory exists, make it and/or set correct permissions */
    if (dirtype != HOME_DIR && !dirname.empty())
    {
        fs::path path(dirname);

        if (!fs::exists(path))
        {
            fs::create_directories(path);
        }
        else if ((fs::status(path).permissions() & fs::perms::owner_write) == fs::perms::none)
        {
            fs::permissions(path, fs::perms::owner_write, fs::perm_options::add);
        }
    }

    /* Anything else, use ROM filename path */
    if (dirname.empty() && !Memory.ROMFilename.empty())
    {
        fs::path path(Memory.ROMFilename);

        path.remove_filename();

        if (!fs::is_directory(path))
            dirname = fs::current_path().string();
        else
            dirname = path.string();
    }

    return dirname;
}

void S9xInitInputDevices()
{
}

void S9xHandlePortCommand(s9xcommand_t, short, short)
{
}

bool S9xPollButton(unsigned int, bool *)
{
    return false;
}

void S9xToggleSoundChannel(int c)
{
    Snes9xController::get()->toggleSoundChannel(c);
}

std::string S9xGetFilenameInc(std::string e, enum s9x_getdirtype dirtype)
{
    fs::path rom_filename(Memory.ROMFilename);

    fs::path filename_base(S9xGetDirectory(dirtype));
    filename_base /= rom_filename.filename();

    fs::path new_filename;

    if (e[0] != '.')
        e = "." + e;
    int i = 0;
    do
    {
        std::string new_extension = std::to_string(i);
        while (new_extension.length() < 3)
            new_extension = "0" + new_extension;
        new_extension += e;

        new_filename = filename_base;
        new_filename.replace_extension(new_extension);

        i++;
    } while (fs::exists(new_filename));

    return new_filename.string();
}

bool8 S9xInitUpdate()
{
    return true;
}

void S9xExtraUsage()
{
}

bool8 S9xOpenSoundDevice()
{
    return true;
}

bool S9xPollAxis(unsigned int axis, short *value)
{
    return true;
}

void S9xParseArg(char *argv[], int &index, int argc)
{
}

void S9xExit()
{
}

bool S9xPollPointer(unsigned int, short *, short *)
{
    return false;
}

void Snes9xController::SamplesAvailable()
{
    static std::vector<int16_t> data;
    if (sound_output_function)
    {
        int samples = S9xGetSampleCount();
        if (data.size() < samples)
            data.resize(samples);
        S9xMixSamples((uint8_t *)data.data(), samples);
        sound_output_function(data.data(), samples);
    }
    else
    {
        S9xClearSamples();
    }
}

void Snes9xController::clearSoundBuffer()
{
    S9xClearSamples();
}

void S9xMessage(int message_class, int type, const char *message)
{
    if (type == S9X_ROM_INFO)
        S9xSetInfoString(Memory.GetMultilineROMInfo().c_str());

    fprintf(stderr, "[snes9x] %s\n", message);
    fflush(stderr);

    // Missing freeze file (quick-load slot not saved yet) is a routine,
    // expected condition, not a failure worth interrupting the user with a
    // modal dialog -- show it the same way a successful load/save is shown.
    if (type == S9X_FREEZE_FILE_NOT_FOUND || type == S9X_SCREENSHOT_INFO)
    {
        S9xSetInfoString(message);
        return;
    }

    // Surface the message to the user via the message bus. Errors become modal
    // dialogs so a "multicart failed because BIOS missing" is visible, not
    // silently logged to stdout.
    if (auto *app = EmuApplication::get_unwrapped())
    {
        if (auto *win = app->window.get())
        {
            if (message_class == S9X_ERROR)
                QMetaObject::invokeMethod(win, "showCoreError", Qt::QueuedConnection,
                                          Q_ARG(QString, QString::fromUtf8(message)));
        }
    }
}

const char *S9xStringInput(const char *prompt)
{
    return "";
}

bool8 S9xOpenSnapshotFile(const char *filename, bool8 read_only, STREAM *file)
{
    if (read_only)
    {
        if ((*file = OPEN_STREAM(filename, "rb")))
            return (true);
        else
            fprintf(stderr, "Failed to open file stream for reading.\n");
    }
    else
    {
        if ((*file = OPEN_STREAM(filename, "wb")))
        {
            return (true);
        }
        else
        {
            fprintf(stderr, "Couldn't open stream with zlib.\n");
        }
    }

    fprintf(stderr, "Couldn't open snapshot file:\n%s\n", filename);

    return false;
}

void S9xCloseSnapshotFile(STREAM file)
{
    CLOSE_STREAM(file);
}

void S9xAutoSaveSRAM()
{
    printf("%s\n", S9xGetFilename(".srm", SRAM_DIR).c_str());
    Memory.SaveSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
    S9xSaveCheatFile(S9xGetFilename(".cht", CHEAT_DIR));
}

bool Snes9xController::acceptsCommand(const char *command)
{
    auto cmd = S9xGetCommandT(command);
    return !(cmd.type == S9xNoMapping || cmd.type == S9xBadMapping);
}

void Snes9xController::toggleSoundChannel(int c)
{
    if (c == 8)
        sound_channel_switch = 255;
    else
        sound_channel_switch ^= 1 << c;

    S9xSetSoundControl(sound_channel_switch);
}

void Snes9xController::setSoundChannelEnabled(int channel, bool enabled)
{
    bool currently_enabled = (sound_channel_switch >> channel) & 1;
    if (currently_enabled != enabled)
        toggleSoundChannel(channel);
}

void Snes9xController::updateBindings(const EmuConfig *const config)
{
    const char *snes9x_names[] = {
        "Up",
        "Down",
        "Left",
        "Right",
        "A",
        "B",
        "X",
        "Y",
        "L",
        "R",
        "Start",
        "Select",
        "Turbo A",
        "Turbo B",
        "Turbo X",
        "Turbo Y",
        "Turbo L",
        "Turbo R",
    };

    S9xUnmapAllControls();

    switch (config->port_configuration)
    {
    case EmuConfig::eTwoControllers:
        S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
        S9xSetController(1, CTL_JOYPAD, 1, 1, 1, 1);
        break;
    case EmuConfig::eMousePlusController:
        S9xSetController(0, CTL_MOUSE, 0, 0, 0, 0);
        S9xSetController(1, CTL_JOYPAD, 0, 0, 0, 0);
        break;
    case EmuConfig::eSuperScopePlusController:
        // The real Super Scope hardware only works when plugged into port 2
        // (index 1) -- games poll that port specifically, so this can't be
        // swapped like Mouse can. See unix.cpp/wsnes9x.cpp's equivalent presets.
        S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
        S9xSetController(1, CTL_SUPERSCOPE, 0, 0, 0, 0);
        break;
    case EmuConfig::eControllerPlusMultitap:
        S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
        S9xSetController(1, CTL_MP5, 1, 2, 3, 4);
        break;
    default:
        S9xSetController(0, CTL_JOYPAD, 0, 0, 0, 0);
        S9xSetController(1, CTL_NONE, 0, 0, 0, 0);
    }

    for (int controller_number = 0; controller_number < 5; controller_number++)
    {
        auto &controller = config->binding.controller[controller_number];
        for (int i = 0; i < EmuConfig::num_controller_bindings; i++)
        {
            for (int b = 0; b < EmuConfig::allowed_bindings; b++)
            {
                auto binding = controller.buttons[i * EmuConfig::allowed_bindings + b];
                if (binding.hash() == 0)
                    continue;
                std::string name = "Joypad" +
                                    std::to_string(controller_number + 1) + " " +
                                    snes9x_names[i];

                auto cmd = S9xGetCommandT(name.c_str());
                S9xMapButton(binding.hash(), cmd, false);
            }
        }
    }

    // Click L/R bindings -- this is the ONLY place Mouse1 L/R get mapped, so
    // the SNES Mouse's clicks are entirely config-driven (defaulted to the
    // physical mouse's own clicks via EmuBinding::mouse_click(), but the user
    // can rebind or clear them like any other binding).
    {
        const char *mouse_button_names[] = { "Mouse1 L", "Mouse1 R" };
        for (int i = 0; i < EmuConfig::num_mouse_buttons; i++)
        {
            for (int b = 0; b < EmuConfig::allowed_bindings; b++)
            {
                auto binding = config->binding.mouse_buttons[i * EmuConfig::allowed_bindings + b];
                if (binding.hash() == 0)
                    continue;

                auto cmd = S9xGetCommandT(mouse_button_names[i]);
                S9xMapButton(binding.hash(), cmd, false);
            }
        }
    }

    // Super Scope button bindings -- entirely config-driven like Click L/R
    // above (only Fire defaults to a physical click; everything else is
    // unbound until the user assigns it). Auto Fire is a real core command
    // ("Superscope AutoFire"): the periodic Fire pulsing happens in
    // S9xControlEOF(), frame-synced exactly like joypad Turbo A.
    {
        const char *superscope_button_names[] = {
            "Superscope Fire", "Superscope Pause", "Superscope AutoFire",
            "Superscope Cursor", "Superscope AimOffscreen"
        };
        for (int i = 0; i < EmuConfig::num_superscope_buttons; i++)
        {
            for (int b = 0; b < EmuConfig::allowed_bindings; b++)
            {
                auto binding = config->binding.superscope_buttons[i * EmuConfig::allowed_bindings + b];
                if (binding.hash() == 0)
                    continue;

                auto cmd = S9xGetCommandT(superscope_button_names[i]);
                S9xMapButton(binding.hash(), cmd, false);
            }
        }
    }

    for (int i = 0; i < EmuConfig::num_shortcuts; i++)
    {
        auto command = S9xGetCommandT(EmuConfig::getShortcutNames()[i]);
        if (command.type == S9xNoMapping)
            continue;

        for (int b = 0; b < 4; b++)
        {
            auto binding = config->binding.shortcuts[i * 4 + b];
            if (binding.type != 0)
                S9xMapButton(binding.hash(), command, false);
        }
    }

    auto cmd = S9xGetCommandT("Pointer Mouse1+Superscope+Justifier1");
    S9xMapPointer(EmuBinding::MOUSE_POINTER, cmd, false);
    mouse_x = mouse_y = 0;
    S9xReportPointer(EmuBinding::MOUSE_POINTER, mouse_x, mouse_y);

    // Mouse1 L/R deliberately isn't mapped here: whether the physical mouse's
    // own clicks drive the SNES Mouse is entirely up to the Click L/R
    // bindings above (defaulted to the physical clicks, but user-removable).
    // Same goes for the Super Scope's buttons above -- Justifier has no
    // binding UI of its own, so it stays hardwired to the physical clicks.
    cmd = S9xGetCommandT("Justifier1 Trigger");
    S9xMapButton(EmuBinding::MOUSE_BUTTON1, cmd, false);

    cmd = S9xGetCommandT("Justifier1 AimOffscreen Trigger");
    S9xMapButton(EmuBinding::MOUSE_BUTTON3, cmd, false);

    cmd = S9xGetCommandT("Justifier1 Start");
    S9xMapButton(EmuBinding::MOUSE_BUTTON2, cmd, false);

}

void Snes9xController::reportBinding(EmuBinding b, bool active)
{
    S9xReportButton(b.hash(), active);
}

void Snes9xController::reportMouseButton(int button, bool pressed)
{
    S9xReportButton(EmuBinding::MOUSE_POINTER + button, pressed);
}

void Snes9xController::reportPointer(int x, int y)
{
    mouse_x += x;
    mouse_y += y;
    S9xReportPointer(EmuBinding::MOUSE_POINTER, mouse_x, mouse_y);
}

// Unlike the SNES Mouse (reportPointer above), lightgun-style devices such as
// the Superscope read an absolute aim position rather than an accumulated
// relative delta (see controls.cpp: superscope.x/y are assigned directly from
// the reported coordinates, not diffed against a previous value). x and y are
// expected to already be in SNES screen space (0..255, 0..PPU.ScreenHeight-1).
void Snes9xController::reportAbsolutePointer(int x, int y)
{
    mouse_x = x;
    mouse_y = y;
    S9xReportPointer(EmuBinding::MOUSE_POINTER, mouse_x, mouse_y);
}

static fs::path save_slot_path(int slot)
{
    std::string extension = std::to_string(slot);
    while (extension.length() < 3)
        extension = "0" + extension;
    fs::path path(S9xGetDirectory(SNAPSHOT_DIR));
    path /= fs::path(Memory.ROMFilename).filename();
    path.replace_extension(extension);
    return path;
}

void Snes9xController::loadUndoState()
{
    S9xUnfreezeGame(S9xGetFilename(".undo", SNAPSHOT_DIR).c_str());
}

std::string Snes9xController::getStateFolder()
{
    return S9xGetDirectory(SNAPSHOT_DIR);
}

std::string Snes9xController::getMovieFolder()
{
    if (!movie_folder.empty())
        return movie_folder;
    return getContentFolder();
}

bool Snes9xController::slotUsed(int slot)
{
    return fs::exists(save_slot_path(slot));
}

std::string Snes9xController::resumeStatePath()
{
    return S9xGetFilename(".resume", SNAPSHOT_DIR);
}

bool Snes9xController::resumeStateExists()
{
    return fs::exists(resumeStatePath());
}

bool Snes9xController::loadState(int slot)
{
    return loadState(save_slot_path(slot).string());
}

bool Snes9xController::statePreview(int slot, std::vector<uint16_t> &pixels, int &width, int &height)
{
    auto path = save_slot_path(slot);
    if (!fs::exists(path))
        return false;

    uint16 *image = nullptr;
    STREAM stream = nullptr;
    if (!S9xOpenSnapshotFile(path.string().c_str(), true, &stream))
        return false;

    int result = S9xUnfreezeScreenshotFromStream(stream, &image, width, height);
    S9xCloseSnapshotFile(stream);
    if (result != SUCCESS || !image)
        return false;

    pixels.assign(image, image + width * height);
    free(image);
    return true;
}

bool Snes9xController::loadState(const std::string &filename)
{
    if (!active)
        return false;

    S9xFreezeGame(S9xGetFilename(".undo", SNAPSHOT_DIR).c_str());

    if (S9xUnfreezeGame(filename.c_str()))
    {
        auto info_string = filename + " loaded";
        S9xSetInfoString(info_string.c_str());
        return true;
    }
    else
    {
        fprintf(stderr, "Failed to load state file: %s\n", filename.c_str());
        auto info_string = "Failed to load " + filename;
        S9xSetInfoString(info_string.c_str());
        return false;
    }
}

bool Snes9xController::saveState(const std::string &filename)
{
    if (!active)
        return false;

    if (S9xFreezeGame(filename.c_str()))
    {
        auto info_string = filename + " saved";
        S9xSetInfoString(info_string.c_str());
        return true;
    }
    else
    {
        fprintf(stderr, "Couldn't save state file: %s\n", filename.c_str());
        auto info_string = "Failed to save " + filename;
        S9xSetInfoString(info_string.c_str());
        return false;
    }
}

void Snes9xController::mute(bool muted)
{
    Settings.Mute = muted;
}

bool Snes9xController::isAbnormalSpeed()
{
    return (Settings.TurboMode || rewinding);
}

void Snes9xController::reset()
{
    S9xReset();
}

void Snes9xController::softReset()
{
    S9xSoftReset();
}

bool Snes9xController::saveState(int slot)
{
    return saveState(save_slot_path(slot).string());
}

void Snes9xController::setMessage(const std::string &message)
{
    S9xSetInfoString(message.c_str());
}

std::vector<std::tuple<bool, std::string, std::string>> Snes9xController::getCheatList()
{
    std::vector<std::tuple<bool, std::string, std::string>> cheat_list;

    cheat_list.reserve(Cheat.group.size());

    for (auto &c : Cheat.group)
        cheat_list.emplace_back(c.enabled, c.name, S9xCheatGroupToText(c));

    return std::move(cheat_list);
}

bool Snes9xController::cheatsEnabled() const
{
    return Settings.ApplyCheats;
}

void Snes9xController::setCheatsEnabled(bool enabled)
{
    Settings.ApplyCheats = enabled;
    if (enabled)
        S9xCheatsEnable();
    else
        S9xCheatsDisable();
}

void Snes9xController::restoreCheats(const std::vector<std::tuple<bool, std::string, std::string>> &cheats,
                                     bool enabled)
{
    S9xDeleteCheats();
    for (const auto &[cheat_enabled, name, code] : cheats)
    {
        auto index = S9xAddCheatGroup(name, code);
        if (index >= 0 && cheat_enabled)
            S9xEnableCheatGroup(index);
    }
    setCheatsEnabled(enabled);
}

void Snes9xController::disableAllCheats()
{
    for (size_t i = 0; i < Cheat.group.size(); i++)
    {
        S9xDisableCheatGroup(i);
    }
}

void Snes9xController::enableCheat(int index)
{
    S9xEnableCheatGroup(index);
}

void Snes9xController::disableCheat(int index)
{
    S9xDisableCheatGroup(index);
}

void Snes9xController::resetCheatSearch()
{
    S9xStartCheatSearch(&Cheat);
}

std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> Snes9xController::searchCheats(int comparison,
                                                                                        int data_size,
                                                                                        int compare_to,
                                                                                        uint32_t value,
                                                                                        bool signed_value)
{
    auto size = static_cast<S9xCheatDataSize>(data_size);

    if (comparison >= 0)
    {
        auto comp = static_cast<S9xCheatComparisonType>(comparison);
        if (compare_to == 1)
            S9xSearchForValue(&Cheat, comp, size, value, signed_value, FALSE);
        else if (compare_to == 2)
            S9xSearchForAddress(&Cheat, comp, size, value, FALSE);
        else
            S9xSearchForChange(&Cheat, comp, size, signed_value, FALSE);
    }

    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> results;
    int width = data_size + 1;
    results.reserve(0x1000);
    for (uint32_t address = 0; address < 0x32000U - width; address++)
    {
        if (!(Cheat.ALL_BITS[address >> 5] & (1U << (address & 31))))
            continue;

        const uint8_t *current;
        const uint8_t *previous;
        uint32_t offset;
        uint32_t display_address;
        if (address < 0x20000)
        {
            current = Cheat.RAM;
            previous = Cheat.CWRAM;
            offset = address;
            display_address = 0x7e0000 + address;
        }
        else if (address < 0x30000)
        {
            current = Cheat.SRAM;
            previous = Cheat.CSRAM;
            offset = address - 0x20000;
            display_address = 0x7e0000 + address;
        }
        else
        {
            current = Cheat.FillRAM + 0x3000;
            previous = Cheat.CIRAM;
            offset = address - 0x30000;
            display_address = 0x7e0000 + address;
        }

        uint32_t current_value = 0;
        uint32_t previous_value = 0;
        for (int byte = 0; byte < width; byte++)
        {
            current_value |= static_cast<uint32_t>(current[offset + byte]) << (byte * 8);
            previous_value |= static_cast<uint32_t>(previous[offset + byte]) << (byte * 8);
        }
        results.emplace_back(display_address, current_value, previous_value);
    }

    memcpy(Cheat.CWRAM, Cheat.RAM, 0x20000);
    memcpy(Cheat.CSRAM, Cheat.SRAM, 0x10000);
    memcpy(Cheat.CIRAM, Cheat.FillRAM, 0x2000);
    return results;
}

bool Snes9xController::addCheat(const std::string &description,
                                const std::string &code)
{
    return S9xAddCheatGroup(description, code) >= 0;
}

void Snes9xController::deleteCheat(int index)
{
    S9xDeleteCheatGroup(index);
}

void Snes9xController::deleteAllCheats()
{
    S9xDeleteCheats();
}

int Snes9xController::tryImportCheats(const std::string &filename)
{
    return S9xImportCheatsFromDatabase(filename);
}

std::string Snes9xController::validateCheat(const std::string &code)
{
    return S9xCheatValidate(code);
}

int Snes9xController::modifyCheat(int index, const std::string &name,
                                  const std::string &code)
{
    return S9xModifyCheatGroup(index, name, code);
}

bool Snes9xController::loadMultiCart(const std::string &cart_a, const std::string &cart_b)
{
    // Mirror openFile()'s pattern: autosave the outgoing game if one is
    // running, then attempt the load unconditionally. The previous code
    // bailed out with `if (!active) return false;`, which meant the very
    // first MultiCart load of a session (the common case) always failed
    // before Memory.LoadMultiCart was ever called.
    if (active)
        S9xAutoSaveSRAM();
    active = false;
    rewind_needs_init = true;

    bool ok = Memory.LoadMultiCart(cart_a.c_str(), cart_b.c_str());
    if (ok)
    {
        active = true;
        Memory.LoadSRAM(S9xGetFilename(".srm", SRAM_DIR).c_str());
    }
    else
    {
        // The core silently returns false on most failure paths. Diagnose
        // here so the user can act: print a specific reason to stderr AND
        // push a typed error string to the GUI via the S9xMessage bus so
        // the modal dialog tells them which case they're hitting.
        auto fail = [&](const char *msg) {
            std::fprintf(stderr, "[multicart] %s\n", msg);
            S9xMessage(S9X_ERROR, 0, msg);
        };

        std::string bios_dir = S9xGetDirectory(BIOS_DIR);
        std::string stbios_path = bios_dir + SLASH_STR + "STBIOS.bin";
        FILE *test = std::fopen(stbios_path.c_str(), "rb");
        if (!test)
        {
            fail(("Could not find STBIOS.bin for Sufami Turbo. "
                  "Set the BIOS folder in Settings → Files → BIOS to the directory that contains STBIOS.bin "
                  "(current lookup: " + stbios_path + ")").c_str());
        }
        else
        {
            std::fclose(test);
            fail("Memory.LoadMultiCart returned false (cart detection failed — "
                 "check the ROM files in Slot A and Slot B are valid and that the "
                 "BS-X/Sufami Turbo combination is supported).");
        }
    }

    Settings.StopEmulation = !active;

    return ok;
}

bool Snes9xController::saveGamePosition()
{
    if (!active) return false;
    auto fname = S9xGetFilename(".oops", SNAPSHOT_DIR);
    return S9xFreezeGame(fname.c_str()) != FALSE;
}

bool Snes9xController::loadGamePosition()
{
    if (!active) return false;
    auto fname = S9xGetFilename(".oops", SNAPSHOT_DIR);
    return S9xUnfreezeGame(fname.c_str()) != FALSE;
}

bool Snes9xController::takeScreenshot()
{
    if (!active) return false;

    int width = IPPU.RenderedScreenWidth;
    int height = IPPU.RenderedScreenHeight;
    if (width <= 0 || height <= 0)
        return false;

    QImage image(reinterpret_cast<const uchar *>(GFX.Screen), width, height,
                 GFX.Pitch, QImage::Format_RGB16);
    auto filename = S9xGetFilenameInc(".png", SCREENSHOT_DIR);
    if (!image.copy().save(QString::fromStdString(filename), "PNG"))
        return false;

    auto message = "Saved screenshot " + S9xBasename(filename);
    S9xSetInfoString(message.c_str());
    return true;
}

bool Snes9xController::saveSram()
{
    if (!active) return false;
    auto filename = S9xGetFilename(".srm", SRAM_DIR);
    if (!Memory.SaveSRAM(filename.c_str()))
        return false;

    auto message = "Saved S-RAM " + S9xBasename(filename);
    S9xSetInfoString(message.c_str());
    return true;
}

bool Snes9xController::saveMemoryPack()
{
    if (!canSaveMemoryPack()) return false;
    auto filename = S9xGetFilenameInc(".bs", SAT_DIR);
    if (!Memory.SaveMPAK(filename.c_str()))
        return false;

    auto message = "Saved Memory Pack " + S9xBasename(filename);
    S9xSetInfoString(message.c_str());
    return true;
}

bool Snes9xController::canSaveMemoryPack() const
{
    return active && (Settings.BS || (Multi.cartSizeB && Multi.cartType == 3));
}

bool Snes9xController::startMovieRecord(const std::string &filename)
{
    if (!active) return false;
    suspend();
    int rc = S9xMovieCreate(filename.c_str(), 0xFF, MOVIE_OPT_FROM_RESET, nullptr, 0);
    resume();
    return rc == 1;
}

bool Snes9xController::openMovie(const std::string &filename)
{
    if (!active) return false;
    suspend();
    int rc = S9xMovieOpen(filename.c_str(), FALSE);
    resume();
    return rc == 1;
}

void Snes9xController::stopMovie()
{
    if (!S9xMoviePlaying() && !S9xMovieRecording()) return;
    suspend();
    S9xMovieStop(FALSE);
    resume();
}

bool Snes9xController::isMovieActive() const
{
    return S9xMoviePlaying() || S9xMovieRecording();
}

bool Snes9xController::dumpSpc()
{
    if (!active) return false;
    suspend();
    auto filename = S9xGetFilenameInc(".spc", SPC_DIR);
    bool dumped = S9xSPCDump(filename.c_str()) != FALSE;
    resume();
    if (!dumped)
        return false;

    auto message = "Saved SPC " + S9xBasename(filename);
    S9xSetInfoString(message.c_str());
    return true;
}

std::string Snes9xController::romInfo() const
{
    if (!active) return QString("No ROM loaded.").toStdString();
    QString out;
    out += QString("Title: %1\n").arg(Memory.ROMName);
    out += QString("Size: %1 KB\n").arg(Memory.ROMSize * 8);
    out += QString("Map: %1\n").arg(Memory.MapType());
    out += QString("Region: %1\n").arg(Memory.Country());
    out += QString("SRAM: %1\n").arg(Memory.StaticRAMSize());
    out += QString("Company: %1\n").arg(Memory.PublishingCompany());
    out += QString("Cart contents: %1\n").arg(Memory.KartContents());
    return out.toStdString();
}

std::string Snes9xController::getContentFolder()
{
    return S9xGetDirectory(ROMFILENAME_DIR);
}

bool8 S9xNPConfirmLoadROM(const char *rom_name)
{
    // MVP: the Qt frontend requires both peers to already have the same ROM
    // loaded before connecting, so this ought not be hit in practice. Auto-
    // accept rather than block the netplay thread on a dialog.
    return TRUE;
}

void S9xSetPause(uint32 mask)
{
    Settings.ForcedPause |= mask;
}

void S9xClearPause(uint32 mask)
{
    Settings.ForcedPause &= ~mask;
}

bool Snes9xController::netplayConnectInternal(const std::string &host, int port)
{
    if (!active)
    {
        S9xNPSetError("Load a ROM before connecting to a Netplay server.");
        return false;
    }

    S9xAutoSaveSRAM();

    NetPlay.MaxBehindFrameCount = 15;
    NetPlay.Waiting4EmulationThread = false;
    NetPlay.ErrorMsg[0] = 0;
    NetPlay.WarningMsg[0] = 0;
    netplay_last_warning.clear();

    uint32 flags = CPU.Flags;

    S9xSetPause(PAUSE_NETPLAY_CONNECT);

    if (!S9xNPConnectToServer(host.c_str(), port, Memory.ROMName))
    {
        S9xClearPause(PAUSE_NETPLAY_CONNECT);
        return false;
    }

    for (int waited = 0; waited < 15000 && !Settings.NetPlay && !NetPlay.ErrorMsg[0]; waited += 20)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));

    if (!Settings.NetPlay)
    {
        // The actual connect() runs on a background thread (see
        // S9xNPConnectToServer/S9xNPClientLoop) and may still be stuck
        // waiting on a filtered/unreachable port when our wait above gives
        // up, leaving NetPlay.ErrorMsg empty -- without this, the dialog
        // shows a blank, uninformative error box.
        if (!NetPlay.ErrorMsg[0])
            S9xNPSetError("Connection attempt timed out.\n\nCheck that the server address and port are "
                          "correct, and that the port is forwarded/open on the host's router and firewall.");
        return false;
    }

    S9xReset();
    CPU.Flags = flags;

    return true;
}

bool Snes9xController::netplayConnect(const std::string &host, int port)
{
    netplayDisconnect();

    if (!netplayConnectInternal(host, port))
        return false;

    S9xSetInfoString(("Connected to " + host + ":" + std::to_string(port)).c_str());
    return true;
}

bool Snes9xController::netplayStartServer(int port)
{
    netplayDisconnect();

    if (!active)
    {
        S9xNPSetError("Load a ROM before starting a Netplay server.");
        return false;
    }

    S9xAutoSaveSRAM();

    Settings.NetPlayServer = true;

    if (!S9xNPStartServer(port))
    {
        Settings.NetPlayServer = false;
        return false;
    }

    // Give the server thread a moment to start listening before connecting
    // to it as the first client. Use the internal helper directly so we
    // don't tear the server back down via netplayDisconnect().
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    if (!netplayConnectInternal("127.0.0.1", port))
    {
        S9xNPStopServer();
        Settings.NetPlayServer = false;
        return false;
    }

    // Seed the tracked state with the loopback self-connect above already
    // applied, so mainLoop() doesn't mistake it for a new player joining.
    for (int i = 0; i < 8; i++)
        netplay_client_was_connected[i] = NPServer.Clients[i].Connected;

    S9xSetInfoString(("Netplay server started on port " + std::to_string(port)).c_str());
    return true;
}

void Snes9xController::netplayDisconnect()
{
    bool was_server = Settings.NetPlayServer;
    bool was_connected = Settings.NetPlay && NetPlay.Connected;

    if (was_connected)
        S9xNPDisconnect();

    if (Settings.NetPlayServer)
    {
        S9xNPStopServer();
        Settings.NetPlayServer = false;
    }

    NetPlay.Paused = false;
    netplay_last_warning.clear();
    for (int i = 0; i < 8; i++)
        netplay_client_was_connected[i] = false;

    if (was_server)
        S9xSetInfoString("Netplay server stopped");
    else if (was_connected)
        S9xSetInfoString("Disconnected from netplay server");
}

bool Snes9xController::netplayConnected() const
{
    return Settings.NetPlay && NetPlay.Connected;
}

bool Snes9xController::netplayIsServer() const
{
    return Settings.NetPlayServer;
}

void Snes9xController::netplayResyncClients()
{
    if (Settings.NetPlay && Settings.NetPlayServer)
        S9xNPServerQueueSyncAll();
}

void Snes9xController::netplaySendRomToClients()
{
    if (Settings.NetPlay && Settings.NetPlayServer)
        S9xNPServerQueueSendingROMImage();
}

void Snes9xController::netplaySendJoypadSwap()
{
    if (Settings.NetPlay)
        S9xNPSendJoypadSwap();
}

void Snes9xController::netplaySetSendRomOnConnect(bool enabled)
{
    NPServer.SendROMImageOnConnect = enabled;
}

void Snes9xController::netplaySetSyncByReset(bool enabled)
{
    NPServer.SyncByReset = enabled;
}

void Snes9xController::netplaySetMaxFrameLoss(int frames)
{
    NetPlay.MaxBehindFrameCount = frames;
}

std::string Snes9xController::netplayLastError()
{
    return std::string(NetPlay.ErrorMsg);
}

int Snes9xController::netplaySyncSpeed()
{
    if (!Settings.NetPlay || !NetPlay.Connected)
        return 0;

    // S9xNPConnectToServer() spawns a background thread that owns
    // NetPlay.Socket on Windows -- must synchronize via the semaphore
    // helper, not by reading the socket ourselves (see netplay.cpp).
    S9xNPClientSyncSpeed(netplay_local_joypads[0], netplay_joypads);

    return 1;
}

bool Snes9xController::netplayPush()
{
    if (!Settings.NetPlay)
        return false;

    if (NetPlay.PendingWait4Sync && !S9xNPClientWaitPendingSync(100))
    {
        NetPlay.Paused = true;
        return true;
    }

    NetPlay.Paused = false;

    for (int i = 0; i < 8; i++)
    {
        netplay_local_joypads[i] = MovieGetJoypad(i);
        MovieSetJoypad(i, netplay_joypads[i]);
    }

    if (NetPlay.PendingWait4Sync)
    {
        NetPlay.PendingWait4Sync = false;
        NetPlay.FrameCount++;
        S9xNPStepJoypadHistory();
    }

    return false;
}

void Snes9xController::netplayPop()
{
    if (!Settings.NetPlay)
        return;

    for (int i = 0; i < 8; i++)
        MovieSetJoypad(i, netplay_local_joypads[i]);
}