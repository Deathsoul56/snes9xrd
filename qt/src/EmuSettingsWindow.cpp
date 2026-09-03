#include "EmuSettingsWindow.hpp"
#include "EmuMainWindow.hpp"
#include "EmuConfig.hpp"
#include "EmuTheme.hpp"
#include "SDLInputManager.hpp"
#ifdef _WIN32
#include "WinFileAssociation.hpp"
#endif

#include <QScrollArea>
#include <QWhatsThis>

EmuSettingsWindow::EmuSettingsWindow(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setupUi(this);

    // Plain QWidget subclasses don't paint a stylesheet background unless
    // told to, so the settings pages fall through to raw (black) backing
    // store without this -- see the QSS rules targeting these pages below.
    auto styleBackground = [](QWidget *w) { w->setAttribute(Qt::WA_StyledBackground, true); };
    styleBackground(this);
    styleBackground(stackedWidget);
    styleBackground(panelList);

    general_panel = new GeneralPanel(app);
    styleBackground(general_panel);
    stackedWidget->addWidget(general_panel);

    auto *area = new QScrollArea(stackedWidget);
    area->setWidgetResizable(true);
    area->setFrameStyle(0);
    display_panel = new DisplayPanel(app);
    styleBackground(display_panel);
    area->setWidget(display_panel);
    stackedWidget->addWidget(area);

    sound_panel = new SoundPanel(app);
    styleBackground(sound_panel);
    stackedWidget->addWidget(sound_panel);

    emulation_panel = new EmulationPanel(app);
    styleBackground(emulation_panel);
    stackedWidget->addWidget(emulation_panel);

    controller_panel = new ControllerPanel(app);
    styleBackground(controller_panel);
    stackedWidget->addWidget(controller_panel);

    shortcuts_panel = new ShortcutsPanel(app);
    styleBackground(shortcuts_panel);
    stackedWidget->addWidget(shortcuts_panel);

    folders_panel = new FoldersPanel(app);
    styleBackground(folders_panel);
    stackedWidget->addWidget(folders_panel);

    stackedWidget->setCurrentIndex(0);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &EmuSettingsWindow::reject);

    connect(panelList, &QListWidget::currentItemChanged, [&](QListWidgetItem *prev, QListWidgetItem *cur) {
        stackedWidget->setCurrentIndex(panelList->currentRow());
    });

    refreshIcons();

    connect(defaultsButton, &QPushButton::clicked, [&](bool) {
        auto section = stackedWidget->currentIndex();
        bool restart_needed = app->config->setDefaults(stackedWidget->currentIndex());
        stackedWidget->currentWidget()->hide();
        stackedWidget->currentWidget()->show();

        if (restart_needed)
        {
            if (section == 1) // Display
                app->window->recreateCanvas();
            else if (section == 2) // Sound
                app->restartAudio();
            else if (section == 4 || section == 5) // Controller Bindings
                app->updateBindings();
        }
        app->updateSettings();
    });

    connect(pushButton_help, &QPushButton::clicked, [&] {
        QWhatsThis::enterWhatsThisMode();
    });
}

void EmuSettingsWindow::show(int page)
{
    panelList->setCurrentRow(page);
    stackedWidget->setCurrentIndex(page);
    if (!isVisible())
    {
        original_config_ = *app->config;
        open();
    }
}

void EmuSettingsWindow::refreshIcons()
{
    auto iconset = app->iconPrefix();
    auto icon = [iconset](const QString &name) -> QIcon { return QIcon(iconset + name); };

    panelList->item(0)->setIcon(icon("settings.svg"));
    panelList->item(1)->setIcon(icon("display.svg"));
    panelList->item(2)->setIcon(icon("sound.svg"));
    panelList->item(3)->setIcon(icon("emulation.svg"));
    panelList->item(4)->setIcon(icon("joypad.svg"));
    panelList->item(5)->setIcon(icon("keyboard.svg"));
    panelList->item(6)->setIcon(icon("folders.svg"));

    controller_panel->refreshIcons();
    shortcuts_panel->refreshIcons();
}

void EmuSettingsWindow::reject()
{
    bool restart_audio = app->config->sound_driver != original_config_.sound_driver ||
                         app->config->playback_rate != original_config_.playback_rate ||
                         app->config->audio_buffer_size_ms != original_config_.audio_buffer_size_ms;
    bool recreate_canvas = app->config->display_driver != original_config_.display_driver ||
                           app->config->display_device_index != original_config_.display_device_index;

    *app->config = original_config_;
    app->updateSettings();
    app->updateBindings();
    if (restart_audio)
        app->restartAudio();
    if (recreate_canvas)
        app->window->recreateCanvas();
    app->window->shaderChanged();
    app->window->refreshLibrary();

    // These are applied live as the user interacts (same as every other
    // setting), but unlike config fields they're external state that the
    // *app->config = original_config_ restore above can't undo by itself.
    EmuTheme::apply(EmuTheme::list()[app->config->theme].first);
    app->window->refreshIcons();
    SDLInputManager::setBackgroundInputEnabled(app->config->background_gamepad_input);
#ifdef _WIN32
    WinFileAssociation::apply(app->config->add_to_registry, EmuApplication::romExtensionsForRegistry());
#endif

    QDialog::reject();
}