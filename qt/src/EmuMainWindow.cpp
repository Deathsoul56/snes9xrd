#include <QAbstractItemView>
#include <QAction>
#include <QBoxLayout>
#include <QFileDialog>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QStackedWidget>
#include <QStatusBar>
#include <QTimer>
#include <QtEvents>
#include <QGuiApplication>
#include <algorithm>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

#include "CheatsDialog.hpp"
#include "CheatSearchDialog.hpp"
#include "EmuApplication.hpp"
#include "EmuBinding.hpp"
#include "EmuCanvasOpenGL.hpp"
#include "EmuCanvasQt.hpp"
#include "EmuCanvasVulkan.hpp"
#include "EmuConfig.hpp"
#include "EmuGameList.hpp"
#include "HacksDialog.hpp"
#include "EmuMainWindow.hpp"
#include "EmuSettingsWindow.hpp"
#include "LibraryPage.hpp"
#include "MultiCartDialog.hpp"
#include "StatePreviewDialog.hpp"
#include "snes9x.h"

#undef KeyPress

// Single source of truth for the fork's own version, shown in the window
// title and the About dialog. Separate from the upstream Snes9x core's
// VERSION (snes9x.h), which tracks the emulator core, not this frontend fork.
static const char *const kSnes9xrdVersion = "0.3";

static EmuSettingsWindow *g_emu_settings_window = nullptr;

EmuMainWindow::EmuMainWindow(EmuApplication *app)
    : app(app)
{
    createWidgets();

    app->qtapp->installEventFilter(this);
    mouse_timer.setTimerType(Qt::CoarseTimer);
    mouse_timer.setInterval(1000);
    mouse_timer.callOnTimeout([&] {
        if (cursor_visible && isActivelyDrawing())
        {
            if (canvas)
                canvas->setCursor(QCursor(Qt::BlankCursor));
            cursor_visible = false;
            mouse_timer.stop();
        }
    });

    showLibraryPage();
}

EmuMainWindow::~EmuMainWindow() = default;

void EmuMainWindow::destroyCanvas()
{
    if (!canvas) return;
    auto *w = canvas;
    canvas = nullptr;
    w->deinit();
    delete w;
}

bool EmuMainWindow::createCanvas()
{
    auto fallback = [this]() -> bool {
        QMessageBox::warning(
            this, tr("Unable to Start Display Driver"),
            tr("Unable to create a %1 context. Attempting to use qt.")
                .arg(QString::fromUtf8(app->config->display_driver)));
        app->config->display_driver = "qt";
        return createCanvas();
    };

    if (app->config->display_driver != "vulkan" &&
        app->config->display_driver != "opengl" &&
        app->config->display_driver != "qt")
        app->config->display_driver = "qt";

    if (app->config->display_driver == "vulkan")
    {
        canvas = new EmuCanvasVulkan(app->config.get(), this);
        QGuiApplication::processEvents();
        if (!canvas->createContext())
        {
            delete canvas;
            canvas = nullptr;
            return fallback();
        }
    }
    else if (app->config->display_driver == "opengl")
    {
        canvas = new EmuCanvasOpenGL(app->config.get(), this);
        QGuiApplication::processEvents();
        app->emu_thread->runOnThread([&] { canvas->createContext(); }, true);
    }
    else
        canvas = new EmuCanvasQt(app->config.get(), this);

    if (QGuiApplication::platformName() == "wayland")
    {
        auto saved_width = width(), saved_height = height();
        resize(width() + 1, height());
        resize(saved_width, saved_height);
    }

    center_stack_->addWidget(canvas);
    center_stack_->setCurrentWidget(canvas);
    return true;
}

void EmuMainWindow::recreateCanvas()
{
    if (!canvas) return;
    app->suspendThread();
    destroyCanvas();
    createCanvas();
    app->unsuspendThread();
}

void EmuMainWindow::setRunningActionsEnabled(bool enable)
{
    for (auto *a : running_actions_)
        a->setEnabled(enable);
}

void EmuMainWindow::createWidgets()
{
    setWindowTitle(QStringLiteral("snes9xrd v%1").arg(kSnes9xrdVersion));
    if (QIcon::hasThemeIcon("snes9x"))
        setWindowIcon(QIcon::fromTheme("snes9x"));
    else
        setWindowIcon(QIcon(":/icons/snes9x.png"));

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref,
                          sizeof(cornerPref));
#endif

    menuBar()->addMenu(createFileMenu());
    menuBar()->addMenu(createEmulationMenu());
    menuBar()->addMenu(createViewMenu());
    menuBar()->addMenu(createOptionsMenu());
    menuBar()->addMenu(createCheatMenu());
    menuBar()->addMenu(createNetplayMenu());
    menuBar()->addMenu(createHelpMenu());

    createCenterStack();

    status_label_ = new QLabel("", this);
    statusBar()->addPermanentWidget(status_label_, 1);
    statusBar()->setSizeGripEnabled(false);

    if (app->config->main_window_width != 0 && app->config->main_window_height != 0)
        resize(app->config->main_window_width, app->config->main_window_height);

    // Center on the primary screen instead of relying on the window manager's
    // default placement. Harmless if the window ends up maximized right after
    // (main.cpp decides that based on main_window_maximized).
    if (auto *screen = QGuiApplication::primaryScreen())
    {
        QRect avail = screen->availableGeometry();
        move(avail.center().x() - width() / 2, avail.center().y() - height() / 2);
    }

    setRunningActionsEnabled(false);
}

