// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/importdescriptorsdialog.h>
#include <qt/forms/ui_importdescriptorsdialog.h>

#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>
#include <wallet/wallet.h>

#include <QCheckBox>
#include <QMessageBox>
#include <QTableWidgetItem>

enum Col {
    COL_DESCRIPTOR = 0,
    COL_TIMESTAMP,     // int string or "now"
    COL_RANGE_FROM,
    COL_RANGE_TO,
    COL_INTERNAL,
    COL_ACTIVE,
    COL_LABEL,
    COL_COUNT
};

ImportDescriptorsDialog::ImportDescriptorsDialog(WalletModel* wallet_model, QWidget* parent)
    : QDialog(parent, GUIUtil::dialog_flags)
    , m_ui(new Ui::ImportDescriptorsDialog)
    , m_wallet_model(wallet_model)
{
    m_ui->setupUi(this);

    m_ui->tableWidget->setColumnCount(COL_COUNT);
    m_ui->tableWidget->setHorizontalHeaderLabels({
        tr("Descriptor"), tr("Timestamp"), tr("Range From"),
        tr("Range To"), tr("Internal"), tr("Active"), tr("Label")
    });
    m_ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    m_ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);

    connect(m_ui->cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_ui->tableWidget, &QTableWidget::itemChanged,
            this, &ImportDescriptorsDialog::updateImportButtonState);

    on_addButton_clicked(); // start with one row
}

ImportDescriptorsDialog::~ImportDescriptorsDialog()
{
    delete m_ui;
}

void ImportDescriptorsDialog::on_addButton_clicked()
{
    int row = m_ui->tableWidget->rowCount();
    m_ui->tableWidget->insertRow(row);

    m_ui->tableWidget->setItem(row, COL_DESCRIPTOR,  new QTableWidgetItem{});
    m_ui->tableWidget->setItem(row, COL_TIMESTAMP,   new QTableWidgetItem{QStringLiteral("now")});
    m_ui->tableWidget->setItem(row, COL_RANGE_FROM,  new QTableWidgetItem{});
    m_ui->tableWidget->setItem(row, COL_RANGE_TO,    new QTableWidgetItem{});
    m_ui->tableWidget->setItem(row, COL_LABEL,       new QTableWidgetItem{});

    auto* internal_cb = new QCheckBox{this};
    m_ui->tableWidget->setCellWidget(row, COL_INTERNAL, internal_cb);

    auto* active_cb = new QCheckBox{this};
    m_ui->tableWidget->setCellWidget(row, COL_ACTIVE, active_cb);

    connect(internal_cb, &QCheckBox::checkStateChanged, this, &ImportDescriptorsDialog::updateImportButtonState);
    connect(active_cb,   &QCheckBox::checkStateChanged, this, &ImportDescriptorsDialog::updateImportButtonState);

    m_ui->tableWidget->scrollToBottom();
    updateImportButtonState();
}

void ImportDescriptorsDialog::on_removeButton_clicked()
{
    QSet<int> rows;
    for (auto* item : m_ui->tableWidget->selectedItems())
        rows.insert(item->row());

    auto sorted = rows.values();
    std::sort(sorted.begin(), sorted.end(), std::greater<int>());
    for (int r : sorted)
        m_ui->tableWidget->removeRow(r);

    updateImportButtonState();
}

void ImportDescriptorsDialog::on_clearButton_clicked()
{
    m_ui->tableWidget->setRowCount(0);
    setStatus({});
    on_addButton_clicked();
}

void ImportDescriptorsDialog::on_importButton_clicked()
{
    std::vector<interfaces::ImportDescriptorRequest> requests;

    for (int row = 0; row < m_ui->tableWidget->rowCount(); ++row) {
        auto text = [&](int col) -> QString {
            auto* it = m_ui->tableWidget->item(row, col);
            return it ? it->text().trimmed() : QString{};
        };
        auto checked = [&](int col) -> bool {
            auto* cb = qobject_cast<QCheckBox*>(m_ui->tableWidget->cellWidget(row, col));
            return cb && cb->isChecked();
        };

        const QString desc = text(COL_DESCRIPTOR);
        if (desc.isEmpty()) continue;

        interfaces::ImportDescriptorRequest req;
        req.descriptor = desc.toStdString();
        req.internal   = checked(COL_INTERNAL);
        req.active     = checked(COL_ACTIVE);

        // label is std::optional<std::string>
        const QString label_str = text(COL_LABEL);
        if (!label_str.isEmpty()) {
            req.label = label_str.toStdString();
        }

        // Timestamp
        const QString ts_str = text(COL_TIMESTAMP);
        if (ts_str.isEmpty() || ts_str == QLatin1String("now")) {
            req.timestamp = -1;
        } else {
            bool ok = false;
            req.timestamp = ts_str.toLongLong(&ok);
            if (!ok || req.timestamp < 0) {
                setStatus(tr("Row %1: invalid timestamp. Use a Unix timestamp or \"now\".").arg(row + 1), true);
                return;
            }
        }

        // Range
        const QString from_str = text(COL_RANGE_FROM);
        const QString to_str   = text(COL_RANGE_TO);
        if (!from_str.isEmpty() || !to_str.isEmpty()) {
            bool ok1 = false, ok2 = false;
            int64_t from_val = from_str.toLongLong(&ok1);
            int64_t to_val   = to_str.toLongLong(&ok2);
            if (!ok1 || !ok2 || from_val < 0 || to_val < from_val) {
                setStatus(tr("Row %1: invalid range.").arg(row + 1), true);
                return;
            }
            req.range = {from_val, to_val};
        }

        requests.push_back(std::move(req));
    }

    if (requests.empty()) {
        setStatus(tr("Please enter at least one descriptor."), true);
        return;
    }

    const std::vector<wallet::ImportDescriptorResult> results = m_wallet_model->wallet().importDescriptors(requests);

    QString summary;
    bool any_failure = false;

    for (size_t i = 0; i < results.size(); ++i) {
        const wallet::ImportDescriptorResult& r = results[i];
        if (!r.success) {
            any_failure = true;
            summary += tr("Descriptor %1: FAILED — %2\n")
                .arg(i + 1)
                .arg(QString::fromStdString(r.error));
        } else {
            summary += tr("Descriptor %1: OK\n").arg(i + 1);
            for (const auto& w : r.warnings) {
                summary += tr("  Warning: %1\n").arg(QString::fromStdString(w));
            }
        }
    }

    setStatus(summary.trimmed(), any_failure);

    if (!any_failure) {
        QMessageBox::information(this, tr("Import Descriptors"),
            tr("All descriptors imported successfully."));
        accept();
    }
}

void ImportDescriptorsDialog::updateImportButtonState()
{
    for (int row = 0; row < m_ui->tableWidget->rowCount(); ++row) {
        auto* item = m_ui->tableWidget->item(row, COL_DESCRIPTOR);
        if (item && !item->text().trimmed().isEmpty()) {
            m_ui->importButton->setEnabled(true);
            return;
        }
    }
    m_ui->importButton->setEnabled(false);
}

void ImportDescriptorsDialog::setStatus(const QString& msg, bool is_error)
{
    m_ui->statusLabel->setText(msg);
    m_ui->statusLabel->setStyleSheet(is_error
        ? QStringLiteral("color: red;")
        : QStringLiteral("color: green;"));
}