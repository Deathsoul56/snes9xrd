#include "AchievementsListDialog.hpp"

#include "../EmuApplication.hpp"

#include <QLabel>
#include <QListWidget>
#include <QVBoxLayout>

AchievementsListDialog::AchievementsListDialog(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setWindowTitle(tr("Achievements"));
    resize(480, 400);

    auto *layout = new QVBoxLayout(this);

    summary_label = new QLabel(this);
    layout->addWidget(summary_label);

    list_widget = new QListWidget(this);
    layout->addWidget(list_widget);

    refresh();
}

void AchievementsListDialog::refresh()
{
    list_widget->clear();

    if (!app->achievementsIsGameLoaded())
    {
        summary_label->setText(tr("No game with achievements is currently loaded."));
        return;
    }

    Achievements::GameSummary summary = app->achievementsGameSummary();
    summary_label->setText(tr("%1 -- %2 of %3 achievements unlocked")
                                .arg(QString::fromStdString(summary.title))
                                .arg(summary.num_unlocked_achievements)
                                .arg(summary.num_core_achievements));

    for (const auto &achievement : app->achievementsList())
    {
        QString text = QStringLiteral("%1[%2] %3 -- %4")
                            .arg(achievement.unlocked ? QStringLiteral("\u2713 ") : QString())
                            .arg(achievement.points)
                            .arg(QString::fromStdString(achievement.title))
                            .arg(QString::fromStdString(achievement.description));
        auto *item = new QListWidgetItem(text, list_widget);
        item->setForeground(achievement.unlocked ? palette().text() : palette().placeholderText());
    }
}
