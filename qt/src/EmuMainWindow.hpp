#ifndef __EMU_MAIN_WINDOW_HPP
#define __EMU_MAIN_WINDOW_HPP

#include <QMainWindow>
#include <QPointer>
#include <QTimer>
#include "EmuCanvas.hpp"

class EmuApplication;
class CheatsDialog;
class EmuGameList;
class LibraryPage;
class QStackedWidget;
class QLabel;

class EmuMainWindow : public QMainWindow
{
    Q_OBJECT

  public Q_SLOTS:
    void output(uint8_t *buffer, int width, int height, QImage::Format format, int bytes_per_line, double frame_rate);
    // Surface a core-side error message (e.g. "multicart failed: STBIOS.bin not found")
    // as a modal dialog. Triggered from S9xMessage via Qt::QueuedConnection.
    void showCoreError(const QString &message);

  public:
    EmuMainWindow(EmuApplication *app);
    ~EmuMainWindow();

    void toggleFullscreen();
    bool createCanvas();
    void destroyCanvas();
    void recreateCanvas();
    void setBypassCompositor(bool);
    void setRunningActionsEnabled(bool);
    bool event(QEvent *event) override;
    bool eventFilter(QObject *, QEvent *event) override;
    void resizeToMultiple(int multiple);
    void populateRecentlyUsed();
    void chooseState(bool save);
    void pauseContinue();
    bool isActivelyDrawing();
    void openFile();
    bool openFile(const std::string &filename);
    void closeCurrentGame();
    void recreateUIAssets();
    void shaderChanged();
    void gameChanging();
    void toggleMouseGrab();
    std::vector<std::string> getDisplayDeviceList();

    EmuApplication *app = nullptr;
    EmuCanvas *canvas = nullptr;

  private:
    void createWidgets();
    void showLibraryPage();
    void showRunningPage();

    QPointer<CheatsDialog> cheats_dialog;

    bool manual_pause = false;
    bool focus_pause = false;
    bool minimized_pause = false;
    bool mouse_grabbed = false;

    QMenu *load_state_menu;
    QMenu *save_state_menu;
    QMenu *recent_menu;

    QTimer mouse_timer;
    bool cursor_visible = true;

    static const size_t recent_menu_size = 10;
    static const size_t state_items_size = 10;

    std::vector<QAction *> recent_menu_items;

    // Center
    LibraryPage  *library_page_ = nullptr;
    EmuGameList  *game_list_ = nullptr;
    QStackedWidget *center_stack_ = nullptr;
    QLabel       *status_label_ = nullptr;

    std::vector<QAction *> running_actions_;
};

#endif
