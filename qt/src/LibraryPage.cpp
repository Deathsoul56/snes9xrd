#include "LibraryPage.hpp"

#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "EmuGameList.hpp"
#include "GameListWidget.hpp"

#include <QAction>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

LibraryPage::LibraryPage(EmuApplication *app, EmuGameList *game_list, QWidget *parent)
    : QWidget(parent), app_(app), game_list_(game_list)
{
    setObjectName("libraryPage");

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto *header = new QWidget(this);
    header->setObjectName("libraryHeader");
    auto *header_layout = new QHBoxLayout(header);
    header_layout->setContentsMargins(16, 8, 16, 8);

    header_label_ = new QLabel(tr("Library"), header);
    header_label_->setObjectName("libraryTitle");
    header_layout->addWidget(header_label_);

    header_layout->addStretch();

    search_ = new QLineEdit(header);
    search_->setObjectName("librarySearch");
    search_->setPlaceholderText(tr("Search…"));
    search_->setClearButtonEnabled(true);
    search_->setFixedWidth(280);
    header_layout->addWidget(search_);

    root->addWidget(header);

    stack_ = new QStackedWidget(this);
    stack_->setObjectName("libraryStack");

    auto *empty = new QWidget(stack_);
    auto *empty_layout = new QVBoxLayout(empty);
    empty_layout->setAlignment(Qt::AlignCenter);

    auto *drop = new QWidget(empty);
    drop->setObjectName("libraryDropZone");
    drop->setFixedSize(420, 220);
    auto *drop_layout = new QVBoxLayout(drop);
    drop_layout->setContentsMargins(24, 24, 24, 24);
    drop_layout->setSpacing(8);

    auto *title = new QLabel(tr("No games found"), drop);
    title->setObjectName("libraryEmptyTitle");
    title->setAlignment(Qt::AlignCenter);
    drop_layout->addWidget(title);

    auto *hint = new QLabel(
        tr("Add a folder with SNES ROM files (.smc / .sfc) to start."),
        drop);
    hint->setObjectName("libraryEmptyHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setWordWrap(true);
    drop_layout->addWidget(hint);

    auto *add_button = new QPushButton(tr("Add Game Folder…"), drop);
    connect(add_button, &QPushButton::clicked, this, &LibraryPage::onEmptyAddFolders);
    drop_layout->addWidget(add_button, 0, Qt::AlignCenter);

    empty_layout->addWidget(drop, 0, Qt::AlignCenter);
    stack_->addWidget(empty);

    table_widget_ = new GameListWidget(game_list, stack_);
    connect(table_widget_, &GameListWidget::entryActivated,
            this, &LibraryPage::gameEntryActivated);
    stack_->addWidget(table_widget_);

    root->addWidget(stack_, 1);

    connect(search_, &QLineEdit::textChanged,
            this, &LibraryPage::onSearchTextChanged);
    connect(game_list_, &EmuGameList::scanFinished,
            this, &LibraryPage::onGameListChanged);
    connect(game_list_, &EmuGameList::modelReset,
            this, [this] { onGameListChanged(); });

    // Restore the folders scanned last session; without this the library
    // reverts to empty (and the user has to re-add their ROM folder) every
    // time the emulator starts. The scan itself is deferred to the first
    // event-loop iteration: this constructor runs before EmuMainWindow is
    // shown and before qtapp->exec() starts, so scanning here synchronously
    // would block the whole application from appearing at all until the
    // scan finished, looking like a multi-second freeze on startup.
    if (!app_->config->library_folders.empty())
    {
        QStringList folders;
        for (const auto &folder : app_->config->library_folders)
            folders.append(QString::fromStdString(folder));
        game_list_->setFolders(folders);
        QTimer::singleShot(0, this, [this] { refresh(); });
    }

    setShowingEmpty(game_list_->entryCount() == 0);
}

void LibraryPage::refresh()
{
    game_list_->refresh();
    setShowingEmpty(game_list_->entryCount() == 0);
}

void LibraryPage::setShowingEmpty(bool empty)
{
    stack_->setCurrentIndex(empty ? 0 : 1);
    header_label_->setText(empty ? tr("Welcome to snes9x-sex-edition") : tr("Library"));
}

void LibraryPage::onSearchTextChanged(const QString &text)
{
    table_widget_->setFilter(text);
}

void LibraryPage::onGameListChanged()
{
    setShowingEmpty(game_list_->entryCount() == 0);
    if (stack_->currentIndex() == 1)
        table_widget_->applyFilter();
}

void LibraryPage::onEmptyAddFolders()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a ROM folder"));
    if (dir.isEmpty()) return;

    app_->config->library_folders.push_back(dir.toStdString());
    app_->config->last_rom_folder = dir.toStdString();

    reloadFolders();
}

void LibraryPage::reloadFolders()
{
    QStringList folders;
    for (const auto &folder : app_->config->library_folders)
        folders.append(QString::fromStdString(folder));
    game_list_->setFolders(folders);
    refresh();
}
