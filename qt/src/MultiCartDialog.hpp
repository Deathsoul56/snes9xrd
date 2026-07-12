#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QPushButton;

class MultiCartDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiCartDialog(QWidget *parent = nullptr);

    QString slotA() const { return slot_a_; }
    QString slotB() const { return slot_b_; }

private slots:
    void browseSlotA();
    void browseSlotB();
    void swapAB();

private:
    QLineEdit *slot_a_edit_ = nullptr;
    QLineEdit *slot_b_edit_ = nullptr;

    QString slot_a_;
    QString slot_b_;
};