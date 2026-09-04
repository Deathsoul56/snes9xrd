#pragma once

#include "ui_EmuSettingsWindow.h"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "GeneralPanel.hpp"
#include "DisplayPanel.hpp"
#include "SoundPanel.hpp"
#include "EmulationPanel.hpp"
#include "ControllerPanel.hpp"
#include "FoldersPanel.hpp"
#include "ShortcutsPanel.hpp"
#include "AchievementsPanel.hpp"

class EmuSettingsWindow
  : public QDialog,
    public Ui::EmuSettingsWindow
{
  public:
    EmuSettingsWindow(QWidget *parent, EmuApplication *app);
    void show(int page);
    void reject() override;
    // Re-applies the sidebar icons (and the panels' own icons) using the
    // icon set matching the currently active theme.
    void refreshIcons();

    EmuApplication *app;
    GeneralPanel *general_panel;
    DisplayPanel *display_panel;
    SoundPanel *sound_panel;
    EmulationPanel *emulation_panel;
    ControllerPanel *controller_panel;
    ShortcutsPanel *shortcuts_panel;
    FoldersPanel *folders_panel;
    AchievementsPanel *achievements_panel;

  private:
    EmuConfig original_config_;
};