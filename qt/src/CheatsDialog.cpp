#include "CheatsDialog.hpp"
#include "EmuApplication.hpp"
#include "EmuConfig.hpp"
#include "fscompat.h"

#include <QMessageBox>
#include <QDir>
#include <QtEvents>

static const auto desired_flags = Qt::ItemFlag::ItemIsUserCheckable |
                                  Qt::ItemFlag::ItemIsEnabled |
                                  Qt::ItemFlag::ItemIsSelectable |
                                  Qt::ItemFlag::ItemIsDragEnabled;

CheatsDialog::CheatsDialog(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setupUi(this);
    setObjectName("cheatsDialog");

    connect(pushButton_add, &QPushButton::clicked, [&] { addCode(); });
    connect(pushButton_clear, &QPushButton::clicked, [&] { clearCodes(); });
    connect(pushButton_remove, &QPushButton::clicked, [&] { removeCode(); });
    connect(pushButton_remove_all, &QPushButton::clicked, [&] { removeAll(); });
    connect(pushButton_check_database, &QPushButton::clicked, [&] { searchDatabase(); });
    connect(pushButton_update, &QPushButton::clicked, [&] { updateCurrent(); });
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CheatsDialog::reject);

    treeWidget_cheats->header()->setSectionResizeMode(QHeaderView::ResizeToContents);

    connect(treeWidget_cheats, &QTreeWidget::itemChanged, [&](QTreeWidgetItem *item, int column) {
        if (column != 0)
            return;

        auto index = treeWidget_cheats->indexOfTopLevelItem(item);

        if (item->checkState(0) == Qt::Checked)
            app->enableCheat(index);
        else
            app->disableCheat(index);
    });

    connect(treeWidget_cheats, &QTreeWidget::currentItemChanged, [&](QTreeWidgetItem *item) {
        if (!item)
        {
            lineEdit_code->clear();
            lineEdit_description->clear();
            return;
        }

        lineEdit_code->setText(item->text(1));
        lineEdit_description->setText(item->text(2));
    });

    if (app->config->cheat_dialog_width != 0)
        resize(app->config->cheat_dialog_width, app->config->cheat_dialog_height);

    show();
}

void CheatsDialog::showEvent(QShowEvent *event)
{
    refreshList();
    original_cheats_ = app->getCheatList();
    original_cheats_enabled_ = app->cheatsEnabled();
    QDialog::showEvent(event);
}

void CheatsDialog::reject()
{
    app->restoreCheats(original_cheats_, original_cheats_enabled_);
    QDialog::reject();
}

void CheatsDialog::addCode()
{
    auto description = lineEdit_description->text().toStdString();
    auto code = lineEdit_code->text().toStdString();

    if (description.empty())
        description = tr("No description").toStdString();

    if (!app->addCheat(description, code))
    {
        QMessageBox::information(this, tr("Invalid Cheat"), tr("The cheat you entered was not valid."));
        return;
    }

    refreshList();
    treeWidget_cheats->setTreePosition(treeWidget_cheats->topLevelItemCount() - 1);
}

void CheatsDialog::clearCodes()
{
    app->disableAllCheats();
    refreshList();
}

void CheatsDialog::removeCode()
{
    if (!treeWidget_cheats->currentIndex().isValid())
        return;

    auto index = treeWidget_cheats->currentIndex().row();
    app->deleteCheat(index);
    auto item = treeWidget_cheats->takeTopLevelItem(index);
    delete item;
}

void CheatsDialog::removeAll()
{
    treeWidget_cheats->clear();
    app->deleteAllCheats();
}

void CheatsDialog::searchDatabase()
{
    std::initializer_list<std::string> dirs =
    {
        EmuConfig::findConfigDir(),
        QGuiApplication::applicationDirPath().toStdString(),
        S9xGetDirectory(CHEAT_DIR),
        "/usr/share/snes9x",
        "/usr/local/share/snes9x"
    };

    for (auto &path : dirs)
    {
        auto filename = QDir(QString::fromStdString(path)).absoluteFilePath("cheats.bml").toStdString();
        auto result = app->tryImportCheats(filename);
        if (result == 0)
        {
            refreshList();
            return;
        }

        if (result == -1)
            continue;

        if (result == -2)
        {
            QMessageBox::information(this, tr("No Cheats Found"), tr("No cheats for the current game were found in the database."));
            return;
        }
    }

    QMessageBox::information(this, tr("Unable to Open Cheats Database"), tr("cheats.bml was not found."));
}

void CheatsDialog::updateCurrent()
{
    if (!treeWidget_cheats->currentIndex().isValid())
        return;

    auto description = lineEdit_description->text().toStdString();
    auto code = lineEdit_code->text().toStdString();
    auto index = treeWidget_cheats->currentIndex().row();

    if (description.empty())
        description = tr("No description").toStdString();

    auto validated = app->validateCheat(code);
    if (validated.empty())
    {
        QMessageBox::information(this, tr("Invalid Cheat"), tr("The cheat you entered was not valid."));
        return;
    }

    app->modifyCheat(index, description, validated);

    auto *item = treeWidget_cheats->currentItem();
    item->setText(1, QString::fromStdString(validated));
    item->setText(2, lineEdit_description->text());
}

void CheatsDialog::refreshList()
{
    treeWidget_cheats->clear();

    QList<QTreeWidgetItem *> items;

    auto clist = app->getCheatList();
    for (const auto &[enabled, name, cheat]: clist)
    {
        auto i = new QTreeWidgetItem();
        i->setFlags(desired_flags);
        i->setCheckState(0, enabled ? Qt::Checked : Qt::Unchecked);
        i->setText(1, QString::fromStdString(cheat));
        i->setText(2, QString::fromStdString(name));
        items.push_back(i);
    }

    treeWidget_cheats->insertTopLevelItems(0, items);
}

void CheatsDialog::resizeEvent(QResizeEvent *event)
{
    app->config->cheat_dialog_width = event->size().width();
    app->config->cheat_dialog_height = event->size().height();
}