#include "EmuTheme.hpp"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyle>
#include <QStyleHints>

namespace {

const EmuThemeColors &paletteFor(const std::string &theme_id)
{
    // Dark variants all share the same text/border_light shades -- only the
    // background family and accent change between them.
    const QColor dark_text          = QColor("#e2e2e2");
    const QColor dark_text_bright   = QColor("#f5f5f5");
    const QColor dark_text_muted    = QColor("#9a9dab");
    const QColor dark_text_disabled = QColor("#555555");
    const QColor dark_border_light  = QColor("#474b58");

    // Each theme tints the background/panel/border family too, not just the
    // accent, so switching themes reads as a real reskin (DuckStation/PCSX2
    // style) instead of a single highlight color changing.
    static const EmuThemeColors blue = {
        QColor("#1e1f25"), QColor("#16171c"), QColor("#1a1b21"), QColor("#23252c"), QColor("#2a2c33"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#4a90ff"), QColor("#4a9cff"), QColor("#6aa6ff"), QColor("#3a4d6e"),
    };
    static const EmuThemeColors purple = {
        QColor("#211a2e"), QColor("#170f22"), QColor("#1c1528"), QColor("#2c2140"), QColor("#3d2c56"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#b06bff"), QColor("#c085ff"), QColor("#d4aaff"), QColor("#4a2e70"),
    };
    static const EmuThemeColors green = {
        QColor("#14231c"), QColor("#0d1a14"), QColor("#112018"), QColor("#1c3327"), QColor("#24503a"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#35d98f"), QColor("#5ce8a6"), QColor("#8cf0c0"), QColor("#1f5c3f"),
    };
    static const EmuThemeColors red = {
        QColor("#2c141d"), QColor("#200e15"), QColor("#271219"), QColor("#3d1b26"), QColor("#5c2536"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#ff4f6b"), QColor("#ff7086"), QColor("#ff9aab"), QColor("#6e1f30"),
    };
    static const EmuThemeColors teal = {
        QColor("#10262a"), QColor("#0a1c1f"), QColor("#0f2226"), QColor("#1a3a3e"), QColor("#235055"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#2fd0d1"), QColor("#57dede"), QColor("#8ceaea"), QColor("#1c565a"),
    };
    static const EmuThemeColors orange = {
        QColor("#2a1d10"), QColor("#1f150a"), QColor("#26190d"), QColor("#3d2a15"), QColor("#5c4020"), dark_border_light,
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#ff9a3c"), QColor("#ffb066"), QColor("#ffc98f"), QColor("#6e4420"),
    };
    // PCSX2-inspired additions below -- names/colors adapted from PCSX2's
    // own theme picker.
    static const EmuThemeColors dark_fusion_gray = {
        QColor("#202124"), QColor("#18191b"), QColor("#1c1d20"), QColor("#2a2b2e"), QColor("#34353a"), QColor("#4a4b52"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#6a90c9"), QColor("#82a3d6"), QColor("#a3bfe6"), QColor("#35506e"),
    };
    static const EmuThemeColors grey_matter = {
        QColor("#232323"), QColor("#1a1a1a"), QColor("#1f1f1f"), QColor("#2e2e2e"), QColor("#3a3a3a"), QColor("#505050"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#9a9a9a"), QColor("#b5b5b5"), QColor("#cfcfcf"), QColor("#3a3a3a"),
    };
    static const EmuThemeColors scarlet_devil = {
        QColor("#24141f"), QColor("#190d16"), QColor("#1f111b"), QColor("#33202c"), QColor("#4d2c40"), QColor("#64384f"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#d6386f"), QColor("#e35a8a"), QColor("#ef85ac"), QColor("#7a1f45"),
    };
    static const EmuThemeColors violet_angel = {
        QColor("#1a1830"), QColor("#121022"), QColor("#171528"), QColor("#262143"), QColor("#383060"), QColor("#4c4380"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#7c6bff"), QColor("#9585ff"), QColor("#b6acff"), QColor("#3d3480"),
    };
    static const EmuThemeColors cobalt_sky = {
        QColor("#0f1c2e"), QColor("#0a1420"), QColor("#0d1826"), QColor("#16283f"), QColor("#1f3a56"), QColor("#2c4d6e"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#2f8fe0"), QColor("#4fa3ea"), QColor("#7fc0f2"), QColor("#1a5085"),
    };
    static const EmuThemeColors amoled = {
        QColor("#000000"), QColor("#000000"), QColor("#060606"), QColor("#121212"), QColor("#202020"), QColor("#333333"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#3d8bff"), QColor("#5c9fff"), QColor("#8ebeff"), QColor("#1c4a86"),
    };
    static const EmuThemeColors amoled_purple = {
        QColor("#000000"), QColor("#000000"), QColor("#060606"), QColor("#121212"), QColor("#202020"), QColor("#333333"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#b06bff"), QColor("#c085ff"), QColor("#d4aaff"), QColor("#4a2e70"),
    };
    static const EmuThemeColors ruby = {
        QColor("#120608"), QColor("#0a0203"), QColor("#0f0507"), QColor("#1e0b0e"), QColor("#331216"), QColor("#4a1a20"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#e8384f"), QColor("#ee5c70"), QColor("#f38b98"), QColor("#7a1a26"),
    };
    static const EmuThemeColors sapphire = {
        QColor("#050a12"), QColor("#02060b"), QColor("#04080f"), QColor("#0c1a29"), QColor("#142943"), QColor("#1d3a5c"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#2f7fe0"), QColor("#4f97ea"), QColor("#7fb8f2"), QColor("#163f70"),
    };
    static const EmuThemeColors emerald = {
        QColor("#051209"), QColor("#020a05"), QColor("#040e08"), QColor("#0c2013"), QColor("#143a22"), QColor("#1c4e2e"),
        dark_text, dark_text_bright, dark_text_muted, dark_text_disabled,
        QColor("#2fd177"), QColor("#52dc8f"), QColor("#82e8ae"), QColor("#17603a"),
    };

    // Light variants all share the same text shades as "light" -- only the
    // background family and accent change between them.
    const QColor light_text          = QColor("#202124");
    const QColor light_text_bright   = QColor("#101012");
    const QColor light_text_muted    = QColor("#5f6368");
    const QColor light_text_disabled = QColor("#9aa0a6");

    // The accent here is deliberately dark/saturated (not pale) so the
    // white selection-text used throughout theme.qss stays legible even
    // though the rest of the page is light.
    static const EmuThemeColors light = {
        QColor("#f3f3f4"), QColor("#e8e8ea"), QColor("#ececee"), QColor("#dedee1"), QColor("#cfcfd3"), QColor("#b7b7bd"),
        light_text, light_text_bright, light_text_muted, light_text_disabled,
        QColor("#2f6fed"), QColor("#4a84f0"), QColor("#7aa5f5"), QColor("#1a56c4"),
    };
    static const EmuThemeColors untouched_lagoon = {
        QColor("#eef2f0"), QColor("#e2e9e6"), QColor("#e7ece9"), QColor("#d7e0dc"), QColor("#c3d0cb"), QColor("#a9bab3"),
        light_text, light_text_bright, light_text_muted, light_text_disabled,
        QColor("#3f8f8a"), QColor("#5aa8a2"), QColor("#82c4bd"), QColor("#1f5350"),
    };
    static const EmuThemeColors baby_pastel = {
        QColor("#fdf1f5"), QColor("#fbe4ec"), QColor("#fce8ef"), QColor("#f8d7e3"), QColor("#f0c2d4"), QColor("#e3a8c1"),
        light_text, light_text_bright, light_text_muted, light_text_disabled,
        QColor("#e0609a"), QColor("#ea7fae"), QColor("#f2a3c7"), QColor("#a6396d"),
    };
    static const EmuThemeColors pizza_time = {
        QColor("#fbf3e6"), QColor("#f3e6cf"), QColor("#f7ecdb"), QColor("#ecdcbd"), QColor("#ddc79c"), QColor("#c9ac7c"),
        light_text, light_text_bright, light_text_muted, light_text_disabled,
        QColor("#c2703a"), QColor("#d3894f"), QColor("#e3aa78"), QColor("#8a4b21"),
    };
    static const EmuThemeColors pcsx2_light = {
        QColor("#f5f7fb"), QColor("#e9eef7"), QColor("#eef2f9"), QColor("#dde5f2"), QColor("#c7d3e6"), QColor("#aebdd6"),
        light_text, light_text_bright, light_text_muted, light_text_disabled,
        QColor("#2f6fed"), QColor("#4a84f0"), QColor("#7aa5f5"), QColor("#1a56c4"),
    };

    if (theme_id == "light")             return light;
    if (theme_id == "dark_purple")       return purple;
    if (theme_id == "dark_green")        return green;
    if (theme_id == "dark_red")          return red;
    if (theme_id == "dark_teal")         return teal;
    if (theme_id == "dark_orange")       return orange;
    if (theme_id == "dark_fusion_gray")  return dark_fusion_gray;
    if (theme_id == "grey_matter")       return grey_matter;
    if (theme_id == "untouched_lagoon")  return untouched_lagoon;
    if (theme_id == "baby_pastel")       return baby_pastel;
    if (theme_id == "pizza_time")        return pizza_time;
    if (theme_id == "pcsx2_light")       return pcsx2_light;
    if (theme_id == "scarlet_devil")     return scarlet_devil;
    if (theme_id == "violet_angel")      return violet_angel;
    if (theme_id == "cobalt_sky")        return cobalt_sky;
    if (theme_id == "amoled")            return amoled;
    if (theme_id == "amoled_purple")     return amoled_purple;
    if (theme_id == "ruby")              return ruby;
    if (theme_id == "sapphire")          return sapphire;
    if (theme_id == "emerald")           return emerald;
    return blue; // "dark_blue" or unrecognized
}

// "system" just auto-picks between our own Light and Dark (Blue) themes
// based on the OS preference -- it never defers to native/unstyled
// rendering, which is what produced an inconsistent, broken-looking result.
std::string resolveThemeId(const std::string &theme_id)
{
    if (theme_id != "system")
        return theme_id;

    bool os_prefers_light = QApplication::platformName() == "windows" &&
        QApplication::styleHints()->colorScheme() != Qt::ColorScheme::Dark;
    return os_prefers_light ? "light" : "dark_blue";
}

EmuThemeColors g_current_colors = paletteFor("dark_blue");

} // namespace

const std::vector<std::pair<std::string, std::string>> &EmuTheme::list()
{
    // New themes must only be appended, never reordered/removed -- the
    // saved config stores a plain index into this list (see EmuConfig.cpp).
    static const std::vector<std::pair<std::string, std::string>> themes = {
        { "system",      "Follow System" },
        { "light",       "Light" },
        { "dark_blue",   "Dark Fusion (Blue)" },
        { "dark_purple", "Dark (Purple)" },
        { "dark_green",  "Dark (Green)" },
        { "dark_red",    "Dark (Red)" },
        { "dark_teal",   "Dark (Teal)" },
        { "dark_orange", "Dark (Orange)" },
        { "dark_fusion_gray", "Dark Fusion (Gray)" },
        { "grey_matter", "Grey Matter (Gray)" },
        { "untouched_lagoon", "Untouched Lagoon (Grayish Green/Blue)" },
        { "baby_pastel", "Baby Pastel (Pink)" },
        { "pizza_time",  "Pizza Time! (Brown-ish/Creamy White)" },
        { "pcsx2_light", "Blue Cloud (White/Blue)" },
        { "scarlet_devil", "Scarlet Devil (Red/Purple)" },
        { "violet_angel", "Violet Angel (Blue/Purple)" },
        { "cobalt_sky",  "Cobalt Sky (Blue)" },
        { "amoled",      "AMOLED (Black)" },
        { "amoled_purple", "AMOLED (Black/Purple)" },
        { "ruby",        "Ruby (Black/Red)" },
        { "sapphire",    "Sapphire (Black/Blue)" },
        { "emerald",     "Emerald (Black/Green)" },
    };
    return themes;
}

void EmuTheme::apply(const std::string &theme_id)
{
    if (QApplication::platformName() == "windows")
        QApplication::setStyle("fusion");

    const EmuThemeColors &colors = paletteFor(resolveThemeId(theme_id));

    QString qss;
    QFile qss_file(":/theme.qss");
    if (qss_file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qss = QString::fromUtf8(qss_file.readAll());
        qss_file.close();
    }
    qss.replace(QStringLiteral("@BG_WINDOW@"), colors.bg_window.name());
    qss.replace(QStringLiteral("@BG_PANEL@"), colors.bg_panel.name());
    qss.replace(QStringLiteral("@BG_ALT@"), colors.bg_alt.name());
    qss.replace(QStringLiteral("@BG_ELEVATED@"), colors.bg_elevated.name());
    qss.replace(QStringLiteral("@BORDER_LIGHT@"), colors.border_light.name());
    qss.replace(QStringLiteral("@BORDER@"), colors.border.name());
    qss.replace(QStringLiteral("@TEXT_BRIGHT@"), colors.text_bright.name());
    qss.replace(QStringLiteral("@TEXT_MUTED@"), colors.text_muted.name());
    qss.replace(QStringLiteral("@TEXT_DISABLED@"), colors.text_disabled.name());
    qss.replace(QStringLiteral("@TEXT@"), colors.text.name());
    qss.replace(QStringLiteral("@ACCENT_LIGHTER@"), colors.accent_lighter.name());
    qss.replace(QStringLiteral("@ACCENT_LIGHT@"), colors.accent_light.name());
    qss.replace(QStringLiteral("@ACCENT_DARK@"), colors.accent_dark.name());
    qss.replace(QStringLiteral("@ACCENT@"), colors.accent.name());

    // Checkable menu items draw a checkmark glyph that isn't tokenizable
    // via plain color substitution (it's an image, not CSS) -- pick the
    // icon variant whose contrast matches this theme's background, the
    // same way EmuApplication::iconPrefix() picks between icon sets.
    const bool dark_theme = colors.text_bright.lightness() > colors.bg_window.lightness();
    qss.replace(QStringLiteral("@CHECK_ICON@"),
                dark_theme ? QStringLiteral("whiteicons/check.svg") : QStringLiteral("blackicons/check.svg"));

    qApp->setStyleSheet(qss);

    // Custom-painted widgets (e.g. SnesControllerWidget) can't read colors
    // out of a stylesheet, so mirror the same palette here via QPalette.
    QPalette palette = qApp->palette();
    palette.setColor(QPalette::Window, colors.bg_window);
    palette.setColor(QPalette::WindowText, colors.text_bright);
    palette.setColor(QPalette::Base, colors.bg_panel);
    palette.setColor(QPalette::AlternateBase, colors.bg_alt);
    palette.setColor(QPalette::Text, colors.text);
    palette.setColor(QPalette::Button, colors.bg_elevated);
    palette.setColor(QPalette::ButtonText, colors.text_bright);
    palette.setColor(QPalette::Highlight, colors.accent_dark);
    palette.setColor(QPalette::HighlightedText, Qt::white);
    qApp->setPalette(palette);

    g_current_colors = colors;
}

const EmuThemeColors &EmuTheme::colors()
{
    return g_current_colors;
}
