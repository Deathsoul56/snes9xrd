#include "HacksDialog.hpp"

#include "EmuApplication.hpp"
#include "EmuConfig.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

HacksDialog::HacksDialog(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setWindowTitle(tr("Emulator Hacks"));

    auto *layout = new QVBoxLayout(this);
    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    superfx_clock = new QSpinBox(this);
    superfx_clock->setRange(50, 1000);
    superfx_clock->setSingleStep(10);
    superfx_clock->setSuffix("%");
    form->addRow(tr("SuperFX Clock Speed:"), superfx_clock);

    overclock = new QComboBox(this);
    overclock->addItems({ tr("None"), tr("Auto-FastROM"), tr("Low"), tr("High") });
    form->addRow(tr("CPU Overclock:"), overclock);

    sound_filter = new QComboBox(this);
    sound_filter->addItems({ tr("Nearest"), tr("Linear"), tr("Gaussian (SNES Hardware)"), tr("Cubic"), tr("Sinc") });
    form->addRow(tr("Sound Interpolation Type:"), sound_filter);

    allow_invalid_vram = new QCheckBox(tr("Allow Invalid VRAM Access"), this);
    separate_echo_buffer = new QCheckBox(tr("Separate Echo Buffer from RAM"), this);
    disable_sprite_limit = new QCheckBox(tr("Disable Sprite Limit"), this);

    form->addRow(QString(), allow_invalid_vram);
    form->addRow(QString(), separate_echo_buffer);
    form->addRow(QString(), disable_sprite_limit);
    layout->addLayout(form);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *defaults = buttons->addButton(tr("Set Defaults"), QDialogButtonBox::ResetRole);
    layout->addWidget(buttons);

    superfx_clock->setValue(app->config->superfx_clock_multiplier);
    overclock->setCurrentIndex(app->config->overclock);
    sound_filter->setCurrentIndex(app->config->sound_filter);
    allow_invalid_vram->setChecked(app->config->allow_invalid_vram_access);
    separate_echo_buffer->setChecked(app->config->enable_shadow_buffer);
    disable_sprite_limit->setChecked(app->config->remove_sprite_limit);

    connect(defaults, &QPushButton::clicked, this, &HacksDialog::setDefaults);
    connect(buttons, &QDialogButtonBox::accepted, this, [this] {
        apply();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void HacksDialog::setDefaults()
{
    superfx_clock->setValue(100);
    overclock->setCurrentIndex(EmuConfig::eNoOverclock);
    sound_filter->setCurrentIndex(EmuConfig::eGaussian);
    allow_invalid_vram->setChecked(false);
    separate_echo_buffer->setChecked(false);
    disable_sprite_limit->setChecked(false);
}

void HacksDialog::apply()
{
    app->config->superfx_clock_multiplier = superfx_clock->value();
    app->config->overclock = overclock->currentIndex();
    app->config->sound_filter = sound_filter->currentIndex();
    app->config->allow_invalid_vram_access = allow_invalid_vram->isChecked();
    app->config->enable_shadow_buffer = separate_echo_buffer->isChecked();
    app->config->remove_sprite_limit = disable_sprite_limit->isChecked();
    app->updateSettings();
}
