#include "GeneralPanel.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include "EmuTheme.hpp"
#include "PanelConnectHelpers.hpp"
#include "SDLInputManager.hpp"
#include "WinFileAssociation.hpp"

GeneralPanel::GeneralPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    for (const auto &theme : EmuTheme::list())
        comboBox_theme->addItem(QString::fromStdString(theme.second));
    connectComboBox(comboBox_theme, &app->config->theme, app);
    connect(comboBox_theme, &QComboBox::activated, this, [app_](int index) {
        EmuTheme::apply(EmuTheme::list()[index].first);
        app_->window->refreshIcons();
    });

    connectCheckbox(checkBox_fullscreen_on_open, &app->config->fullscreen_on_open, app);
    connectCheckbox(checkBox_disable_screensaver, &app->config->disable_screensaver, app);
    connectCheckbox(checkBox_pause_when_unfocused, &app->config->pause_emulation_when_unfocused, app);
    connectCheckbox(checkBox_background_gamepad_input, &app->config->background_gamepad_input, app);
    connect(checkBox_background_gamepad_input, &QCheckBox::clicked, this, [](bool checked) {
        SDLInputManager::setBackgroundInputEnabled(checked);
    });
    connectCheckbox(checkBox_confirm_save_load, &app->config->confirm_save_load, app);
    connectCheckbox(checkBox_save_state_on_close, &app->config->save_state_on_close, app);
#ifdef _WIN32
    connectCheckbox(checkBox_add_to_registry, &app->config->add_to_registry, app);
    connect(checkBox_add_to_registry, &QCheckBox::clicked, this, [](bool checked) {
        WinFileAssociation::apply(checked, EmuApplication::romExtensionsForRegistry());
    });
#else
    checkBox_add_to_registry->setVisible(false);
#endif
    connectCheckbox(checkBox_show_frame_rate, &app->config->show_frame_rate, app);
    connectCheckbox(checkBox_show_indicators, &app->config->show_indicators, app);
    connectCheckbox(checkBox_show_pressed_keys, &app->config->show_pressed_keys, app);
    connectCheckbox(checkBox_show_time, &app->config->show_time, app);
    connectCheckbox(checkBox_show_rom_info, &app->config->show_rom_info_on_load, app);
    connectSpinBox(spinBox_sram_interval, &app->config->sram_save_interval, app);
}

void GeneralPanel::showEvent(QShowEvent *event)
{
    auto &config = app->config;
    comboBox_theme->setCurrentIndex(config->theme);
    checkBox_fullscreen_on_open->setChecked(config->fullscreen_on_open);
    checkBox_disable_screensaver->setChecked(config->disable_screensaver);
    checkBox_disable_screensaver->setVisible(false);
    checkBox_pause_when_unfocused->setChecked(config->pause_emulation_when_unfocused);
    checkBox_background_gamepad_input->setChecked(config->background_gamepad_input);
    checkBox_confirm_save_load->setChecked(config->confirm_save_load);
    checkBox_save_state_on_close->setChecked(config->save_state_on_close);
#ifdef _WIN32
    checkBox_add_to_registry->setChecked(config->add_to_registry);
#endif
    checkBox_show_frame_rate->setChecked(config->show_frame_rate);
    checkBox_show_indicators->setChecked(config->show_indicators);
    checkBox_show_pressed_keys->setChecked(config->show_pressed_keys);
    checkBox_show_time->setChecked(config->show_time);
    checkBox_show_rom_info->setChecked(config->show_rom_info_on_load);
    spinBox_sram_interval->setValue(config->sram_save_interval);

    QWidget::showEvent(event);
}