QMenu *EmuMainWindow::createFileMenu()
{
    auto iconset = app->iconPrefix();

    // ──────── Menu bar ────────
    auto file_menu = new QMenu(tr("&File"));
    auto open_item = file_menu->addAction(QIcon(iconset + "open.svg"), tr("&Open File…"));
    connect(open_item, &QAction::triggered, this, [&] { openFile(); });

    recent_menu = new QMenu("Recent Files");
    file_menu->addMenu(recent_menu);
    populateRecentlyUsed();

    file_menu->addSeparator();

    // Load MultiCart — Sufami Turbo / Same Game / etc. needs Slot A and Slot B.
    // BIOS (STBIOS.bin) is resolved from the configured BIOS folder by the
    // core itself, same as the GTK and macOS front-ends.
    auto multicart_item = file_menu->addAction(QIcon(iconset + "open.svg"), tr("Load &MultiCart…"));
    connect(multicart_item, &QAction::triggered, this, [&] {
        MultiCartDialog dlg(app->config.get(), this);
        dlg.setWindowTitle(tr("Open MultiCart"));
        if (dlg.exec() != QDialog::Accepted) return;

        if (!app->loadMultiCart(dlg.slotA().toStdString(),
                                dlg.slotB().toStdString()))
        {
            QMessageBox::warning(this, tr("MultiCart"), tr("Failed to load the multicart."));
            return;
        }

        startRunningGame();
    });

    file_menu->addSeparator();

    auto save_position_menu = new QMenu(tr("Save Game Position"));
    auto load_position_menu = new QMenu(tr("Load Game Position"));

    for (int bank = 0; bank < 10; bank++)
    {
        auto *save_bank = save_position_menu->addMenu(tr("Bank #%1").arg(bank));
        auto *load_bank = load_position_menu->addMenu(tr("Bank #%1").arg(bank));

        for (int slot = 0; slot < 10; slot++)
        {
            int position = bank * 10 + slot;
            auto *save_item = save_bank->addAction(tr("Slot #%1").arg(slot));
            connect(save_item, &QAction::triggered, this, [&, position] {
                if (!app->saveState(position))
                    QMessageBox::warning(this, tr("Save Position"), tr("No ROM is currently loaded."));
            });
            running_actions_.push_back(save_item);

            auto *load_item = load_bank->addAction(tr("Slot #%1").arg(slot));
            connect(load_item, &QAction::triggered, this, [&, position] {
                if (!app->loadState(position))
                    QMessageBox::warning(this, tr("Load Position"), tr("No game position file available."));
            });
            running_actions_.push_back(load_item);
        }
    }

    load_position_menu->addSeparator();
    auto *oops_item = load_position_menu->addAction(tr("Oops File"));
    connect(oops_item, &QAction::triggered, this, [&] {
        if (!app->loadGamePosition())
            QMessageBox::warning(this, tr("Load Position"), tr("No game position file available."));
    });
    running_actions_.push_back(oops_item);

    save_position_menu->addSeparator();
    auto *save_file_item = save_position_menu->addAction(tr("Select File"));
    connect(save_file_item, &QAction::triggered, this, [&] { chooseState(true); });
    running_actions_.push_back(save_file_item);

    load_position_menu->addSeparator();
    auto *load_file_item = load_position_menu->addAction(tr("Select File"));
    connect(load_file_item, &QAction::triggered, this, [&] { chooseState(false); });
    running_actions_.push_back(load_file_item);

    file_menu->addMenu(save_position_menu);
    file_menu->addMenu(load_position_menu);

    auto save_preview_item = file_menu->addAction(tr("Save with Preview"));
    connect(save_preview_item, &QAction::triggered, this, [&] {
        StatePreviewDialog dialog(this, app, true);
        dialog.exec();
    });
    running_actions_.push_back(save_preview_item);

    auto load_preview_item = file_menu->addAction(tr("Load with Preview"));
    connect(load_preview_item, &QAction::triggered, this, [&] {
        StatePreviewDialog dialog(this, app, false);
        dialog.exec();
    });
    running_actions_.push_back(load_preview_item);

    file_menu->addSeparator();

    // Save Other → submenu (SPC dump)
    auto save_other = file_menu->addMenu(tr("Save &Other"));
    save_other->setToolTipsVisible(true);
    auto dump_spc_item = save_other->addAction(tr("Dump &SPC…"));
    connect(dump_spc_item, &QAction::triggered, this, [&] {
        if (!app->dumpSpc())
            QMessageBox::warning(this, tr("Dump SPC"), tr("No ROM is currently loaded."));
    });
    running_actions_.push_back(dump_spc_item);

    auto screenshot_item = save_other->addAction(tr("Save Screenshot"));
    connect(screenshot_item, &QAction::triggered, this, [&] {
        if (!app->takeScreenshot())
            QMessageBox::warning(this, tr("Save Screenshot"), tr("No ROM is currently loaded."));
    });
    running_actions_.push_back(screenshot_item);

    auto save_sram_item = save_other->addAction(tr("Save S-RAM Data"));
    connect(save_sram_item, &QAction::triggered, this, [&] {
        if (!app->saveSram())
            QMessageBox::warning(this, tr("Save S-RAM Data"), tr("No ROM is currently loaded."));
    });
    running_actions_.push_back(save_sram_item);

    auto save_memory_pack_item = save_other->addAction(tr("Save Memory Pack"));
    connect(save_memory_pack_item, &QAction::triggered, this, [&] {
        if (!app->saveMemoryPack())
            QMessageBox::warning(this, tr("Save Memory Pack"), tr("Failed to save the Memory Pack."));
    });
    running_actions_.push_back(save_memory_pack_item);

    connect(save_other, &QMenu::aboutToShow, this, [&, save_memory_pack_item] {
        bool supported = app->canSaveMemoryPack();
        save_memory_pack_item->setEnabled(supported);
        save_memory_pack_item->setToolTip(supported ? QString() : tr("Only available in MultiCart mode."));
    });

    auto rom_info_item = file_menu->addAction(tr("ROM &Information…"));
    connect(rom_info_item, &QAction::triggered, this, [&] {
        QMessageBox::information(this, tr("ROM Information"),
                                 QString::fromStdString(app->coreInfo()));
    });
    running_actions_.push_back(rom_info_item);

    file_menu->addSeparator();
    auto movie_play_item = file_menu->addAction(tr("Movie &Play…"));
    connect(movie_play_item, &QAction::triggered, this, [&] {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Movie"),
                                                    QString::fromStdString(app->getMovieFolder()),
                                                    tr("Snes9x Movie (*.smv)"));
        if (path.isEmpty()) return;
        if (!app->openMovie(path.toStdString()))
            QMessageBox::warning(this, tr("Open Movie"), tr("Failed to open movie."));
    });

    auto movie_record_item = file_menu->addAction(tr("Movie &Record…"));
    connect(movie_record_item, &QAction::triggered, this, [&] {
        QString path = QFileDialog::getSaveFileName(this, tr("Record Movie"),
                                                    QString::fromStdString(app->getMovieFolder()),
                                                    tr("Snes9x Movie (*.smv)"));
        if (path.isEmpty()) return;
        if (!app->startMovieRecord(path.toStdString()))
            QMessageBox::warning(this, tr("Record Movie"), tr("Failed to start recording."));
    });

    auto movie_stop_item = file_menu->addAction(tr("Movie &Stop"));
    connect(movie_stop_item, &QAction::triggered, this, [&] { app->stopMovie(); });

    connect(file_menu, &QMenu::aboutToShow, this, [&, movie_record_item, movie_play_item, movie_stop_item] {
        bool active = app->isMovieActive();
        movie_record_item->setEnabled(canvas && !active);
        movie_play_item->setEnabled(canvas && !active);
        movie_stop_item->setEnabled(canvas && active);
    });

    file_menu->addSeparator();
    auto avi_recording_item = file_menu->addAction(tr("Start AVI &Recording…"));
    connect(avi_recording_item, &QAction::triggered, this, [&] {
        if (app->isAviRecording())
        {
            app->stopAviRecording();
            return;
        }

        QString path = QFileDialog::getSaveFileName(this, tr("Start AVI Recording"),
                                                    QString::fromStdString(app->config->last_rom_folder),
                                                    tr("AVI Video (*.avi)"));
        if (path.isEmpty()) return;
        if (!app->startAviRecording(path.toStdString()))
            QMessageBox::warning(this, tr("AVI Recording"), tr("Failed to start AVI recording."));
    });

    connect(file_menu, &QMenu::aboutToShow, this, [&, avi_recording_item] {
        bool recording = app->isAviRecording();
        avi_recording_item->setText(recording ? tr("Stop AVI Recording") : tr("Start AVI &Recording…"));
        avi_recording_item->setEnabled(canvas);
    });

    file_menu->addSeparator();

    auto file_reset_item = file_menu->addAction(QIcon(iconset + "refresh.svg"), tr("&Reset Game"));
    connect(file_reset_item, &QAction::triggered, this, [&] {
        app->reset();
        if (manual_pause) { manual_pause = false; app->unpause(); }
    });

    auto close_game_item = file_menu->addAction(QIcon(iconset + "exit.svg"), tr("&Close Game"));
    connect(close_game_item, &QAction::triggered, this, [&] { closeCurrentGame(); });
    running_actions_.push_back(close_game_item);

    auto exit_item = new QAction(QIcon(iconset + "exit.svg"), tr("E&xit"));
    connect(exit_item, &QAction::triggered, this, [&](bool) { close(); });
    file_menu->addAction(exit_item);

    return file_menu;
}

