#pragma once

#include <QDialog>

#include <tuple>
#include <vector>

class EmuApplication;
class QButtonGroup;
class QComboBox;
class QLineEdit;
class QPushButton;
class QTableView;

class CheatSearchDialog : public QDialog
{
  public:
    CheatSearchDialog(QWidget *parent, EmuApplication *app);

  private:
    void refreshResults();
    void search();
    void reset();
    void addCheat();
    uint32_t enteredValue(bool *ok) const;

    EmuApplication *app;
    QTableView *results_view;
    QComboBox *comparison_box;
    QComboBox *compare_to_box;
    QComboBox *data_size_box;
    QComboBox *data_type_box;
    QLineEdit *value_edit;
    QPushButton *add_button;
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> results_;
};