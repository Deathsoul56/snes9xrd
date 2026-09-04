#include "AchievementsPanel.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "PanelConnectHelpers.hpp"
#include "Achievements/AchievementsLoginDialog.hpp"
#include "Achievements/AchievementsListDialog.hpp"

#include <QDesktopServices>
#include <QUrl>

AchievementsPanel::AchievementsPanel(EmuApplication *app_)
    : app(app_)
{
    setupUi(this);

    connectCheckbox(checkBox_enable_achievements, &app->config->achievements_enabled, app);
    connectCheckbox(checkBox_enable_spectator, &app->config->achievements_spectator_mode, app);
    connectCheckbox(checkBox_enable_encore, &app->config->achievements_encore_mode, app);
    connectCheckbox(checkBox_track_unofficial, &app->config->achievements_track_unofficial, app);
    connectCheckbox(checkBox_show_notifications, &app->config->achievements_notifications, app);
    connectCheckbox(checkBox_show_leaderboard_notifications, &app->config->achievements_leaderboard_notifications, app);
    connectCheckbox(checkBox_show_leaderboard_trackers, &app->config->achievements_leaderboard_trackers, app);
    connectCheckbox(checkBox_show_progress_indicators, &app->config->achievements_progress_indicators, app);
    connectCheckbox(checkBox_show_challenge_indicators, &app->config->achievements_challenge_indicators, app);
    connectSpinBox(spinBox_notification_duration, &app->config->achievements_notification_duration, app);
    connectComboBox(comboBox_notification_location, &app->config->achievements_notification_location, app);

    connect(pushButton_login_logout, &QPushButton::clicked, this, [this] {
        if (app->achievementsIsLoggedIn())
        {
            app->achievementsLogout();
        }
        else
        {
            AchievementsLoginDialog dialog(this, app);
            dialog.exec();
        }
        refreshAccountState();
    });

    connect(pushButton_view_profile, &QPushButton::clicked, this, [this] {
        auto user = app->achievementsUserInfo();
        if (user.logged_in)
            QDesktopServices::openUrl(QUrl("https://retroachievements.org/user/" +
                                            QString::fromStdString(user.username)));
    });

    connect(pushButton_view_list, &QPushButton::clicked, this, [this] {
        AchievementsListDialog dialog(this, app);
        dialog.exec();
    });
}

void AchievementsPanel::showEvent(QShowEvent *event)
{
    checkBox_enable_achievements->setChecked(app->config->achievements_enabled);
    checkBox_enable_spectator->setChecked(app->config->achievements_spectator_mode);
    checkBox_enable_encore->setChecked(app->config->achievements_encore_mode);
    checkBox_track_unofficial->setChecked(app->config->achievements_track_unofficial);
    checkBox_show_notifications->setChecked(app->config->achievements_notifications);
    checkBox_show_leaderboard_notifications->setChecked(app->config->achievements_leaderboard_notifications);
    checkBox_show_leaderboard_trackers->setChecked(app->config->achievements_leaderboard_trackers);
    checkBox_show_progress_indicators->setChecked(app->config->achievements_progress_indicators);
    checkBox_show_challenge_indicators->setChecked(app->config->achievements_challenge_indicators);
    spinBox_notification_duration->setValue(app->config->achievements_notification_duration);
    comboBox_notification_location->setCurrentIndex(app->config->achievements_notification_location);
    refreshAccountState();
    QWidget::showEvent(event);
}

void AchievementsPanel::refreshAccountState()
{
    bool logged_in = app->achievementsIsLoggedIn();
    if (logged_in)
    {
        auto user = app->achievementsUserInfo();
        label_account_status->setText(tr("Logged in as %1.").arg(QString::fromStdString(user.display_name)));
        pushButton_login_logout->setText(tr("Logout"));
    }
    else
    {
        label_account_status->setText(tr("Not logged in."));
        pushButton_login_logout->setText(tr("Login…"));
    }
    pushButton_view_profile->setEnabled(logged_in);
    checkBox_enable_achievements->setEnabled(logged_in);
    checkBox_enable_spectator->setEnabled(logged_in);
    checkBox_enable_encore->setEnabled(logged_in);
    checkBox_track_unofficial->setEnabled(logged_in);
    checkBox_show_notifications->setEnabled(logged_in);
    checkBox_show_leaderboard_notifications->setEnabled(logged_in);
    checkBox_show_leaderboard_trackers->setEnabled(logged_in);
    checkBox_show_progress_indicators->setEnabled(logged_in);
    checkBox_show_challenge_indicators->setEnabled(logged_in);
    spinBox_notification_duration->setEnabled(logged_in);
    comboBox_notification_location->setEnabled(logged_in);

    bool game_loaded = app->achievementsIsGameLoaded();
    pushButton_view_list->setEnabled(logged_in && game_loaded);
    if (game_loaded)
    {
        auto summary = app->achievementsGameSummary();
        label_game_summary->setText(tr("%1 -- %2 of %3 achievements unlocked.")
                                         .arg(QString::fromStdString(summary.title))
                                         .arg(summary.num_unlocked_achievements)
                                         .arg(summary.num_core_achievements));
    }
    else
    {
        label_game_summary->setText(tr("No game loaded."));
    }
}