QMenu *EmuMainWindow::createEmulationMenu()
{
    auto iconset = app->iconPrefix();

    auto emulation_menu = new QMenu(tr("&Emulation"));
    pause_item_ = emulation_menu->addAction(QIcon(iconset + "pause.svg"), tr("&Pause"));
    connect(pause_item_, &QAction::triggered, this, [&] { pauseContinue(); });
    running_actions_.push_back(pause_item_);

    connect(emulation_menu, &QMenu::aboutToShow, this, [&] {
        updatePauseMenuItem();
    });

    auto pause_when_inactive_item = emulation_menu->addAction(tr("Pause &When Inactive"));
    pause_when_inactive_item->setCheckable(true);
    pause_when_inactive_item->setChecked(app->config->pause_emulation_when_unfocused);
    connect(pause_when_inactive_item, &QAction::toggled, this, [&](bool checked) {
        app->config->pause_emulation_when_unfocused = checked;
    });

    auto frame_advance_item = emulation_menu->addAction(tr("&Frame Advance"));
    connect(frame_advance_item, &QAction::triggered, this, [&] { frameAdvance(); });
    running_actions_.push_back(frame_advance_item);

    emulation_menu->addSeparator();

    auto hacks_item = emulation_menu->addAction(tr("&Hacks…"));
    connect(hacks_item, &QAction::triggered, this, [&] {
        auto result = QMessageBox::warning(this, tr("Warning: Unsupported"),
                                           tr("The settings in this dialog should only be used for\n"
                                              "compatibility with old ROM hacks or if you otherwise know\n"
                                              "what you're doing.\n\n"
                                              "If any problems occur, click 'Set Defaults' to reset the options\n"
                                              "to normal."),
                                           QMessageBox::Ok | QMessageBox::Cancel);
        if (result != QMessageBox::Ok)
            return;

        HacksDialog dialog(this, app);
        dialog.exec();
    });
    running_actions_.push_back(hacks_item);

    emulation_menu->addSeparator();

    auto reset_item = emulation_menu->addAction(QIcon(iconset + "refresh.svg"), tr("Rese&t"));
    connect(reset_item, &QAction::triggered, [&] {
        app->reset();
        if (manual_pause) { manual_pause = false; app->unpause(); }
    });
    running_actions_.push_back(reset_item);

    auto hard_reset_item = emulation_menu->addAction(QIcon(iconset + "reset.svg"), tr("&Hard Reset"));
    connect(hard_reset_item, &QAction::triggered, [&] {
        app->powerCycle();
        if (manual_pause) { manual_pause = false; app->unpause(); }
    });
    running_actions_.push_back(hard_reset_item);

    return emulation_menu;
}

