#pragma once
#include "ui_ControllerPanel.h"
#include "BindingPanel.hpp"
#include <QMenu>
#include <QSet>
#include <QString>
#include <QTimer>

class EmuApplication;
class SnesControllerWidget;

class ControllerPanel :
    public Ui::ControllerPanel,
    public BindingPanel
{
  public:
    explicit ControllerPanel(EmuApplication *app);
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void clearAllControllers();
    void clearCurrentController();
    void autoPopulateWithKeyboard(int slot);
    void autoPopulateWithJoystick(int joystick_id, int slot);
    void recreateAutoAssignMenu();
    void onImageButtonClicked(const QString &snes_name);
    static QString snes_name_for_row(int row);

    QMenu edit_menu;
    QMenu auto_assign_menu;

  private:
    void updateBindingView(int combo_index);
    void updateMouseShortcutHint();

    SnesControllerWidget *controller_image_ = nullptr;
    QTimer live_input_timer_;
    // SNES button names currently held down on the keyboard, mirroring
    // SDLInputManager::pressedSnesNames() for gamepads (see eventFilter()).
    QSet<QString> pressed_keyboard_names_;
};