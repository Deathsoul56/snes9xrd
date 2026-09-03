#pragma once

#include <QColor>
#include <string>
#include <utility>
#include <vector>

// Full set of colors making up one theme. Background/border tones are
// included (not just the accent) so themes can radically recolor the whole
// app chrome, the way DuckStation/PCSX2's theme pickers do, instead of only
// swapping a highlight color.
struct EmuThemeColors
{
    QColor bg_window;
    QColor bg_panel;
    QColor bg_alt;
    QColor bg_elevated;
    QColor border;
    QColor border_light;
    QColor text;
    QColor text_bright;
    QColor text_muted;
    QColor text_disabled;
    QColor accent;
    QColor accent_light;
    QColor accent_lighter;
    QColor accent_dark;
};

// Central registry for the DuckStation/PCSX2-style theme picker: a handful
// of dark color variants plus a native "Light" and "Follow System" option.
// Both startup (main.cpp) and the live Settings change handler
// (GeneralPanel) call apply() so the stylesheet/palette/colors can never
// drift between the two.
struct EmuTheme
{
    // {config id (stored in the ini, stable across reorders), display name}
    static const std::vector<std::pair<std::string, std::string>> &list();

    // Applies the given theme's stylesheet and QPalette to the whole
    // application. Safe to call again any time the setting changes.
    static void apply(const std::string &theme_id);

    // Colors of whichever theme is currently applied, for the few
    // custom-painted widgets (e.g. GameListWidget's header hover box) that
    // can't pull colors out of a stylesheet.
    static const EmuThemeColors &colors();
};
