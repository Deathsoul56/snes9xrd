#pragma once
#include "ui_AchievementsPanel.h"

class EmuApplication;

class AchievementsPanel :
    public Ui::AchievementsPanel,
    public QWidget
{
  public:
    explicit AchievementsPanel(EmuApplication *app);
    void showEvent(QShowEvent *event) override;

    EmuApplication *app;

  private:
    void refreshAccountState();
};
