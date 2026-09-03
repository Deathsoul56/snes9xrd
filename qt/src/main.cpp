#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include "EmuGameList.hpp"
#include "EmuTheme.hpp"
#include "SDLInputManager.hpp"
#ifdef _WIN32
#include "WinFileAssociation.hpp"
#endif

#include <clocale>
#include <qnamespace.h>

#ifndef _WIN32
#include <csignal>
#endif

#ifdef _WIN32
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, const char *lpCmdLine, int nShowCmd)
{
    char **argv = nullptr;
    int argc = 0;
    setlocale(LC_ALL, ".utf8");
#else
int main(int argc, char *argv[])
{
#endif
    EmuApplication emu;
    emu.qtapp = std::make_unique<QApplication>(argc, argv);

    QGuiApplication::setDesktopFileName("snes9xrd");

    emu.startThread();

    emu.config = std::make_unique<EmuConfig>();
    emu.config->setDefaults();
    emu.config->loadFile(EmuConfig::findConfigFile());

    const auto &themes = EmuTheme::list();
    EmuTheme::apply(emu.config->theme >= 0 && emu.config->theme < (int)themes.size()
        ? themes[emu.config->theme].first
        : "system");

#ifndef _WIN32
    auto quit_handler = [](int) { QApplication::quit(); };
    for (auto s : { SIGQUIT, SIGINT, SIGTERM, SIGHUP })
        signal(s, quit_handler);
#endif

#ifdef _WIN32
    // Re-apply the file association on every launch so it self-heals if the
    // exe was moved/updated since the user last enabled it.
    if (emu.config->add_to_registry)
        WinFileAssociation::apply(true, EmuApplication::romExtensionsForRegistry());
#endif

    emu.input_manager = std::make_unique<SDLInputManager>();
    SDLInputManager::setBackgroundInputEnabled(emu.config->background_gamepad_input);
    emu.window = std::make_unique<EmuMainWindow>(&emu);
    if (emu.config->main_window_maximized)
        emu.window->showMaximized();
    else
        emu.window->show();

    emu.updateBindings();
    emu.startInputTimer();
    emu.qtapp->exec();

    emu.stopThread();
    emu.revertControllerSwap();
    emu.config->saveFile(EmuConfig::findConfigFile());

    return 0;
}
