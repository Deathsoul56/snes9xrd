#pragma once

// Shared connect helpers for the simple "checkbox/spinbox/combobox directly
// maps to a config field, then reload settings" pattern repeated across the
// settings panels (GeneralPanel, EmulationPanel, ...). Kept as free functions
// rather than a base class so each panel can still use Qt Designer's
// setupUi()-generated multiple inheritance without diamond issues.

#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>

#include "EmuApplication.hpp"

inline void connectCheckbox(QCheckBox *box, bool *config, EmuApplication *app)
{
    QObject::connect(box, &QCheckBox::clicked, [config, app](bool checked) {
        *config = checked;
        app->updateSettings();
    });
}

inline void connectSpinBox(QSpinBox *box, int *config, EmuApplication *app)
{
    QObject::connect(box, &QSpinBox::valueChanged, [config, app](int value) {
        *config = value;
        app->updateSettings();
    });
}

inline void connectComboBox(QComboBox *box, int *config, EmuApplication *app)
{
    QObject::connect(box, &QComboBox::activated, [config, app](int index) {
        *config = index;
        app->updateSettings();
    });
}
