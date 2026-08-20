#include "CheatSearchDialog.hpp"

#include "EmuApplication.hpp"

#include <QAbstractTableModel>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>

namespace
{
class ResultModel : public QAbstractTableModel
{
  public:
    explicit ResultModel(std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> *results, QObject *parent)
        : QAbstractTableModel(parent), results_(results) {}

    int rowCount(const QModelIndex &parent) const override
    {
        return parent.isValid() ? 0 : static_cast<int>(results_->size());
    }

    int columnCount(const QModelIndex &parent) const override
    {
        return parent.isValid() ? 0 : 3;
    }

    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole)
            return {};

        const auto &[address, current, previous] = results_->at(index.row());
        if (index.column() == 0)
            return QString("%1").arg(address, 6, 16, QLatin1Char('0')).toUpper();
        if (index.column() == 1)
            return QString::number(current);
        return QString::number(previous);
    }

    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        static const QStringList labels = { QObject::tr("Address"), QObject::tr("Curr. Value"), QObject::tr("Prev. Value") };
        return labels.at(section);
    }

    void refresh()
    {
        beginResetModel();
        endResetModel();
    }

  private:
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> *results_;
};
}

CheatSearchDialog::CheatSearchDialog(QWidget *parent, EmuApplication *app_)
    : QDialog(parent), app(app_)
{
    setWindowTitle(tr("Cheat Search"));
    resize(620, 510);

    auto *layout = new QVBoxLayout(this);
    auto *top = new QHBoxLayout;
    results_view = new QTableView(this);
    results_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view->setModel(new ResultModel(&results_, results_view));
    results_view->horizontalHeader()->setStretchLastSection(true);
    results_view->verticalHeader()->hide();
    top->addWidget(results_view, 1);

    auto *actions = new QVBoxLayout;
    auto *search_button = new QPushButton(tr("Search"), this);
    add_button = new QPushButton(tr("Add Cheat"), this);
    auto *reset_button = new QPushButton(tr("Reset"), this);
    add_button->setEnabled(false);
    actions->addWidget(search_button);
    actions->addWidget(add_button);
    actions->addWidget(reset_button);
    actions->addStretch();
    top->addLayout(actions);
    layout->addLayout(top);

    auto *options = new QFormLayout;
    comparison_box = new QComboBox(this);
    comparison_box->addItems({ tr("< (Less Than)"), tr("> (Greater Than)"), tr("<= (Less Than or Equal to)"),
                               tr(">= (Greater Than or Equal to)"), tr("= (Equal To)"), tr("!= (Not Equal To)") });
    compare_to_box = new QComboBox(this);
    compare_to_box->addItems({ tr("Previous Value"), tr("Entered Value"), tr("Entered Address") });
    data_size_box = new QComboBox(this);
    data_size_box->addItems({ tr("1 byte"), tr("2 bytes"), tr("3 bytes"), tr("4 bytes") });
    data_type_box = new QComboBox(this);
    data_type_box->addItems({ tr("Unsigned (>= 0)"), tr("Signed (+/-)"), tr("Hexadecimal") });
    value_edit = new QLineEdit(this);
    value_edit->setEnabled(false);
    options->addRow(tr("Comparison Type:"), comparison_box);
    options->addRow(tr("Compare To:"), compare_to_box);
    options->addRow(tr("Data Size:"), data_size_box);
    options->addRow(tr("Data Type:"), data_type_box);
    options->addRow(tr("Enter a Value:"), value_edit);
    layout->addLayout(options);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);

    connect(compare_to_box, &QComboBox::currentIndexChanged, this, [this](int index) {
        value_edit->setEnabled(index != 0);
    });
    connect(search_button, &QPushButton::clicked, this, &CheatSearchDialog::search);
    connect(reset_button, &QPushButton::clicked, this, &CheatSearchDialog::reset);
    connect(add_button, &QPushButton::clicked, this, &CheatSearchDialog::addCheat);
    connect(results_view->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
        add_button->setEnabled(results_view->currentIndex().isValid());
    });
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    reset();
}

uint32_t CheatSearchDialog::enteredValue(bool *ok) const
{
    bool parsed = false;
    uint32_t value = value_edit->text().toUInt(&parsed, data_type_box->currentIndex() == 2 ? 16 : 10);
    *ok = parsed;
    return value;
}

void CheatSearchDialog::refreshResults()
{
    static_cast<ResultModel *>(results_view->model())->refresh();
    add_button->setEnabled(false);
}

void CheatSearchDialog::reset()
{
    app->resetCheatSearch();
    results_ = app->searchCheats(-1, data_size_box->currentIndex(), 0, 0, false);
    refreshResults();
}

void CheatSearchDialog::search()
{
    bool ok = true;
    uint32_t value = 0;
    if (compare_to_box->currentIndex() != 0)
        value = enteredValue(&ok);
    if (!ok)
    {
        QMessageBox::information(this, tr("Cheat Search"), tr("Enter a valid value."));
        return;
    }

    results_ = app->searchCheats(comparison_box->currentIndex(), data_size_box->currentIndex(),
                                 compare_to_box->currentIndex(), value, data_type_box->currentIndex() == 1);
    refreshResults();
}

void CheatSearchDialog::addCheat()
{
    const auto &[address, current, previous] = results_.at(results_view->currentIndex().row());
    Q_UNUSED(previous);
    bool ok = false;
    auto description = QInputDialog::getText(this, tr("Cheat Details"), tr("Description:"), QLineEdit::Normal, {}, &ok);
    if (!ok)
        return;

    QStringList codes;
    for (int byte = 0; byte <= data_size_box->currentIndex(); byte++)
    {
        codes << QString("%1=%2")
                     .arg(address + byte, 6, 16, QLatin1Char('0'))
                     .arg((current >> (byte * 8)) & 0xff, 2, 16, QLatin1Char('0'));
    }
    auto code = codes.join('+');
    if (!app->addCheat(description.toStdString(), code.toStdString()))
        QMessageBox::information(this, tr("Cheat Details"), tr("Could not add this cheat."));
}