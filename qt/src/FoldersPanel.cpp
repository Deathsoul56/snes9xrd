#include "FoldersPanel.hpp"
#include "EmuConfig.hpp"
#include "EmuMainWindow.hpp"
#include <QFileDialog>
#include <QDesktopServices>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

FoldersPanel::FoldersPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    connectEntry(comboBox_sram, lineEdit_sram, pushButton_sram, &app->config->sram_location, &app->config->sram_folder);
    connectEntry(comboBox_state, lineEdit_state, pushButton_state, &app->config->state_location, &app->config->state_folder);
    connectEntry(comboBox_cheat, lineEdit_cheat, pushButton_cheat, &app->config->cheat_location, &app->config->cheat_folder);
    connectEntry(comboBox_patch, lineEdit_patch, pushButton_patch, &app->config->patch_location, &app->config->patch_folder);
    connectEntry(comboBox_export, lineEdit_export, pushButton_export, &app->config->export_location, &app->config->export_folder);

    // Append a BIOS folder row to the existing data-locations group. The
    // snes9x core hard-codes a filename lookup for Sufami Turbo / BS-X
    // (STBIOS.bin / BS-X.bin) in this directory, so this is required for
    // multicart loading.
    if (auto *group = findChild<QGroupBox *>("groupBox"))
    {
        if (auto *grid = group->findChild<QGridLayout *>("gridLayout"))
        {
            int row = grid->rowCount();
            auto *label = new QLabel(tr("BIOS"), group);
            bios_combo_  = new QComboBox(group);
            bios_combo_->addItem(tr("ROM Folder"));
            bios_combo_->addItem(tr("Config Folder"));
            bios_combo_->addItem(tr("Custom Folder"));
            bios_line_   = new QLineEdit(group);
            bios_line_->setReadOnly(true);
            bios_button_ = new QPushButton(tr("Browse..."), group);
            grid->addWidget(label, row, 0, 1, 2);
            grid->addWidget(bios_combo_, row, 2);
            grid->addWidget(bios_line_, row, 3);
            grid->addWidget(bios_button_, row, 4);
            connectEntry(bios_combo_, bios_line_, bios_button_,
                         &app->config->bios_location, &app->config->bios_folder);
        }
    }

    // ROM library folders: previously the only way to set this was the
    // "Add Game Folder…" button shown on the empty Library page, and once
    // set there was no way to add another folder or remove one without
    // hand-editing the config file. This gives the same add/remove controls
    // from the Files settings panel.
    auto *library_group = new QGroupBox(tr("ROM Library Folders"), this);
    auto *library_layout = new QVBoxLayout(library_group);

    library_list_ = new QListWidget(library_group);
    library_layout->addWidget(library_list_);

    auto *library_button_row = new QHBoxLayout;
    auto *add_button = new QPushButton(tr("Add Folder…"), library_group);
    auto *remove_button = new QPushButton(tr("Remove Selected"), library_group);
    library_button_row->addWidget(add_button);
    library_button_row->addWidget(remove_button);
    library_button_row->addStretch(1);
    library_layout->addLayout(library_button_row);

    layout()->addWidget(library_group);

    connect(add_button, &QPushButton::clicked, this, [this] {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Choose a ROM folder"));
        if (dir.isEmpty()) return;

        for (const auto &existing : app->config->library_folders)
            if (QString::fromStdString(existing) == dir) return;

        app->config->library_folders.push_back(dir.toStdString());
        refreshLibraryList();
        if (app->window) app->window->refreshLibrary();
    });

    connect(remove_button, &QPushButton::clicked, this, [this] {
        int row = library_list_->currentRow();
        if (row < 0 || row >= static_cast<int>(app->config->library_folders.size())) return;

        app->config->library_folders.erase(app->config->library_folders.begin() + row);
        refreshLibraryList();
        if (app->window) app->window->refreshLibrary();
    });

    refreshLibraryList();
}

void FoldersPanel::connectEntry(QComboBox *combo, QLineEdit *lineEdit, QPushButton *browse, int *location, std::string *folder)
{
    connect(combo, &QComboBox::activated, [=, this](int index) {
        *location = index;
        this->refreshEntry(combo, lineEdit, browse, location, folder);
        app->updateSettings();
    });
}

void FoldersPanel::refreshData()
{
    refreshEntry(comboBox_sram, lineEdit_sram, pushButton_sram, &app->config->sram_location, &app->config->sram_folder);
    refreshEntry(comboBox_state, lineEdit_state, pushButton_state, &app->config->state_location, &app->config->state_folder);
    refreshEntry(comboBox_cheat, lineEdit_cheat, pushButton_cheat, &app->config->cheat_location, &app->config->cheat_folder);
    refreshEntry(comboBox_patch, lineEdit_patch, pushButton_patch, &app->config->patch_location, &app->config->patch_folder);
    refreshEntry(comboBox_export, lineEdit_export, pushButton_export, &app->config->export_location, &app->config->export_folder);
    if (bios_combo_)
        refreshEntry(bios_combo_, bios_line_, bios_button_,
                     &app->config->bios_location, &app->config->bios_folder);
    refreshLibraryList();
}

void FoldersPanel::refreshLibraryList()
{
    library_list_->clear();
    for (const auto &folder : app->config->library_folders)
        library_list_->addItem(QString::fromStdString(folder));
}

void FoldersPanel::refreshEntry(QComboBox *combo, QLineEdit *lineEdit, QPushButton *browse, int *location, std::string *folder)
{
    QString rom_dir;
    bool custom = (*location == EmuConfig::eCustomDirectory);
    combo->setCurrentIndex(*location);
    if (custom)
    {
        lineEdit->setText(QDir::toNativeSeparators(QString::fromUtf8(*folder)));
    }
    else if (*location == EmuConfig::eConfigDirectory)
    {
        lineEdit->setText(tr("Config folder is %1").arg(EmuConfig::findConfigDir().c_str()));
    } else
    {
        rom_dir = QString::fromStdString(app->getContentFolder());
        if (rom_dir.isEmpty())
            rom_dir = QString::fromStdString(app->config->last_rom_folder);
        rom_dir = QDir::toNativeSeparators(rom_dir);

        lineEdit->setText("ROM Folder: " + rom_dir);
    }

    lineEdit->setEnabled(custom);

    browse->disconnect(SIGNAL(pressed()));
    if (custom)
    {
        browse->setText(tr("Browse..."));
        connect(browse, &QPushButton::pressed, [=, this] {
            QFileDialog dialog(this, tr("Select a Folder"));
            dialog.setFileMode(QFileDialog::Directory);
            dialog.setDirectory(QString::fromUtf8(*folder));
            if (!dialog.exec())
                return;
            *folder = dialog.selectedFiles().at(0).toUtf8();
            *folder = QDir::toNativeSeparators(QString::fromUtf8(*folder)).toStdString();
            lineEdit->setText(QString::fromStdString(*folder));
            app->updateSettings();
        });
    }
    else
    {
        QString dir{};
        if (*location == EmuConfig::eConfigDirectory)
            dir = EmuConfig::findConfigDir().c_str();
        else if (*location == EmuConfig::eROMDirectory)
            dir = rom_dir;

        connect(browse, &QPushButton::pressed, [dir] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
        });
        lineEdit->setEnabled(custom);
        browse->setText(tr("Open Folder..."));
    }
}

void FoldersPanel::showEvent(QShowEvent *event)
{
    refreshData();

    QWidget::showEvent(event);
}