QMenu *EmuMainWindow::createViewMenu()
{
    auto iconset = app->iconPrefix();

    auto view_menu = new QMenu(tr("&View"));
    auto set_size_menu = new QMenu(tr("&Set Size"));
    for (size_t i = 1; i <= 10; i++)
    {
        auto label = (i == 10) ? tr("1&0x") : tr("&%1x").arg(i);
        auto item = set_size_menu->addAction(label);
        connect(item, &QAction::triggered, this, [&, i](bool) { resizeToMultiple(i); });
    }
    view_menu->addMenu(set_size_menu);
    view_menu->addSeparator();

    auto fullscreen_item = new QAction(QIcon(iconset + "fullscreen.svg"), tr("&Fullscreen"));
    view_menu->addAction(fullscreen_item);
    connect(fullscreen_item, &QAction::triggered, [&](bool) { toggleFullscreen(); });
    running_actions_.push_back(fullscreen_item);

    return view_menu;
}

QMenu *EmuMainWindow::createCheatMenu()
{
    auto cheat_menu = new QMenu(tr("&Cheat"));

    auto editor_item = cheat_menu->addAction(tr("&Game Genie, Pro-Action Replay Codes"));
    connect(editor_item, &QAction::triggered, this, [this] { showCheatsDialog(); });
    running_actions_.push_back(editor_item);

    auto search_item = cheat_menu->addAction(tr("&Search for New Cheats"));
    connect(search_item, &QAction::triggered, this, [this] { showCheatSearchDialog(); });
    running_actions_.push_back(search_item);

    auto apply_item = cheat_menu->addAction(tr("&Apply Cheats"));
    apply_item->setCheckable(true);
    connect(apply_item, &QAction::toggled, this, [this](bool enabled) {
        app->setCheatsEnabled(enabled);
    });
    connect(cheat_menu, &QMenu::aboutToShow, this, [this, apply_item] {
        apply_item->setChecked(app->config->apply_cheats);
    });
    running_actions_.push_back(apply_item);

    return cheat_menu;
}

QMenu *EmuMainWindow::createNetplayMenu()
{
    auto netplay_menu = new QMenu(tr("&Netplay"));
    const auto unavailable = tr("Netplay is not available in the Qt frontend.");
    auto add_item = [&](const QString &text, bool checkable = false, bool checked = false) {
        auto *item = netplay_menu->addAction(text);
        item->setCheckable(checkable);
        item->setChecked(checked);
        item->setEnabled(false);
        item->setToolTip(unavailable);
    };

    add_item(tr("&Connect to Server…"));
    add_item(tr("&Disconnect from Server"));
    netplay_menu->addSeparator();
    add_item(tr("&Act as Server"));
    add_item(tr("&Re-sync all Clients Using Freeze File Now"));
    add_item(tr("&Send ROM Image to Clients Now"));
    add_item(tr("S&end ROM Image to Clients"), true);
    add_item(tr("S&ync Using Reset Game"), true, true);
    netplay_menu->addSeparator();
    add_item(tr("&Options…"));

    return netplay_menu;
}

QMenu *EmuMainWindow::createOptionsMenu()
{
    auto iconset = app->iconPrefix();

    auto options_menu = new QMenu(tr("&Options"));
    std::array<QString, 7> setting_panels = { tr("&General…"),
                                              tr("&Display…"),
                                              tr("&Sound…"),
                                              tr("&Emulation…"),
                                              tr("&Controllers…"),
                                              tr("Shortcu&ts…"),
                                              tr("&Files…") };
    const char *setting_icons[] = { "settings.svg", "display.svg", "sound.svg",
                                    "emulation.svg", "joypad.svg",
                                    "keyboard.svg", "folders.svg" };
    for (size_t i = 0; i < setting_panels.size(); i++)
    {
        auto action = options_menu->addAction(QIcon(iconset + setting_icons[i]), setting_panels[i]);
        QObject::connect(action, &QAction::triggered, [&, i] {
            if (!g_emu_settings_window)
                g_emu_settings_window = new EmuSettingsWindow(this, app);
            g_emu_settings_window->show(i);
        });
    }
    options_menu->addSeparator();
    auto shader_settings_item = new QAction(QIcon(iconset + "shader.svg"), tr("S&hader Settings…"));
    QObject::connect(shader_settings_item, &QAction::triggered, [&] {
        if (canvas) canvas->showParametersDialog();
    });
    options_menu->addAction(shader_settings_item);

    return options_menu;
}

