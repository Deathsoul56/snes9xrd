#pragma once

#include <QDialog>

class EmuApplication;
class QComboBox;
class QToolButton;

class StatePreviewDialog : public QDialog
{
  public:
    StatePreviewDialog(QWidget *parent, EmuApplication *app, bool save);

  private:
    void refresh();
    void selectSlot(int slot);

    EmuApplication *app;
    bool save;
    QComboBox *bank;
    QToolButton *slot_buttons[10];
};
