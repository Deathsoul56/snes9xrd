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

#ifdef Q_OS_WIN
#include <dwmapi.h>
#endif

#include "CheatsDialog.hpp"
#include "EmuApplication.hpp"
#include "EmuBinding.hpp"
#include "EmuCanvasOpenGL.hpp"
#include "EmuCanvasQt.hpp"
#include "EmuCanvasVulkan.hpp"
#include "EmuConfig.hpp"
#include "EmuGameList.hpp"
#include "EmuMainWindow.hpp"
#include "EmuSettingsWindow.hpp"
#include "LibraryPage.hpp"
#include "MultiCartDialog.hpp"
#include "snes9x.h"

#undef KeyPress

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
    setWindowTitle("snes9xrd");
    if (QIcon::hasThemeIcon("snes9x"))
        setWindowIcon(QIcon::fromTheme("snes9x"));
    else
        setWindowIcon(QIcon(":/icons/snes9x.svg"));

#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(winId());
    DWM_WINDOW_CORNER_PREFERENCE cornerPref = DWMWCP_DONOTROUND;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &cornerPref,
                          sizeof(cornerPref));
#endif

    auto iconset = app->iconPrefix();

    // ──────── Menu bar ────────
    auto file_menu = new QMenu(tr("&File"));
    auto open_item = file_menu->addAction(QIcon(iconset + "open.svg"), tr("&Open File…"));
    connect(open_item, &QAction::triggered, this, [&] { openFile(); });

    recent_menu = new QMenu("Recent Files");
    file_menu->addMenu(recent_menu);
    populateRecentlyUsed();

    file_menu->addSeparator();

    load_state_menu = new QMenu(tr("&Load State"));
    save_state_menu = new QMenu(tr("&Save State"));
    for (size_t i = 0; i < state_items_size; i++)
    {
        auto action = load_state_menu->addAction(tr("Slot &%1").arg(i));
        connect(action, &QAction::triggered, [&, i] { app->loadState(i); });
        running_actions_.push_back(action);

        action = save_state_menu->addAction(tr("Slot &%1").arg(i));
        connect(action, &QAction::triggered, [&, i] { app->saveState(i); });
        running_actions_.push_back(action);
    }
    load_state_menu->addSeparator();
    auto load_state_file_item = load_state_menu->addAction(QIcon(iconset + "open.svg"), tr("From &File…"));
    connect(load_state_file_item, &QAction::triggered, [&] { this->chooseState(false); });
    running_actions_.push_back(load_state_file_item);

    load_state_menu->addSeparator();
    auto load_state_undo_item = load_state_menu->addAction(QIcon(iconset + "refresh.svg"), tr("&Undo Load State"));
    connect(load_state_undo_item, &QAction::triggered, [&] { app->loadUndoState(); });
    running_actions_.push_back(load_state_undo_item);

    file_menu->addMenu(load_state_menu);
    save_state_menu->addSeparator();
    auto save_state_file_item = save_state_menu->addAction(QIcon(iconset + "save.svg"), tr("To &File…"));
    connect(save_state_file_item, &QAction::triggered, [&] { this->chooseState(true); });
    running_actions_.push_back(save_state_file_item);
    file_menu->addMenu(save_state_menu);

    file_menu->addSeparator();

    // Load MultiCart — Sufami Turbo / Same Game / etc. needs Slot A and Slot B.
    // BIOS (STBIOS.bin) is resolved from the configured BIOS folder by the
    // core itself, same as the GTK and macOS front-ends.
    auto multicart_item = file_menu->addAction(QIcon(iconset + "open.svg"), tr("Load &MultiCart…"));
    connect(multicart_item, &QAction::triggered, this, [&] {
        MultiCartDialog dlg(this);
        dlg.setWindowTitle(tr("Open MultiCart"));
        if (dlg.exec() != QDialog::Accepted) return;

        if (!app->loadMultiCart(dlg.slotA().toStdString(),
                                dlg.slotB().toStdString()))
        {
            QMessageBox::warning(this, tr("MultiCart"), tr("Failed to load the multicart."));
        }
    });

    // Save / Load Game Position (snes9x's "oops" snapshot used as a safety net).
    auto save_pos_item = file_menu->addAction(tr("Save Game Position"));
    connect(save_pos_item, &QAction::triggered, this, [&] {
        if (!app->saveGamePosition())
            QMessageBox::warning(this, tr("Save Position"), tr("No ROM is currently loaded."));
    });
    auto load_pos_item = file_menu->addAction(tr("Load Game Position"));
    connect(load_pos_item, &QAction::triggered, this, [&] {
        if (!app->loadGamePosition())
            QMessageBox::warning(this, tr("Load Position"), tr("No game position file available."));
    });

    // Save Other → submenu (ROM info, SPC dump)
    auto save_other = file_menu->addMenu(tr("Save &Other"));
    auto rom_info_item = save_other->addAction(tr("ROM &Information…"));
    connect(rom_info_item, &QAction::triggered, this, [&] {
        QMessageBox::information(this, tr("ROM Information"),
                                 QString::fromStdString(app->coreInfo()));
    });
    auto dump_spc_item = save_other->addAction(tr("Dump &SPC…"));
    connect(dump_spc_item, &QAction::triggered, this, [&] {
        if (!app->dumpSpc())
            QMessageBox::warning(this, tr("Dump SPC"), tr("No ROM is currently loaded."));
    });

    // Movies → submenu (record / play / stop)
    auto movies_menu = file_menu->addMenu(tr("&Movie"));
    auto movie_record_item = movies_menu->addAction(tr("&Record…"));
    connect(movie_record_item, &QAction::triggered, this, [&] {
        QString path = QFileDialog::getSaveFileName(this, tr("Record Movie"),
                                                    QString::fromStdString(app->config->last_rom_folder),
                                                    tr("Snes9x Movie (*.smv)"));
        if (path.isEmpty()) return;
        if (!app->startMovieRecord(path.toStdString()))
            QMessageBox::warning(this, tr("Record Movie"), tr("Failed to start recording."));
    });
    auto movie_play_item = movies_menu->addAction(tr("&Play…"));
    connect(movie_play_item, &QAction::triggered, this, [&] {
        QString path = QFileDialog::getOpenFileName(this, tr("Open Movie"),
                                                    QString::fromStdString(app->config->last_rom_folder),
                                                    tr("Snes9x Movie (*.smv)"));
        if (path.isEmpty()) return;
        if (!app->openMovie(path.toStdString()))
            QMessageBox::warning(this, tr("Open Movie"), tr("Failed to open movie."));
    });
    auto movie_stop_item = movies_menu->addAction(tr("&Stop"));
    connect(movie_stop_item, &QAction::triggered, this, [&] { app->stopMovie(); });

    // Reset Game (mirrors the Emulation menu's Reset, kept here for parity).
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
    menuBar()->addMenu(file_menu);

    auto emulation_menu = new QMenu(tr("&Emulation"));
    auto run_item = emulation_menu->addAction(tr("&Run"));
    connect(run_item, &QAction::triggered, [&] {
        if (manual_pause) { manual_pause = false; app->unpause(); }
    });
    running_actions_.push_back(run_item);

    auto pause_item = emulation_menu->addAction(QIcon(iconset + "pause.svg"), tr("&Pause"));
    connect(pause_item, &QAction::triggered, [&] {
        if (!manual_pause) { manual_pause = true; app->pause(); }
    });
    running_actions_.push_back(pause_item);

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

    emulation_menu->addSeparator();

    auto cheats_item = emulation_menu->addAction(tr("&Cheats"));
    connect(cheats_item, &QAction::triggered, [&] {
        if (!cheats_dialog) cheats_dialog = new CheatsDialog(this, app);
        cheats_dialog->show();
    });
    running_actions_.push_back(cheats_item);

    menuBar()->addMenu(emulation_menu);

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

    menuBar()->addMenu(view_menu);

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
    for (int i = 0; i < setting_panels.size(); i++)
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

    menuBar()->addMenu(options_menu);

    auto help_menu = new QMenu(tr("&Help"));
    auto about_item = help_menu->addAction(tr("&About…"));
    connect(about_item, &QAction::triggered, this, [&] {
        QMessageBox::about(this, tr("About Snes9x"),
            tr("Snes9x v%1 for Windows.\n"
               "(c) Copyright 1996 - 2002  Gary Henderson and Jerremy Koot (jkoot@snes9x.com)\n"
               "(c) Copyright 2002 - 2004  Matthew Kendora\n"
               "(c) Copyright 2002 - 2005  Peter Bortas\n"
               "(c) Copyright 2004 - 2005  Joel Yliluoma\n"
               "(c) Copyright 2001 - 2006  John Weidman\n"
               "(c) Copyright 2002 - 2010  Brad Jorsch, funkyass, Kris Bleakley, Nach, zones\n"
               "(c) Copyright 2006 - 2007  nitsuja\n"
               "(c) Copyright 2009 - 2023  BearOso, OV2\n"
               "(c) Copyright 2026 - 2026  DeAtSoUl56\n\n"
               "Windows Port Authors: Matthew Kendora, funkyass, nitsuja, Nach, blip, OV2.\n\n"
               "Snes9x is a Super Nintendo Entertainment System\n"
               "emulator that allows you to play most games designed\n"
               "for the SNES on your PC.\n\n"
               "This is snes9xrd, a fork of Snes9x — sex edition.\n\n"
               "Please visit http://www.snes9x.com for\n"
               "up-to-the-minute information and help on Snes9x.\n\n"
               "Nintendo is a trademark.").arg(QString::fromUtf8(VERSION)));
    });
    menuBar()->addMenu(help_menu);

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