QMenu *EmuMainWindow::createHelpMenu()
{
    auto help_menu = new QMenu(tr("&Help"));
    auto about_item = help_menu->addAction(tr("&About…"));
    connect(about_item, &QAction::triggered, this, [&] {
        QMessageBox::about(this, tr("About snes9xrd"),
            tr("Snes9x v%1 for Windows.<br>"
               "(c) Copyright 1996 - 2002  Gary Henderson and Jerremy Koot (jkoot@snes9x.com)<br>"
               "(c) Copyright 2002 - 2004  Matthew Kendora<br>"
               "(c) Copyright 2002 - 2005  Peter Bortas<br>"
               "(c) Copyright 2004 - 2005  Joel Yliluoma<br>"
               "(c) Copyright 2001 - 2006  John Weidman<br>"
               "(c) Copyright 2002 - 2010  Brad Jorsch, funkyass, Kris Bleakley, Nach, zones<br>"
               "(c) Copyright 2006 - 2007  nitsuja<br>"
               "(c) Copyright 2009 - 2023  BearOso, OV2<br>"
               "(c) Copyright 2026 - 2026  DeAtSoUl56<br><br>"
               "Windows Port Authors: Matthew Kendora, funkyass, nitsuja, Nach, blip, OV2.<br><br>"
               "Snes9x is a Super Nintendo Entertainment System<br>"
               "emulator that allows you to play most games designed<br>"
               "for the SNES on your PC.<br><br>"
               "This is snes9xrd v%2 <s>sex edition</s>, a fork of Snes9x.<br><br>"
               "Please visit http://www.snes9x.com for<br>"
               "up-to-the-minute information and help on Snes9x.<br><br>"
               "Nintendo is a trademark.").arg(QString::fromUtf8(VERSION), kSnes9xrdVersion));
    });

    return help_menu;
}

void EmuMainWindow::createCenterStack()
{
    // ──────── Center stack ────────
    center_stack_ = new QStackedWidget(this);
    game_list_ = new EmuGameList(this);
    library_page_ = new LibraryPage(app, game_list_, center_stack_);
    center_stack_->addWidget(library_page_);

    setCentralWidget(center_stack_);

    connect(library_page_, &LibraryPage::gameEntryActivated,
            this, [this](const QString &path) {
        openFile(path.toStdString());
    });
}

void EmuMainWindow::showLibraryPage()
{
    if (library_page_) library_page_->refresh();
    center_stack_->setCurrentWidget(library_page_);
    menuBar()->setVisible(true);
}

void EmuMainWindow::refreshLibrary()
{
    if (library_page_) library_page_->reloadFolders();
}

void EmuMainWindow::showRunningPage()
{
    center_stack_->setCurrentWidget(canvas);
    if (isFullScreen()) menuBar()->setVisible(false);
}

void EmuMainWindow::closeCurrentGame()
{
    if (mouse_grabbed) toggleMouseGrab();

    app->suspendThread();
    app->pause();
    app->closeCurrentGame();
    app->unsuspendThread();

    destroyCanvas();
    setRunningActionsEnabled(false);
    manual_pause = false;
    showLibraryPage();
}

void EmuMainWindow::resizeToMultiple(int multiple)
{
    double hidpi_height = 224 / devicePixelRatioF();
    resize((hidpi_height * multiple) * app->config->aspect_ratio_numerator / app->config->aspect_ratio_denominator,
           (hidpi_height * multiple) + menuBar()->height());
}

void EmuMainWindow::setBypassCompositor(bool bypass)
{
#ifndef _WIN32
    if (QGuiApplication::platformName() == "xcb")
    {
        uint32_t value = bypass;
        auto iface = app->qtapp->nativeInterface<QNativeInterface::QX11Application>();
        auto display = iface->display();
        auto xid = winId();
        Atom net_wm_bypass_compositor = XInternAtom(display, "_NET_WM_BYPASS_COMPOSITOR", False);
        XChangeProperty(display, xid, net_wm_bypass_compositor, 6, 32,
                        PropModeReplace, (unsigned char *)&value, 1);
    }
#endif
}

void EmuMainWindow::chooseState(bool save)
{
    app->pause();

    QFileDialog dialog(this, tr("Choose a State File"));
    dialog.setDirectory(QString::fromStdString(app->getStateFolder()));
    dialog.setNameFilters({ tr("Save States (*.sst *.oops *.undo *.0?? *.1?? *.2?? *.3?? *.4?? *.5?? *.6?? *.7?? *.8?? *.9*)"),
                            tr("All Files (*)") });

    if (!save) dialog.setFileMode(QFileDialog::ExistingFile);
    else { dialog.setFileMode(QFileDialog::AnyFile); dialog.setAcceptMode(QFileDialog::AcceptSave); }

    if (!dialog.exec() || dialog.selectedFiles().empty()) { app->unpause(); return; }

    auto filename = dialog.selectedFiles()[0];
    if (!save) app->loadState(filename.toStdString());
    else        app->saveState(filename.toStdString());

    app->unpause();
}

