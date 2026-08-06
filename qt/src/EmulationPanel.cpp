#include "EmulationPanel.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "PanelConnectHelpers.hpp"

EmulationPanel::EmulationPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    connectComboBox(comboBox_speed_control_method, &app->config->speed_sync_method, app);
    connect(doubleSpinBox_frame_rate, &QDoubleSpinBox::valueChanged, [&](double value) {
        app->config->fixed_frame_rate = value;
    });

    connectSpinBox(spinBox_rewind_buffer_size, &app->config->rewind_buffer_size, app);
    connectSpinBox(spinBox_rewind_frames, &app->config->rewind_frame_interval, app);
    connectSpinBox(spinBox_fast_forward_skip_frames, &app->config->fast_forward_skip_frames, app);
    connectSpinBox(spinBox_fixed_frame_skip, &app->config->fixed_frame_skip, app);

    connectCheckbox(checkBox_allow_invalid_vram_access, &app->config->allow_invalid_vram_access, app);
    connectCheckbox(checkBox_allow_opposing_dpad_directions, &app->config->allow_opposing_dpad_directions, app);
    connectComboBox(comboBox_overclock, &app->config->overclock, app);
    connectCheckbox(checkBox_remove_sprite_limit, &app->config->remove_sprite_limit, app);
    connectCheckbox(checkBox_use_shadow_echo_buffer, &app->config->enable_shadow_buffer, app);
    connectSpinBox(spinBox_superfx_clock_speed, &app->config->superfx_clock_multiplier, app);
    connectComboBox(comboBox_sound_filter, &app->config->sound_filter, app);
}

void EmulationPanel::showEvent(QShowEvent *event)
{
    auto &config = app->config;
    comboBox_speed_control_method->setCurrentIndex(config->speed_sync_method);
    doubleSpinBox_frame_rate->setValue(config->fixed_frame_rate);
    spinBox_fast_forward_skip_frames->setValue(config->fast_forward_skip_frames);
    spinBox_fixed_frame_skip->setValue(config->fixed_frame_skip);

    spinBox_rewind_buffer_size->setValue(config->rewind_buffer_size);
    spinBox_rewind_frames->setValue(config->rewind_frame_interval);

    checkBox_allow_invalid_vram_access->setChecked(config->allow_invalid_vram_access);
    checkBox_allow_opposing_dpad_directions->setChecked(config->allow_opposing_dpad_directions);
    comboBox_overclock->setCurrentIndex(config->overclock);
    checkBox_remove_sprite_limit->setChecked(config->remove_sprite_limit);
    checkBox_use_shadow_echo_buffer->setChecked(config->enable_shadow_buffer);
    spinBox_superfx_clock_speed->setValue(config->superfx_clock_multiplier);
    comboBox_sound_filter->setCurrentIndex(config->sound_filter);

    QWidget::showEvent(event);
}

