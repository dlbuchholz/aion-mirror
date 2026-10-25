#ifndef RENAMEDPAYEESDIALOG_H
#define RENAMEDPAYEESDIALOG_H

#include <QDialog>
#include <QList>

#include "transaction.h"

class QCheckBox;
class QPushButton;

// After a rule's rename changed: lists the transactions the rule renamed and, for the ticked ones,
// restores their original names (the rule no longer renames) or gives them the rule's new name
class RenamedPayeesDialog : public QDialog {
    Q_OBJECT

public:
    // newPayee empty = restore the original names
    RenamedPayeesDialog(const QList<Transaction>& transactions, const QString& oldPayee, const QString& newPayee,
                        int ruleId, QWidget* parent = nullptr);

    bool restores() const { return newPayee.isEmpty(); }

private:
    void updateButton();
    void apply();

    QList<Transaction> transactions;
    QString newPayee;
    int ruleId;

    QList<QCheckBox*> checks;                        // One per transaction, in the same order
    QPushButton* applyButton = nullptr;
};

#endif // RENAMEDPAYEESDIALOG_H
