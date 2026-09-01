#pragma once

#include <QDialog>

class EmuApplication;
class QRadioButton;
class QLineEdit;
class QSpinBox;
class QCheckBox;
class QLabel;

class NetplayDialog : public QDialog
{
  public:
    NetplayDialog(QWidget *parent, EmuApplication *app, bool host_mode);

  private:
    void connectOrHost();

    EmuApplication *app;
    QRadioButton *host_radio;
    QRadioButton *connect_radio;
    QLineEdit *host_edit;
    QSpinBox *port_edit;
    QCheckBox *sync_reset_check;
    QCheckBox *send_rom_check;
    QSpinBox *max_frame_loss_edit;
    QLabel *status_label;
};
