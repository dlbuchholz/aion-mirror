#include "accounttypessettings.h"
#include "account.h"
#include "db/databasemanager.h"
#include "theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QHash>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSqlQuery>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

// Accounts per type, so only unused types can be deleted
QHash<int, int> accountCounts() {
    QHash<int, int> counts;
    QSqlQuery query(DatabaseManager::instance().database());
    if (query.exec("SELECT type_id, COUNT(*) FROM accounts GROUP BY type_id")) {
        while (query.next()) counts.insert(query.value(0).toInt(), query.value(1).toInt());
    }
    return counts;
}

} // namespace

AccountTypesSettings::AccountTypesSettings(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    auto* heading = new QLabel("Account types", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    root->addWidget(heading);
    auto* hint = new QLabel("Edit a name to rename the type. Types that accounts still use can't be deleted. "
                            "Changes are saved right away.", this);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(10, 8, 10, 8);
    listLayout = new QVBoxLayout();
    listLayout->setSpacing(4);
    contentLayout->addLayout(listLayout);

    auto* addRow = new QHBoxLayout();
    newTypeEdit = new QLineEdit(content);
    newTypeEdit->setObjectName("newAccountTypeEdit");
    newTypeEdit->setPlaceholderText("New account type, for example Credit card");
    connect(newTypeEdit, &QLineEdit::returnPressed, this, &AccountTypesSettings::addType);
    auto* addButton = new QPushButton("Add type", content);
    addButton->setObjectName("addAccountTypeButton");
    connect(addButton, &QPushButton::clicked, this, &AccountTypesSettings::addType);
    addRow->addWidget(newTypeEdit, 1);
    addRow->addWidget(addButton);
    contentLayout->addSpacing(8);
    contentLayout->addLayout(addRow);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    errorLabel = new QLabel(this);
    errorLabel->setObjectName("accountTypeError");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    root->addWidget(errorLabel);

    reload();
}

void AccountTypesSettings::reload() {
    while (QLayoutItem* item = listLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();                   // Can run from a row's own button
        }
        delete item;
    }
    const QHash<int, int> counts = accountCounts();
    for (const auto& type : AccountDAO().getAllAccountTypes()) {
        const int typeId = type.first.toInt();
        const QString name = type.second;
        const int used = counts.value(typeId);

        auto* row = new QWidget(listLayout->parentWidget());
        row->setObjectName("accountTypeRow");
        row->setAccessibleName(name);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* edit = new QLineEdit(name, row);
        edit->setObjectName("accountTypeName");
        connect(edit, &QLineEdit::editingFinished, this, [this, typeId, edit, name]() { rename(typeId, edit, name); });
        layout->addWidget(edit, 1);
        auto* count = new QLabel(used == 0 ? QString("Not used") : QString("%1 %2").arg(used).arg(used == 1 ? "account" : "accounts"), row);
        count->setForegroundRole(QPalette::PlaceholderText);
        count->setMinimumWidth(90);
        layout->addWidget(count);
        auto* deleteButton = new QToolButton(row);
        deleteButton->setObjectName("deleteAccountTypeButton");
        deleteButton->setText("Delete");
        deleteButton->setAutoRaise(true);
        deleteButton->setEnabled(used == 0);
        deleteButton->setToolTip(used == 0 ? QString("Delete this account type")
                                     : QString("Accounts use this type. Change their type first."));
        connect(deleteButton, &QToolButton::clicked, this, [this, typeId]() { remove(typeId); });
        layout->addWidget(deleteButton);
        listLayout->addWidget(row);
    }
}

void AccountTypesSettings::showError(const QString& message) {
    QPalette palette = errorLabel->palette();
    palette.setColor(QPalette::WindowText, Theme::colors().danger);
    errorLabel->setPalette(palette);
    errorLabel->setText(message);
    errorLabel->setVisible(!message.isEmpty());
}

void AccountTypesSettings::addType() {
    const QString name = newTypeEdit->text().simplified();
    if (name.isEmpty()) {
        showError("Enter a name for the new account type.");
        return;
    }
    if (AccountDAO().addAccountType(name) <= 0) {
        showError(QString("\"%1\" couldn't be added. Is there already a type with that name?").arg(name));
        return;
    }
    showError(QString());
    newTypeEdit->clear();
    reload();
    emit accountTypesChanged();
}

void AccountTypesSettings::rename(int typeId, QLineEdit* edit, const QString& oldName) {
    const QString name = edit->text().simplified();
    if (name == oldName) {
        return;
    }
    if (name.isEmpty() || !AccountDAO().editAccountType(typeId, name)) {
        showError(name.isEmpty() ? QString("An account type needs a name.")
                                 : QString("\"%1\" couldn't be saved. Is there already a type with that name?").arg(name));
        edit->setText(oldName);
        return;
    }
    showError(QString());
    reload();
    emit accountTypesChanged();
}

void AccountTypesSettings::remove(int typeId) {
    if (!AccountDAO().deleteAccountType(typeId)) {
        showError("The account type couldn't be deleted.");
        return;
    }
    showError(QString());
    reload();
    emit accountTypesChanged();
}
