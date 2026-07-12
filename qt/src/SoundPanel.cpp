#include "SoundPanel.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "Snes9xController.hpp"

static constexpr int playback_rates[] = { 96000, 48000, 44100 };

SoundPanel::SoundPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    connect(comboBox_driver, &QComboBox::activated, [&](int index){
        if (app->config->sound_driver != driver_list[index])
        {
            app->config->sound_driver = driver_list[index];
            app->restartAudio();
        }
    });

    connect(comboBox_playback_rate, &QComboBox::activated, [&](int index)
    {
        if (index < 3)
        {
            if (playback_rates[index] != app->config->playback_rate)
            {
                app->config->playback_rate = playback_rates[index];
                app->restartAudio();
                app->updateSettings();
            }
        }
    });

    connect(spinBox_buffer_size, &QSpinBox::valueChanged, [&](int value) {
        if (app->config->audio_buffer_size_ms != value)
        {
            app->config->audio_buffer_size_ms = value;
            app->restartAudio();
        }
    });

    connect(checkBox_adjust_input_rate, &QCheckBox::clicked, [&](bool checked) {
        if (!checked && checked != app->config->adjust_input_rate_automatically)
            app->config->input_rate = 32040;
        app->config->adjust_input_rate_automatically = checked;
        app->updateSettings();
        updateInputRate();

        if (checked)
        {
           horizontalSlider_input_rate->setValue(app->config->input_rate);
        }
    });

    connect(horizontalSlider_input_rate, &QSlider::valueChanged, [&](int value) {
        app->config->input_rate = value;
        app->updateSettings();
        updateInputRate();
    });

    connect(checkBox_dynamic_rate_control, &QCheckBox::clicked, [&](bool checked) {
        app->config->dynamic_rate_control = checked;
        app->updateSettings();
    });

    connect(doubleSpinBox_dynamic_rate_limit, &QDoubleSpinBox::valueChanged, [&](double value) {
        app->config->dynamic_rate_limit = value;
        app->updateSettings();
    });

    connect(checkBox_mute_sound, &QCheckBox::toggled, [&](bool checked) {
        app->config->mute_audio = checked;
    });

    connect(checkBox_mute_during_alt_speed, &QCheckBox::toggled, [&](bool checked) {
        app->config->mute_audio_during_alternate_speed = checked;
    });

    auto connect_volume_slider = [&](QSlider *slider, QSpinBox *spinbox, int EmuConfig::*field) {
        connect(slider, &QSlider::valueChanged, [&, spinbox, field](int value) {
            app->config.get()->*field = value;
            spinbox->blockSignals(true);
            spinbox->setValue(value);
            spinbox->blockSignals(false);
        });
        connect(spinbox, &QSpinBox::valueChanged, [&, slider, field](int value) {
            app->config.get()->*field = value;
            slider->blockSignals(true);
            slider->setValue(value);
            slider->blockSignals(false);
        });
    };
    connect_volume_slider(horizontalSlider_volume_regular, spinBox_volume_regular, &EmuConfig::volume_regular);
    connect_volume_slider(horizontalSlider_volume_turbo, spinBox_volume_turbo, &EmuConfig::volume_turbo);

    auto connect_channel = [&](QCheckBox *checkbox, int channel) {
        connect(checkbox, &QCheckBox::toggled, [&, channel](bool checked) {
            app->core->setSoundChannelEnabled(channel, checked);
        });
    };
    connect_channel(checkBox_channel_0, 0);
    connect_channel(checkBox_channel_1, 1);
    connect_channel(checkBox_channel_2, 2);
    connect_channel(checkBox_channel_3, 3);
    connect_channel(checkBox_channel_4, 4);
    connect_channel(checkBox_channel_5, 5);
    connect_channel(checkBox_channel_6, 6);
    connect_channel(checkBox_channel_7, 7);

    connect(pushButton_enable_all_channels, &QPushButton::clicked, [&] {
        app->core->toggleSoundChannel(8);
        for (auto *cb : { checkBox_channel_0, checkBox_channel_1, checkBox_channel_2, checkBox_channel_3,
                          checkBox_channel_4, checkBox_channel_5, checkBox_channel_6, checkBox_channel_7 })
        {
            cb->blockSignals(true);
            cb->setChecked(true);
            cb->blockSignals(false);
        }
    });
}

void SoundPanel::updateInputRate()
{
    constexpr double ir_ratio = 60.098813 / 32040.0;

    app->updateSettings();
    if (app->config->adjust_input_rate_automatically)
        horizontalSlider_input_rate->setEnabled(false);
    else
        horizontalSlider_input_rate->setEnabled(true);

    double hz = app->config->input_rate * ir_ratio;

    label_input_rate->setText(QString("%1%3\n%2 Hz")
        .arg(app->config->input_rate)
        .arg(hz, 6, 'g', 6)
        .arg(app->config->input_rate == 32040.0 ? " (Default)" : ""));
}

void SoundPanel::showEvent(QShowEvent *event)
{
    auto &config = app->config;

    comboBox_driver->clear();
    driver_list.clear();

    comboBox_driver->addItem("Cubeb");
    driver_list.push_back("cubeb");
    comboBox_driver->addItem("SDL");
    driver_list.push_back("sdl");
#ifdef USE_PULSEAUDIO
    comboBox_driver->addItem("PulseAudio");
    driver_list.push_back("pulseaudio");
#endif

    for (int i = 0; i < driver_list.size(); i++)
    {
        if (driver_list[i] == config->sound_driver)
        {
            comboBox_driver->setCurrentIndex(i);
            break;
        }
    }

    comboBox_device->clear();
    comboBox_device->addItem("Default");

    comboBox_playback_rate->clear();
    comboBox_playback_rate->addItem("96000Hz");
    comboBox_playback_rate->addItem("48000Hz");
    comboBox_playback_rate->addItem("44100Hz");
    int pbr_index = 1;
    if (config->playback_rate == 96000)
        pbr_index = 0;
    else if (config->playback_rate == 44100)
        pbr_index = 2;

    comboBox_playback_rate->setCurrentIndex(pbr_index);
    spinBox_buffer_size->setValue(config->audio_buffer_size_ms);

    checkBox_adjust_input_rate->setChecked(config->adjust_input_rate_automatically);
    updateInputRate();
    horizontalSlider_input_rate->setValue(config->input_rate);
    checkBox_dynamic_rate_control->setChecked(config->dynamic_rate_control);
    doubleSpinBox_dynamic_rate_limit->setValue(config->dynamic_rate_limit);

    checkBox_mute_sound->setChecked(config->mute_audio);
    checkBox_mute_during_alt_speed->setChecked(config->mute_audio_during_alternate_speed);

    horizontalSlider_volume_regular->setValue(config->volume_regular);
    spinBox_volume_regular->setValue(config->volume_regular);
    horizontalSlider_volume_turbo->setValue(config->volume_turbo);
    spinBox_volume_turbo->setValue(config->volume_turbo);

    QCheckBox *channel_checkboxes[] = { checkBox_channel_0, checkBox_channel_1, checkBox_channel_2, checkBox_channel_3,
                                        checkBox_channel_4, checkBox_channel_5, checkBox_channel_6, checkBox_channel_7 };
    for (int i = 0; i < 8; i++)
    {
        channel_checkboxes[i]->blockSignals(true);
        channel_checkboxes[i]->setChecked((app->core->sound_channel_switch >> i) & 1);
        channel_checkboxes[i]->blockSignals(false);
    }
}

