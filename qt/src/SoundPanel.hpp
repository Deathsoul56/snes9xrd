#pragma once
#include "ui_SoundPanel.h"
#include "EmuApplication.hpp"
#include <QMenu>
#include <array>

class SoundPanel :
    public Ui::SoundPanel,
    public QWidget
{
  public:
    explicit SoundPanel(EmuApplication *app);
    EmuApplication *app;
    void showEvent(QShowEvent *event) override;
    void updateInputRate();

    std::vector<std::string> driver_list;

  private:
    // The 8 SNES APU sound channel checkboxes, listed once and reused by the
    // constructor's per-channel connections, the "Enable All" handler, and
    // showEvent's initial state sync.
    std::array<QCheckBox *, 8> channelCheckboxes() const;
};