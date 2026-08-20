#pragma once
#include "ui_CheatsDialog.h"

class EmuApplication;

class CheatsDialog : public QDialog, public Ui_Dialog
{
  public:
    CheatsDialog(QWidget *parent, EmuApplication *app);
    void addCode();
    void clearCodes();
    void removeCode();
    void updateCurrent();
    void removeAll();
    void searchDatabase();
    void refreshList();
    void showEvent(QShowEvent *) override;
    void reject() override;
    EmuApplication *app;
    void resizeEvent(QResizeEvent *event) override;

  private:
    std::vector<std::tuple<bool, std::string, std::string>> original_cheats_;
    bool original_cheats_enabled_ = false;
};

