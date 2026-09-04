#include "AchievementsLoginDialog.hpp"

#include "../EmuApplication.hpp"
#include "../EmuConfig.hpp"

#include <QDialogButtonBox>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

AchievementsLoginDialog::AchievementsLoginDialog(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setWindowTitle(tr("RetroAchievements Login"));
    setMinimumWidth(460);

    auto *layout = new QVBoxLayout(this);

    auto *header_layout = new QHBoxLayout;
    auto *icon_label = new QLabel(this);
    icon_label->setPixmap(QIcon(app->iconPrefix() + "achievements.svg").pixmap(48, 48));
    header_layout->addWidget(icon_label);

    auto *header_text_layout = new QVBoxLayout;
    auto *title_label = new QLabel(tr("RetroAchievements Login"), this);
    QFont title_font = title_label->font();
    title_font.setBold(true);
    title_font.setPointSize(title_font.pointSize() + 2);
    title_label->setFont(title_font);
    header_text_layout->addWidget(title_label);

    auto *description_label = new QLabel(
        tr("Please enter your user name and password for retroachievements.org below. "
           "Your password will not be saved in snes9xrd; an access token will be generated and used instead."),
        this);
    description_label->setWordWrap(true);
    header_text_layout->addWidget(description_label);

    header_layout->addLayout(header_text_layout, 1);
    layout->addLayout(header_layout);

    auto *separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);
    layout->addWidget(separator);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    username_edit = new QLineEdit(this);
    username_edit->setText(QString::fromStdString(app->config->ra_username));
    form->addRow(tr("User Name:"), username_edit);

    password_edit = new QLineEdit(this);
    password_edit->setEchoMode(QLineEdit::Password);
    form->addRow(tr("Password:"), password_edit);

    layout->addLayout(form);

    status_label = new QLabel(tr("Ready…"), this);
    status_label->setWordWrap(true);
    layout->addWidget(status_label);

    buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    buttons->addButton(tr("Login"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    poll_timer = new QTimer(this);
    poll_timer->setInterval(200);
    connect(poll_timer, &QTimer::timeout, this, &AchievementsLoginDialog::pollLoginResult);

    connect(buttons, &QDialogButtonBox::accepted, this, &AchievementsLoginDialog::login);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}


void AchievementsLoginDialog::login()
{
    std::string username = username_edit->text().toStdString();
    std::string password = password_edit->text().toStdString();
    if (username.empty() || password.empty())
    {
        status_label->setText(tr("Username and password are required."));
        return;
    }

    username_edit->setEnabled(false);
    password_edit->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setEnabled(false);
    for (auto *button : buttons->buttons())
        if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
            button->setEnabled(false);
    status_label->setText(tr("Logging in…"));

    app->achievementsLoginWithPassword(username, password);
    poll_timer->start();
}

void AchievementsLoginDialog::pollLoginResult()
{
    // The emu thread's main loop only drains pending network responses while
    // a game is running -- pump it here too, since login can happen before
    // any ROM is ever loaded.
    app->achievementsIdle();

    if (app->achievementsLoginPending())
        return;

    poll_timer->stop();

    if (app->achievementsIsLoggedIn())
    {
        Achievements::UserInfo info = app->achievementsUserInfo();
        app->config->ra_username = info.username;
        app->config->ra_api_token = info.token;
        accept();
        return;
    }

    status_label->setText(QString::fromStdString(app->achievementsLastError()));
    username_edit->setEnabled(true);
    password_edit->setEnabled(true);
    buttons->button(QDialogButtonBox::Cancel)->setEnabled(true);
    for (auto *button : buttons->buttons())
        if (buttons->buttonRole(button) == QDialogButtonBox::AcceptRole)
            button->setEnabled(true);
}