void EmuMainWindow::openFile()
{
    app->pause();
    QFileDialog dialog(this, tr("Open a ROM File"));
    dialog.setFileMode(QFileDialog::ExistingFile);
    dialog.setDirectory(QString::fromStdString(app->config->last_rom_folder));
    dialog.setNameFilters({ EmuApplication::romFileDialogFilter(),
                            tr("All Files (*)") });

    if (!dialog.exec() || dialog.selectedFiles().empty()) { app->unpause(); return; }

    auto filename = dialog.selectedFiles()[0];
    app->config->last_rom_folder = dialog.directory().canonicalPath().toStdString();
    openFile(filename.toStdString());
    app->unpause();
}

bool EmuMainWindow::openFile(const std::string &filename)
{
    if (app->openFile(filename))
    {
        auto &ru = app->config->recently_used;
        auto it = std::ranges::find(ru, filename);
        if (it != ru.end()) ru.erase(it);
        ru.insert(ru.begin(), filename);
        populateRecentlyUsed();

        return startRunningGame();
    }
    return false;
}

bool EmuMainWindow::startRunningGame()
{
    setRunningActionsEnabled(true);

    if (!canvas)
    {
        if (!createCanvas())
        {
            closeCurrentGame();
            return false;
        }
    }

    QApplication::sync();
    app->startGame();
    showRunningPage();
    autoGrabMouseIfNeeded();

    if (!isFullScreen() && app->config->fullscreen_on_open)
        toggleFullscreen();

    mouse_timer.start();
    return true;
}

void EmuMainWindow::populateRecentlyUsed()
{
    if (!recent_menu) return;
    recent_menu->clear();

    if (app->config->recently_used.empty())
    {
        auto action = recent_menu->addAction(tr("No recent files"));
        action->setDisabled(true);
        return;
    }

    while (app->config->recently_used.size() > recent_menu_size)
        app->config->recently_used.pop_back();

    for (int i = 0; i < static_cast<int>(app->config->recently_used.size()); i++)
    {
        auto &string = app->config->recently_used[i];
        auto action = recent_menu->addAction(QString("&%1: %2").arg(i)
            .arg(QDir::toNativeSeparators(QString::fromStdString(string))));
        connect(action, &QAction::triggered, this, [&, string] { openFile(string); });
        recent_menu_items.push_back(action);
    }
    recent_menu->addSeparator();
    auto action = recent_menu->addAction(tr("Clear Recent Files"));
    connect(action, &QAction::triggered, [&] {
        app->config->recently_used.clear();
        populateRecentlyUsed();
    });
}

#undef KeyPress
#undef KeyRelease
bool EmuMainWindow::event(QEvent *event)
{
    switch (event->type())
    {
    case QEvent::Close:
        app->suspendThread();
        if (isFullScreen()) toggleFullscreen();
        QGuiApplication::processEvents();
        QGuiApplication::sync();
        app->stopThread();
        if (canvas) canvas->deinit();
        QGuiApplication::sync();
        event->accept();
        break;
    case QEvent::Resize:
        if (!isFullScreen() && !isMaximized())
        {
            app->config->main_window_width = static_cast<QResizeEvent *>(event)->size().width();
            app->config->main_window_height = static_cast<QResizeEvent *>(event)->size().height();
        }
        break;
    case QEvent::WindowActivate:
        if (focus_pause) { focus_pause = false; app->unpause(); }
        break;
    case QEvent::WindowDeactivate:
        if (mouse_grabbed) toggleMouseGrab();
        // Only treat this as "the user switched away from Snes9x" (and thus
        // auto-pause) if the whole application actually lost focus, e.g. to
        // another program. Qt also fires WindowDeactivate on this window
        // whenever one of our *own* top-level windows takes focus instead
        // (Options, About, Cheats, etc.), in which case applicationState()
        // stays Qt::ApplicationActive. Without this check, simply opening
        // the Display settings dialog would auto-pause the game, making
        // every setting look like it "does nothing" until the dialog is
        // closed and the main window reactivates.
        if (app->config->pause_emulation_when_unfocused && !focus_pause &&
            QGuiApplication::applicationState() != Qt::ApplicationActive)
        {
            focus_pause = true;
            app->pause();
        }
        break;
    case QEvent::WindowStateChange:
    {
        auto scevent = static_cast<QWindowStateChangeEvent *>(event);
        if (!(scevent->oldState() & Qt::WindowMinimized) && windowState() & Qt::WindowMinimized)
        {
            minimized_pause = true;
            app->pause();
        }
        else if (minimized_pause && !(windowState() & Qt::WindowMinimized))
        {
            minimized_pause = false;
            app->unpause();
        }

        // Remember whether the window is maximized so it can be restored on
        // next launch. Ignore fullscreen/minimized states so toggling those
        // doesn't clobber the last real windowed/maximized state.
        if (!isFullScreen() && !(windowState() & Qt::WindowMinimized))
            app->config->main_window_maximized = isMaximized();
        break;
    }
    default:
        break;
    }

    return QMainWindow::event(event);
}

