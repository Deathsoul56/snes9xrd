#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include "EmuGameList.hpp"
#include "SDLInputManager.hpp"

#include <clocale>
#include <qnamespace.h>
#include <QFile>
#include <QStyle>
#include <QStyleHints>

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

    if (QApplication::platformName() == "windows")
        QApplication::setStyle("fusion");

    // Load the DuckStation-style stylesheet (replaces the older per-platform
    // dark-palette fallback). It's harmless on light schemes — the stylesheet
    // is mostly dark colors with a few non-conflicting light overrides.
    {
        QFile qss_file(":/duckstation.qss");
        if (qss_file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qApp->setStyleSheet(QString::fromUtf8(qss_file.readAll()));
            qss_file.close();
        }
    }

    if (QApplication::platformName() == "windows")
    {
        if (QApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark)
        {
            // Light scheme on Windows: keep the native-ish look by dropping
            // our dark stylesheet. Otherwise stick with the DuckStation look.
            qApp->setStyleSheet(QString());
            QApplication::setStyle("windowsvista");
        }
    }

#ifndef _WIN32
    auto quit_handler = [](int) { QApplication::quit(); };
    for (auto s : { SIGQUIT, SIGINT, SIGTERM, SIGHUP })
        signal(s, quit_handler);
#endif

    emu.startThread();

    emu.config = std::make_unique<EmuConfig>();
    emu.config->setDefaults();
    emu.config->loadFile(EmuConfig::findConfigFile());

    emu.input_manager = std::make_unique<SDLInputManager>();
    emu.window = std::make_unique<EmuMainWindow>(&emu);
    if (emu.config->main_window_maximized)
        emu.window->showMaximized();
    else
        emu.window->show();

    emu.updateBindings();
    emu.startInputTimer();
    emu.qtapp->exec();

    emu.stopThread();
    emu.config->saveFile(EmuConfig::findConfigFile());

    return 0;
}
