#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;
class QPushButton;
class EmuConfig;

class MultiCartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiCartDialog(EmuConfig *config, QWidget *parent = nullptr);

    QString slotA() const { return slot_a_; }
    QString slotB() const { return slot_b_; }

private slots:
    void browseSlotA();
    void browseSlotB();
    void swapAB();

private:
    // Reflects the STBIOS.bin lookup path resolved from the configured BIOS
    // folder (Settings -> Files -> BIOS), same as win32's read-only BIOS row.
    void refreshBiosStatus();

    QLineEdit *slot_a_edit_ = nullptr;
    QLineEdit *slot_b_edit_ = nullptr;
    QLineEdit *bios_edit_ = nullptr;
    QLabel *bios_status_label_ = nullptr;

    QString slot_a_;
    QString slot_b_;

    EmuConfig *config_ = nullptr;
};