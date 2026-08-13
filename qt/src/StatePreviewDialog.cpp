#include "StatePreviewDialog.hpp"

#include "EmuApplication.hpp"
#include "Snes9xController.hpp"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QToolButton>
#include <QVBoxLayout>

StatePreviewDialog::StatePreviewDialog(QWidget *parent, EmuApplication *app_, bool save_)
    : QDialog(parent), app(app_), save(save_)
{
    setWindowTitle(save ? tr("Save with Preview") : tr("Load with Preview"));

    auto *layout = new QVBoxLayout(this);
    auto *grid = new QGridLayout;
    grid->setSpacing(8);

    for (int slot = 0; slot < 10; slot++)
    {
        auto *button = new QToolButton(this);
        button->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        button->setIconSize(QSize(128, 112));
        button->setMinimumSize(138, 144);
        connect(button, &QToolButton::clicked, this, [this, slot] { selectSlot(slot); });
        grid->addWidget(button, slot / 5, slot % 5);
        slot_buttons[slot] = button;
    }
    layout->addLayout(grid);

    auto *footer = new QHBoxLayout;
    footer->addWidget(new QLabel(tr("Bank:"), this));
    bank = new QComboBox(this);
    for (int index = 0; index < 10; index++)
        bank->addItem(tr("Bank #%1").arg(index));
    footer->addWidget(bank);
    footer->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    footer->addWidget(buttons);
    layout->addLayout(footer);

    connect(bank, &QComboBox::currentIndexChanged, this, [this] { refresh(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    refresh();
}

void StatePreviewDialog::refresh()
{
    int current_bank = bank->currentIndex();
    for (int slot = 0; slot < 10; slot++)
    {
        std::vector<uint16_t> pixels;
        int width = 0;
        int height = 0;
        auto *button = slot_buttons[slot];
        button->setText(tr("Slot #%1").arg(slot));
        button->setIcon(QIcon());

        if (app->core->statePreview(current_bank * 10 + slot, pixels, width, height))
        {
            QImage image(reinterpret_cast<const uchar *>(pixels.data()), width, height,
                         width * (int)sizeof(uint16_t), QImage::Format_RGB16);
            button->setIcon(QPixmap::fromImage(image.copy().scaled(128, 112, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        }
    }
}

void StatePreviewDialog::selectSlot(int slot)
{
    int position = bank->currentIndex() * 10 + slot;
    if (save)
    {
        if (app->saveState(position))
            refresh();
    }
    else if (app->loadState(position))
    {
        accept();
    }
}
