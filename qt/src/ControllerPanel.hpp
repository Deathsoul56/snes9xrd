#pragma once
#include "ui_ControllerPanel.h"
#include "BindingPanel.hpp"
#include <QMenu>
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
    void clearAllControllers();
    void clearCurrentController();
    void autoPopulateWithKeyboard(int slot);
    void autoPopulateWithJoystick(int joystick_id, int slot);
    void swapControllers(int first, int second);
    void recreateAutoAssignMenu();
    void onImageButtonClicked(const QString &snes_name);
    static QString snes_name_for_row(int row);

    QMenu edit_menu;
    QMenu auto_assign_menu;

  private:
    SnesControllerWidget *controller_image_ = nullptr;
    QTimer live_input_timer_;
};