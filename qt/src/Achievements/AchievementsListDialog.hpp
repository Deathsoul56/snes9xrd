#pragma once

#include <QDialog>

class EmuApplication;
class QListWidget;
class QLabel;

// Text-only achievement list for the MVP -- no badge image download/caching
// (that's a natural follow-up once this is proven out, not core to showing
// users their unlock progress).
class AchievementsListDialog : public QDialog
{
  public:
    AchievementsListDialog(QWidget *parent, EmuApplication *app);

  private:
    void refresh();

    EmuApplication *app;
    QLabel *summary_label;
    QListWidget *list_widget;
};
