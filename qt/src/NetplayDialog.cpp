#include "NetplayDialog.hpp"

#include "EmuApplication.hpp"
#include "EmuConfig.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

NetplayDialog::NetplayDialog(QWidget *parent, EmuApplication *app_, bool host_mode)
    : QDialog(parent), app(app_)
{
    setWindowTitle(tr("Netplay"));

    auto *layout = new QVBoxLayout(this);

    host_radio = new QRadioButton(tr("Act as Server"), this);
    connect_radio = new QRadioButton(tr("Connect to Server"), this);
    layout->addWidget(host_radio);
    layout->addWidget(connect_radio);

    auto *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignRight);

    host_edit = new QLineEdit(this);
    host_edit->setText(QString::fromStdString(app->config->netplay_last_host));
    form->addRow(tr("Server Address:"), host_edit);

    port_edit = new QSpinBox(this);
    port_edit->setRange(1, 65535);
    port_edit->setValue(app->config->netplay_last_port);
    form->addRow(tr("Port:"), port_edit);

    max_frame_loss_edit = new QSpinBox(this);
    max_frame_loss_edit->setRange(1, 240);
    max_frame_loss_edit->setValue(app->config->netplay_max_frame_loss);
    form->addRow(tr("Max Frames Behind:"), max_frame_loss_edit);

    sync_reset_check = new QCheckBox(tr("Sync Using Reset Game"), this);
    sync_reset_check->setChecked(app->config->netplay_sync_reset);
    send_rom_check = new QCheckBox(tr("Send ROM Image to Clients on Connect"), this);
    send_rom_check->setChecked(app->config->netplay_send_rom);

    form->addRow(QString(), sync_reset_check);
    form->addRow(QString(), send_rom_check);
    layout->addLayout(form);

    status_label = new QLabel(this);
    status_label->setWordWrap(true);
    layout->addWidget(status_label);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    auto *go_button = buttons->addButton(tr("Connect"), QDialogButtonBox::AcceptRole);
    layout->addWidget(buttons);

    auto update_mode = [&, go_button] {
        bool hosting = host_radio->isChecked();
        host_edit->setEnabled(!hosting);
        go_button->setText(hosting ? tr("Host") : tr("Connect"));
    };
    connect(host_radio, &QRadioButton::toggled, this, update_mode);
    host_radio->setChecked(host_mode);
    connect_radio->setChecked(!host_mode);
    update_mode();

    connect(buttons, &QDialogButtonBox::accepted, this, &NetplayDialog::connectOrHost);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void NetplayDialog::connectOrHost()
{
    app->config->netplay_last_host = host_edit->text().toStdString();
    app->config->netplay_last_port = port_edit->value();
    app->config->netplay_default_port = port_edit->value();
    app->config->netplay_max_frame_loss = max_frame_loss_edit->value();
    app->config->netplay_sync_reset = sync_reset_check->isChecked();
    app->config->netplay_send_rom = send_rom_check->isChecked();

    app->netplaySetMaxFrameLoss(max_frame_loss_edit->value());
    app->netplaySetSyncByReset(sync_reset_check->isChecked());
    app->netplaySetSendRomOnConnect(send_rom_check->isChecked());

    bool ok;
    if (host_radio->isChecked())
    {
        app->config->netplay_is_server = true;
        ok = app->netplayStartServer(port_edit->value());
    }
    else
    {
        app->config->netplay_is_server = false;
        ok = app->netplayConnect(host_edit->text().toStdString(), port_edit->value());
    }

    if (!ok)
    {
        QMessageBox::critical(this, tr("Netplay Error"),
                               QString::fromStdString(app->netplayLastError()));
        return;
    }

    accept();
}
