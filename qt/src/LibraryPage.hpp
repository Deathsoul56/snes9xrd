#pragma once

#include <QWidget>
#include <QPointer>

class EmuApplication;
class EmuGameList;
class GameListWidget;
class QStackedWidget;
class QLabel;
class QLineEdit;
class QPushButton;

class LibraryPage : public QWidget
{
    Q_OBJECT

public:
    explicit LibraryPage(EmuApplication *app, EmuGameList *game_list, QWidget *parent = nullptr);
    void refresh();

signals:
    void gameEntryActivated(const QString &path);

private slots:
    void onSearchTextChanged(const QString &text);
    void onGameListChanged();
    void onEmptyAddFolders();

private:
    void setShowingEmpty(bool empty);

    EmuApplication *app_ = nullptr;
    EmuGameList    *game_list_ = nullptr;
    QStackedWidget *stack_ = nullptr;
    GameListWidget *table_widget_ = nullptr;
    QLineEdit      *search_ = nullptr;
    QLabel         *header_label_ = nullptr;
};
