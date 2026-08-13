#pragma once

#include <QDialog>

class EmuApplication;
class QCheckBox;
class QComboBox;
class QSpinBox;

class HacksDialog : public QDialog
{
  public:
    HacksDialog(QWidget *parent, EmuApplication *app);

  private:
    void setDefaults();
    void apply();

    EmuApplication *app;
    QSpinBox *superfx_clock;
    QComboBox *overclock;
    QComboBox *sound_filter;
    QCheckBox *allow_invalid_vram;
    QCheckBox *separate_echo_buffer;
    QCheckBox *disable_sprite_limit;
};
