// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/importdescriptorsdialog.h>
#include <qt/forms/ui_importdescriptorsdialog.h>

#include <interfaces/wallet.h>
#include <qt/guiutil.h>
#include <qt/walletmodel.h>
#include <wallet/imports.h>

#include <QCheckBox>
#include <QMessageBox>
#include <QTableWidgetItem>

#include <utility>

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
    std::vector<wallet::ImportDescriptorRequest> requests;

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

        wallet::ImportDescriptorRequest req;
        req.descriptor = desc.toStdString();
        // `internal` is std::optional<bool>; leave it unset (nullopt) when the
        // box is unchecked so that multipath descriptors can be imported.
        if (checked(COL_INTERNAL)) {
            req.internal = true;
        }
        req.active = checked(COL_ACTIVE);

        // label is a plain std::string (empty means no label)
        const QString label_str = text(COL_LABEL);
        if (!label_str.isEmpty()) {
            req.label = label_str.toStdString();
        }

        // Timestamp: std::optional<int64_t>. An unset (nullopt) timestamp means
        // "now" (the wallet substitutes the current time). Never use a sentinel
        // like -1, which would be clamped to 0 and trigger a full rescan.
        const QString ts_str = text(COL_TIMESTAMP);
        if (!ts_str.isEmpty() && ts_str != QLatin1String("now")) {
            bool ok = false;
            const int64_t ts = ts_str.toLongLong(&ok);
            if (!ok || ts < 0) {
                setStatus(tr("Row %1: invalid timestamp. Use a Unix timestamp or \"now\".").arg(row + 1), true);
                return;
            }
            req.timestamp = ts;
        }

        // Range
        const QString from_str = text(COL_RANGE_FROM);
        const QString to_str   = text(COL_RANGE_TO);
        if (!from_str.isEmpty() || !to_str.isEmpty()) {
            bool ok1 = false, ok2 = false;
            const int64_t from_val = from_str.toLongLong(&ok1);
            const int64_t to_val   = to_str.toLongLong(&ok2);
            if (!ok1 || !ok2 || from_val < 0 || to_val < from_val) {
                setStatus(tr("Row %1: invalid range.").arg(row + 1), true);
                return;
            }
            req.range = std::make_pair(from_val, to_val);
        }

        requests.push_back(std::move(req));
    }

    if (requests.empty()) {
        setStatus(tr("Please enter at least one descriptor."), true);
        return;
    }

    const std::vector<wallet::ImportResult> results = m_wallet_model->wallet().importDescriptors(requests);

    // A wallet-wide precondition failure (e.g. the wallet is locked, or a
    // rescan is already in progress) is reported as a single result whose
    // error is marked as general. Surface it as a top-level error rather than
    // a per-descriptor failure.
    if (results.size() == 1 && results.front().has_error() && results.front().error->is_general_error) {
        const QString msg = QString::fromStdString(results.front().error->wallet_error.message.translated);
        setStatus(msg, true);
        QMessageBox::warning(this, tr("Import Descriptors"), msg);
        return;
    }

    QString summary;
    bool any_failure = false;

    for (size_t i = 0; i < results.size(); ++i) {
        const wallet::ImportResult& r = results[i];
        if (r.has_error()) {
            any_failure = true;
            summary += tr("Descriptor %1: FAILED — %2\n")
                .arg(i + 1)
                .arg(QString::fromStdString(r.error->wallet_error.message.translated));
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