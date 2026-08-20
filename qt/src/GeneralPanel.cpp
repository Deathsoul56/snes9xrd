#include "GeneralPanel.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "PanelConnectHelpers.hpp"

GeneralPanel::GeneralPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    connectCheckbox(checkBox_fullscreen_on_open, &app->config->fullscreen_on_open, app);
    connectCheckbox(checkBox_disable_screensaver, &app->config->disable_screensaver, app);
    connectCheckbox(checkBox_pause_when_unfocused, &app->config->pause_emulation_when_unfocused, app);
    connectCheckbox(checkBox_show_frame_rate, &app->config->show_frame_rate, app);
    connectCheckbox(checkBox_show_indicators, &app->config->show_indicators, app);
    connectCheckbox(checkBox_show_pressed_keys, &app->config->show_pressed_keys, app);
    connectCheckbox(checkBox_show_time, &app->config->show_time, app);
    connectSpinBox(spinBox_sram_interval, &app->config->sram_save_interval, app);
}

void GeneralPanel::showEvent(QShowEvent *event)
{
    auto &config = app->config;
    checkBox_fullscreen_on_open->setChecked(config->fullscreen_on_open);
    checkBox_disable_screensaver->setChecked(config->disable_screensaver);
    checkBox_disable_screensaver->setVisible(false);
    checkBox_pause_when_unfocused->setChecked(config->pause_emulation_when_unfocused);
    checkBox_show_frame_rate->setChecked(config->show_frame_rate);
    checkBox_show_indicators->setChecked(config->show_indicators);
    checkBox_show_pressed_keys->setChecked(config->show_pressed_keys);
    checkBox_show_time->setChecked(config->show_time);
    spinBox_sram_interval->setValue(config->sram_save_interval);

    QWidget::showEvent(event);
}

