#pragma once
#include "ui_FoldersPanel.h"
#include "EmuApplication.hpp"

class QListWidget;

class FoldersPanel :
    public Ui::FoldersPanel,
    public QWidget
{
  public:
    explicit FoldersPanel(EmuApplication *app);
    void showEvent(QShowEvent *event) override;
    void connectEntry(QComboBox *combo, QLineEdit *lineEdit, QPushButton *browse, int *location, std::string *folder);
    void refreshEntry(QComboBox *combo, QLineEdit *lineEdit, QPushButton *browse, int *location, std::string *folder);
    void refreshData();
    void refreshLibraryList();
    EmuApplication *app;

    // Runtime-added BIOS row (the .ui doesn't define one).
    QComboBox   *bios_combo_  = nullptr;
    QLineEdit   *bios_line_   = nullptr;
    QPushButton *bios_button_ = nullptr;

    // Runtime-added ROM library folders list (the .ui doesn't define one).
    // Lets the user add/remove Library folders from Options -> Files
    // instead of only being able to add one via the empty-library screen
    // with no way to change it afterwards.
    QListWidget *library_list_ = nullptr;
};