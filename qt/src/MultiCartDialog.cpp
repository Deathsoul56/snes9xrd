#include "MultiCartDialog.hpp"

#include "EmuApplication.hpp"
#include "EmuConfig.hpp"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

MultiCartDialog::MultiCartDialog(EmuConfig *config, QWidget *parent)
    : QDialog(parent), config_(config)
{
    setWindowTitle(tr("Open MultiCart"));
    setMinimumWidth(520);

    auto *root = new QVBoxLayout(this);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    auto make_row = [&](QString label_text, QLineEdit **edit) -> QHBoxLayout *
    {
        auto *row = new QHBoxLayout;
        *edit = new QLineEdit(this);
        (*edit)->setReadOnly(true);
        (*edit)->setPlaceholderText(tr("No file selected"));
        row->addWidget(*edit, 1);

        auto *browse = new QPushButton(tr("Browse…"), this);
        connect(browse, &QPushButton::clicked, this, [this, edit, label_text]() {
            QString start_dir = config_ ? QString::fromStdString(config_->last_multicart_folder) : QString();
            QString path = QFileDialog::getOpenFileName(this, label_text, start_dir,
                EmuApplication::romFileDialogFilter());
            if (path.isEmpty()) return;
            (*edit)->setText(path);
            if (edit == &this->slot_a_edit_) slot_a_ = path;
            else if (edit == &this->slot_b_edit_) slot_b_ = path;
            if (config_) config_->last_multicart_folder = QFileInfo(path).absolutePath().toStdString();
        });
        row->addWidget(browse);

        auto *clear = new QPushButton(tr("Clear"), this);
        connect(clear, &QPushButton::clicked, this, [this, edit]() {
            (*edit)->clear();
            if (edit == &this->slot_a_edit_) slot_a_.clear();
            else if (edit == &this->slot_b_edit_) slot_b_.clear();
        });
        row->addWidget(clear);
        return row;
    };

    form->addRow(tr("Slot A:"), make_row(tr("Choose Slot A cartridge"), &slot_a_edit_));
    form->addRow(tr("Slot B:"), make_row(tr("Choose Slot B cartridge"), &slot_b_edit_));
    root->addLayout(form);

    // BIOS row: read-only, informational only (matches win32). The path is
    // resolved from Settings -> Files -> BIOS, same lookup the core performs
    // for Sufami Turbo/BS-X (STBIOS.bin), so there is nothing to browse here.
    auto *bios_row = new QHBoxLayout;
    bios_edit_ = new QLineEdit(this);
    bios_edit_->setReadOnly(true);
    bios_row->addWidget(bios_edit_, 1);
    bios_status_label_ = new QLabel(this);
    bios_row->addWidget(bios_status_label_);
    form->addRow(tr("BIOS:"), bios_row);

    auto *swap_row = new QHBoxLayout;
    auto *swap_btn = new QPushButton(tr("Swap A and B"), this);
    connect(swap_btn, &QPushButton::clicked, this, &MultiCartDialog::swapAB);
    swap_row->addWidget(swap_btn);
    swap_row->addStretch(1);
    root->addLayout(swap_row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
        if (slot_a_.isEmpty()) return; // Slot A is mandatory
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);

    // Restore the previously used Slot A/B paths so they don't need to be
    // re-entered every time (classic win32 "Rom:MultiCartA/B" behavior).
    if (config_)
    {
        slot_a_ = QString::fromStdString(config_->last_multicart_slot_a);
        slot_b_ = QString::fromStdString(config_->last_multicart_slot_b);
        slot_a_edit_->setText(slot_a_);
        slot_b_edit_->setText(slot_b_);
    }
    refreshBiosStatus();

    connect(this, &QDialog::accepted, [this]() {
        slot_a_ = slot_a_edit_->text();
        slot_b_ = slot_b_edit_->text();
        if (config_)
        {
            config_->last_multicart_slot_a = slot_a_.toStdString();
            config_->last_multicart_slot_b = slot_b_.toStdString();
        }
    });
}

void MultiCartDialog::refreshBiosStatus()
{
    if (!config_)
        return;

    // Mirror Snes9xController::updateSettings()'s doFolder() resolution for
    // BIOS_DIR so the path shown here always matches what the core will
    // actually look in.
    std::string bios_dir;
    if (config_->bios_location == EmuConfig::eROMDirectory)
        bios_dir = QFileInfo(slot_a_edit_->text()).absolutePath().toStdString();
    else if (config_->bios_location == EmuConfig::eConfigDirectory)
        bios_dir = EmuConfig::findConfigDir() + "/bios";
    else
        bios_dir = config_->bios_folder;

    QString stbios_path = QDir(QString::fromStdString(bios_dir)).filePath("STBIOS.bin");
    bios_edit_->setText(QDir::toNativeSeparators(stbios_path));

    if (QFileInfo::exists(stbios_path))
    {
        bios_status_label_->setText(tr("Found"));
        bios_status_label_->setStyleSheet(QString());
    }
    else
    {
        bios_status_label_->setText(tr("Not found"));
        bios_status_label_->setStyleSheet("color: #d06060;");
    }
}


void MultiCartDialog::browseSlotA()
{
    QString start_dir = config_ ? QString::fromStdString(config_->last_multicart_folder) : QString();
    QString path = QFileDialog::getOpenFileName(this, tr("Choose Slot A cartridge"), start_dir,
        EmuApplication::romFileDialogFilter());
    if (path.isEmpty()) return;
    slot_a_edit_->setText(path);
    slot_a_ = path;
    if (config_) config_->last_multicart_folder = QFileInfo(path).absolutePath().toStdString();
}

void MultiCartDialog::browseSlotB()
{
    QString start_dir = config_ ? QString::fromStdString(config_->last_multicart_folder) : QString();
    QString path = QFileDialog::getOpenFileName(this, tr("Choose Slot B cartridge"), start_dir,
        EmuApplication::romFileDialogFilter());
    if (path.isEmpty()) return;
    slot_b_edit_->setText(path);
    slot_b_ = path;
    if (config_) config_->last_multicart_folder = QFileInfo(path).absolutePath().toStdString();
}

void MultiCartDialog::swapAB()
{
    std::swap(slot_a_, slot_b_);
    slot_a_edit_->setText(slot_a_);
    slot_b_edit_->setText(slot_b_);
}