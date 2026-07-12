#include "MultiCartDialog.hpp"

#include "EmuApplication.hpp"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

MultiCartDialog::MultiCartDialog(QWidget *parent)
    : QDialog(parent)
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
            QString path = QFileDialog::getOpenFileName(this, label_text, QString(),
                EmuApplication::romFileDialogFilter());
            if (path.isEmpty()) return;
            (*edit)->setText(path);
            if (edit == &this->slot_a_edit_) slot_a_ = path;
            else if (edit == &this->slot_b_edit_) slot_b_ = path;
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

    connect(this, &QDialog::accepted, [this]() {
        slot_a_ = slot_a_edit_->text();
        slot_b_ = slot_b_edit_->text();
    });
}

void MultiCartDialog::browseSlotA()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Choose Slot A cartridge"), QString(),
        EmuApplication::romFileDialogFilter());
    if (path.isEmpty()) return;
    slot_a_edit_->setText(path);
    slot_a_ = path;
}

void MultiCartDialog::browseSlotB()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Choose Slot B cartridge"), QString(),
        EmuApplication::romFileDialogFilter());
    if (path.isEmpty()) return;
    slot_b_edit_->setText(path);
    slot_b_ = path;
}

void MultiCartDialog::swapAB()
{
    std::swap(slot_a_, slot_b_);
    slot_a_edit_->setText(slot_a_);
    slot_b_edit_->setText(slot_b_);
}