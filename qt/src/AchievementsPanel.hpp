#pragma once
#include "ui_AchievementsPanel.h"

class EmuApplication;
class QTimer;

class AchievementsPanel :
    public Ui::AchievementsPanel,
    public QWidget
{
  public:
    explicit AchievementsPanel(EmuApplication *app);
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

    EmuApplication *app;

  private:
    void refreshAccountState();

    // Auto re-login with a saved session token resolves in the background
    // (see Snes9xController::pollAchievementsAutoLogin()), so poll here too
    // while this panel is visible instead of only refreshing on show/click.
    QTimer *refresh_timer;
};