// Mouse input for the canvas is handled from eventFilter() (see below), not
// from here: QMouseEvent/QEvent::MouseMove sent to the canvas child widget
// are never forwarded to this top-level widget's event(), so hooking them
// here would silently never fire. eventFilter() is installed application-wide
// and already reliably intercepts canvas events (see the Resize/Paint
// handling for `watched == canvas`), so it's the correct place for this too.
void EmuMainWindow::handleCanvasMouseButton(QMouseEvent *mouse_event, bool pressed)
{
    // The SNES Mouse only reports button state while the pointer is
    // captured (mouse_grabbed). The Superscope is a lightgun: it has no
    // capture step, so its Fire/Cursor buttons must work as soon as the
    // player clicks over the canvas.
    bool is_superscope = app->config->port_configuration == EmuConfig::eSuperScopePlusController;
    if (!mouse_grabbed && !is_superscope) return;

    // Snes9xController::reportMouseButton() expects a sequential 1/2/3
    // index (matching EmuBinding::MOUSE_BUTTON1/2/3), not Qt's bitmask
    // Qt::MouseButton values (Left=1, Right=2, Middle=4). Left/Right happen
    // to line up by coincidence, but Middle (4) does not -- translate it
    // explicitly instead of passing the raw button value through.
    int button;
    switch (mouse_event->button())
    {
    case Qt::LeftButton:   button = 1; break;
    case Qt::RightButton:  button = 2; break;
    case Qt::MiddleButton: button = 3; break;
    default: return;
    }
    app->reportMouseButton(button, pressed);

    // Mouse1 L/R is intentionally NOT mapped by reportMouseButton() (see
    // Snes9xController::updateBindings) -- it's only reachable through this
    // config-driven path, so clearing/rebinding Click L/R actually changes
    // whether the physical click does anything to the SNES Mouse.
    app->reportBinding(EmuBinding::mouse_click(button), pressed);
}

void EmuMainWindow::handleCanvasMouseMove()
{
    if (app->config->port_configuration == EmuConfig::eSuperScopePlusController)
    {
        // Superscope aiming is absolute, not a relative delta: the
        // crosshair must track wherever the cursor actually is over the
        // rendered image, like a real lightgun. No mouse grab/capture is
        // needed or wanted here. The OS cursor itself is blanked (only
        // while it's over the canvas) since the in-game crosshair already
        // shows the aim point.
        if (canvas)
            canvas->setCursor(QCursor(Qt::BlankCursor));

        if (canvas && canvas->ready())
        {
            auto local = canvas->mapFromGlobal(QCursor::pos());
            auto image_rect = canvas->applyAspect(canvas->rect());
            if (image_rect.width() > 0 && image_rect.height() > 0)
            {
                double fx = (local.x() - image_rect.x()) / (double)image_rect.width();
                double fy = (local.y() - image_rect.y()) / (double)image_rect.height();
                fx = std::clamp(fx, 0.0, 1.0);
                fy = std::clamp(fy, 0.0, 1.0);
                int height = app->config->show_overscan ? 239 : 224;
                app->reportAbsolutePointer((int)(fx * 255), (int)(fy * (height - 1)));
            }
        }
        return;
    }
    else if (mouse_grabbed)
    {
        auto center = mapToGlobal(rect().center());
        auto pos = QCursor::pos();
        auto delta = pos - center;
        if (delta.x() == 0 && delta.y() == 0) return;
        app->reportPointer(delta.x(), delta.y());
        QCursor::setPos(center);
    }

    if (!cursor_visible)
    {
        if (canvas && !mouse_grabbed) canvas->setCursor(QCursor(Qt::ArrowCursor));
        cursor_visible = true;
        mouse_timer.start();
    }
}

void EmuMainWindow::toggleFullscreen()
{
    if (isFullScreen())
    {
        if (app->config->adjust_for_vrr)
        {
            app->config->setVRRConfig(false);
            app->updateSettings();
        }
        setBypassCompositor(false);
        showNormal();
        menuBar()->setVisible(true);
    }
    else
    {
        if (app->config->adjust_for_vrr)
        {
            app->config->setVRRConfig(true);
            app->updateSettings();
        }
        QCursor::setPos(mapToGlobal(rect().center()));
        showFullScreen();
        menuBar()->setVisible(false);
        setBypassCompositor(true);
    }
}

