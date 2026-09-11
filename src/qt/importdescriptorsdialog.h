// Copyright (c) 2026-present The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_IMPORTDESCRIPTORSDIALOG_H
#define BITCOIN_QT_IMPORTDESCRIPTORSDIALOG_H

#include <QDialog>

class WalletModel;

namespace Ui {
class ImportDescriptorsDialog;
}

class ImportDescriptorsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ImportDescriptorsDialog(WalletModel* wallet_model, QWidget* parent = nullptr);
    ~ImportDescriptorsDialog();

private Q_SLOTS:
    void on_addButton_clicked();
    void on_removeButton_clicked();
    void on_importButton_clicked();
    void on_clearButton_clicked();
    void updateImportButtonState();

private:
    Ui::ImportDescriptorsDialog* m_ui;
    WalletModel* m_wallet_model;

    void setStatus(const QString& msg, bool is_error = false);
};

#endif // BITCOIN_QT_IMPORTDESCRIPTORSDIALOG_H