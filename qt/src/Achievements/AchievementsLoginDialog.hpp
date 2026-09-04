#pragma once

#include <QDialog>

class EmuApplication;
class QLineEdit;
class QLabel;
class QTimer;
class QDialogButtonBox;

// Modeled on NetplayDialog: hand-coded QFormLayout + QDialogButtonBox, no
// .ui file. Unlike NetplayDialog's connect, RetroAchievements login is
// asynchronous (goes over HTTP), so this can't just call a wrapper and
// check the return value -- it fires the login attempt then polls with a
// QTimer until AchievementsClient reports the attempt is no longer pending.
class AchievementsLoginDialog : public QDialog
{
  public:
    AchievementsLoginDialog(QWidget *parent, EmuApplication *app);

  private:
    void login();
    void pollLoginResult();

    EmuApplication *app;
    QLineEdit *username_edit;
    QLineEdit *password_edit;
    QLabel *status_label;
    QDialogButtonBox *buttons;
    QTimer *poll_timer;
};
