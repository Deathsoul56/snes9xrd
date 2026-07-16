#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include "EmuGameList.hpp"
#include "SDLInputManager.hpp"

#include <clocale>
#include <qnamespace.h>
#include <QFile>
#include <QPalette>
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

    bool use_dark_theme = true;
    if (QApplication::platformName() == "windows" &&
        QApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark)
    {
        // Light scheme on Windows: keep the native-ish look instead of
        // forcing our dark stylesheet/palette.
        use_dark_theme = false;
        QApplication::setStyle("windowsvista");
    }

    // Load the DuckStation-style stylesheet (replaces the older per-platform
    // dark-palette fallback). It's harmless on light schemes — the stylesheet
    // is mostly dark colors with a few non-conflicting light overrides.
    if (use_dark_theme)
    {
        QFile qss_file(":/duckstation.qss");
        if (qss_file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qApp->setStyleSheet(QString::fromUtf8(qss_file.readAll()));
            qss_file.close();
        }

        // Custom-painted widgets (e.g. SnesControllerWidget) can't read colors
        // out of a stylesheet, so mirror the same palette here via QPalette.
        // Keep these values in sync with qt/src/resources/duckstation.qss.
        QPalette dark_palette = qApp->palette();
        dark_palette.setColor(QPalette::Window, QColor("#1e1f25"));
        dark_palette.setColor(QPalette::WindowText, QColor("#e8e8e8"));
        dark_palette.setColor(QPalette::Base, QColor("#16171c"));
        dark_palette.setColor(QPalette::AlternateBase, QColor("#1a1b21"));
        dark_palette.setColor(QPalette::Text, QColor("#d8d8d8"));
        dark_palette.setColor(QPalette::Button, QColor("#23252c"));
        dark_palette.setColor(QPalette::ButtonText, QColor("#e8e8e8"));
        dark_palette.setColor(QPalette::Highlight, QColor("#3a4d6e"));
        dark_palette.setColor(QPalette::HighlightedText, Qt::white);
        qApp->setPalette(dark_palette);
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