bool EmuMainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // Alt (and Ctrl+Alt) combos would otherwise get intercepted as a menu
    // mnemonic/shortcut before ever becoming a normal KeyPress -- Qt asks
    // via ShortcutOverride first, and accepting it here tells Qt "this key
    // is ours", so the real KeyPress still follows and reaches the capture
    // logic below.
    if (event->type() == QEvent::ShortcutOverride && app->binding_callback)
    {
        event->accept();
        return true;
    }

    // Lets the mouse's physical buttons themselves be captured as a binding
    // (e.g. for the SNES Mouse's Click L/R table), the same way KeyPress is
    // captured globally below. Must run before the canvas-specific handling
    // and before the click reaches whatever widget it landed on (a table
    // cell, say), so it doesn't also get treated as a normal UI click.
    if (event->type() == QEvent::MouseButtonPress && app->binding_callback)
    {
        auto *mouse_event = static_cast<QMouseEvent *>(event);
        int button;
        switch (mouse_event->button())
        {
        case Qt::LeftButton:   button = 1; break;
        case Qt::RightButton:  button = 2; break;
        case Qt::MiddleButton: button = 3; break;
        default: return false;
        }
        app->reportBinding(EmuBinding::mouse_click(button), true);
        event->accept();
        return true;
    }

    if (watched == canvas)
    {
        if (event->type() == QEvent::Resize)
        {
            app->emu_thread->runOnThread([&] { canvas->resizeEvent(static_cast<QResizeEvent *>(event)); }, true);
            event->accept();
            return true;
        }
        else if (event->type() == QEvent::Paint)
        {
            app->emu_thread->runOnThread([&] { canvas->paintEvent(static_cast<QPaintEvent *>(event)); }, true);
            event->accept();
            return true;
        }
        else if (event->type() == QEvent::MouseMove)
        {
            handleCanvasMouseMove();
            return false;
        }
        else if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonRelease)
        {
            handleCanvasMouseButton(static_cast<QMouseEvent *>(event), event->type() == QEvent::MouseButtonPress);
            return false;
        }
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) return false;
    if (watched != this && watched != canvas && !app->binding_callback) return false;

    auto key_event = static_cast<QKeyEvent *>(event);

    if (mouse_grabbed && key_event->key() == Qt::Key_Escape && event->type() == QEvent::KeyPress)
    {
        toggleMouseGrab();
        return true;
    }
    if (isFullScreen() && key_event->key() == Qt::Key_Escape && event->type() == QEvent::KeyPress)
    {
        toggleFullscreen();
        return true;
    }

    // While capturing a Shortcuts binding, a bare modifier press shouldn't
    // finalize the binding by itself -- hold it so the next key can combine
    // with it (e.g. Ctrl+G). Controller/mouse bindings don't opt into this,
    // so a bare modifier there still finalizes as its own single binding.
    if (app->binding_callback && app->binding_allow_combos && event->type() == QEvent::KeyPress)
    {
        switch (key_event->key())
        {
        case Qt::Key_Control:
        case Qt::Key_Shift:
        case Qt::Key_Alt:
        case Qt::Key_AltGr:
        case Qt::Key_Meta:
        case Qt::Key_Super_L:
        case Qt::Key_Super_R:
            event->accept();
            return true;
        default:
            break;
        }
    }

    auto binding = EmuBinding::keyboard(key_event->key(),
                                        key_event->modifiers().testFlag(Qt::ShiftModifier),
                                        key_event->modifiers().testFlag(Qt::AltModifier),
                                        key_event->modifiers().testFlag(Qt::ControlModifier),
                                        key_event->modifiers().testFlag(Qt::MetaModifier));

    if ((app->isBound(binding) || app->binding_callback) && !key_event->isAutoRepeat())
    {
        app->reportBinding(binding, event->type() == QEvent::KeyPress);
        event->accept();
        return true;
    }
    return false;
}

std::vector<std::string> EmuMainWindow::getDisplayDeviceList()
{
    if (!canvas) return { "Default" };
    return canvas->getDeviceList();
}

void EmuMainWindow::pauseContinue()
{
    if (manual_pause)
    {
        manual_pause = false;
        app->unpause();
    }
    else
    {
        manual_pause = true;
        app->pause();
        if (canvas) canvas->paintEvent(nullptr);
    }

    updatePauseMenuItem();
}

void EmuMainWindow::updatePauseMenuItem()
{
    if (pause_item_)
        pause_item_->setText(Settings.Paused ? tr("&Pause\t✓") : tr("&Pause"));
}

bool EmuMainWindow::isActivelyDrawing()
{
    return (!app->isPaused() && app->isCoreActive());
}

void EmuMainWindow::output(uint8_t *buffer, int width, int height, QImage::Format format, int bytes_per_line, double frame_rate)
{
    if (canvas) canvas->output(buffer, width, height, format, bytes_per_line, frame_rate);
}

void EmuMainWindow::showCoreError(const QString &message)
{
    QMessageBox::critical(this, tr("snes9xrd Error"), message);
}

void EmuMainWindow::recreateUIAssets()
{
    app->emu_thread->runOnThread([&] { if (canvas) canvas->recreateUIAssets(); }, true);
}

void EmuMainWindow::shaderChanged()
{
    app->emu_thread->runOnThread([&] { if (canvas) canvas->shaderChanged(); });
}

void EmuMainWindow::gameChanging()
{
    if (cheats_dialog) cheats_dialog->close();
    if (cheat_search_dialog) cheat_search_dialog->close();
}

void EmuMainWindow::toggleMouseGrab()
{
    mouse_grabbed = !mouse_grabbed;
    if (mouse_grabbed)
    {
        canvas->setCursor(QCursor(Qt::BlankCursor));
        QCursor::setPos(mapToGlobal(rect().center()));
    }
    else
    {
        canvas->setCursor(QCursor(Qt::ArrowCursor));
    }
}

void EmuMainWindow::autoGrabMouseIfNeeded()
{
    if (canvas && !mouse_grabbed && app->config->port_configuration == EmuConfig::eMousePlusController)
        toggleMouseGrab();
}

void EmuMainWindow::frameAdvance()
{
    if (!manual_pause)
    {
        manual_pause = true;
        app->pause();
    }
    app->advanceFrame();
}

void EmuMainWindow::showCheatsDialog()
{
    if (!cheats_dialog) cheats_dialog = new CheatsDialog(this, app);
    cheats_dialog->show();
    cheats_dialog->raise();
    cheats_dialog->activateWindow();
}

void EmuMainWindow::showCheatSearchDialog()
{
    if (!cheat_search_dialog) cheat_search_dialog = new CheatSearchDialog(this, app);
    cheat_search_dialog->show();
    cheat_search_dialog->raise();
    cheat_search_dialog->activateWindow();
}