void EmuMainWindow::showLibraryPage()
{
    if (library_page_) library_page_->refresh();
    center_stack_->setCurrentWidget(library_page_);
    menuBar()->setVisible(true);
}

void EmuMainWindow::showRunningPage()
{
    center_stack_->setCurrentWidget(canvas);
    if (isFullScreen()) menuBar()->setVisible(false);
}

void EmuMainWindow::closeCurrentGame()
{
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

        if (!isFullScreen() && app->config->fullscreen_on_open)
            toggleFullscreen();

        mouse_timer.start();
        return true;
    }
    return false;
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
            app->config->main_window_width = ((QResizeEvent *)event)->size().width();
            app->config->main_window_height = ((QResizeEvent *)event)->size().height();
        }
        break;
    case QEvent::WindowActivate:
        if (focus_pause) { focus_pause = false; app->unpause(); }
        break;
    case QEvent::WindowDeactivate:
        if (mouse_grabbed) toggleMouseGrab();
        if (app->config->pause_emulation_when_unfocused && !focus_pause)
        {
            focus_pause = true;
            app->pause();
        }
        break;
    case QEvent::WindowStateChange:
    {
        auto scevent = (QWindowStateChangeEvent *)event;
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
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    {
        if (!mouse_grabbed) break;
        auto mouse_event = (QMouseEvent *)event;
        app->reportMouseButton(mouse_event->button(), event->type() == QEvent::MouseButtonPress);
        break;
    }
    case QEvent::MouseMove:
        if (mouse_grabbed)
        {
            auto center = mapToGlobal(rect().center());
            auto pos = QCursor::pos();
            auto delta = pos - center;
            if (delta.x() == 0 && delta.y() == 0) break;
            app->reportPointer(delta.x(), delta.y());
            QCursor::setPos(center);
        }
        if (!cursor_visible)
        {
            if (canvas && !mouse_grabbed) canvas->setCursor(QCursor(Qt::ArrowCursor));
            cursor_visible = true;
            mouse_timer.start();
        }
        break;
    default:
        break;
    }

    return QMainWindow::event(event);
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
    if (watched == canvas)
    {
        if (event->type() == QEvent::Resize)
        {
            app->emu_thread->runOnThread([&] { canvas->resizeEvent((QResizeEvent *)event); }, true);
            event->accept();
            return true;
        }
        else if (event->type() == QEvent::Paint)
        {
            app->emu_thread->runOnThread([&] { canvas->paintEvent((QPaintEvent *)event); }, true);
            event->accept();
            return true;
        }
    }

    if (event->type() != QEvent::KeyPress && event->type() != QEvent::KeyRelease) return false;
    if (watched != this && watched != canvas && !app->binding_callback) return false;

    auto key_event = (QKeyEvent *)event;

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
    QMessageBox::critical(this, tr("SNES9x Error"), message);
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
