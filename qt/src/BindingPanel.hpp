#pragma once

#include <QWidget>
#include <QTableWidget>
#include <functional>
#include <string>
#include "EmuBinding.hpp"

class EmuApplication;

class BindingPanel : public QWidget
{
  public:
    BindingPanel(EmuApplication *app);
    ~BindingPanel();
    void setTableWidget(QTableWidget *bindingTableWidget, EmuBinding *binding, int width, int height);
    void cellActivated(int row, int column);
    void handleKeyPressEvent(QKeyEvent *event);
    void updateCellFromBinding(int row, int column);
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void fillTable();
    void checkJoypadInput();
    void finalizeCurrentBinding(const EmuBinding &b);
    void setRedirectInput(bool redirect);
    void onJoypadsChanged(const std::function<void()> &func);
    void clearRow(int row);
    void resetRowToDefault(int row);
    void updateConflictHighlights();
    // Recomputes keyboard/joypad/mouse device-type icons for the active
    // theme and reapplies them to the table (called after a theme switch).
    virtual void refreshIcons();

    bool awaiting_binding;
    bool accept_return;
    int table_width;
    int table_height;
    int cell_row;
    int cell_column;
    QIcon keyboard_icon;
    QIcon joypad_icon;
    QIcon mouse_icon;
    std::unique_ptr<QTimer> timer;
    EmuApplication *app;
    QTableWidget *binding_table_widget;
    EmuBinding *binding;
    std::function<void()> joypads_changed;
    // Set by ShortcutsPanel to allow Ctrl/Shift/Alt/Meta + key combos when
    // capturing a binding. Left false for ControllerPanel/mouse tables,
    // which only ever accept a single key/button press per binding.
    bool allow_key_combos = false;
    // Set by ShortcutsPanel before calling setTableWidget() to opt into the
    // per-row "reset to default" column. Left unset (empty) by
    // ControllerPanel/mouse tables, which have no per-row default lookup.
    std::function<std::string(int row)> default_binding_resolver;
};