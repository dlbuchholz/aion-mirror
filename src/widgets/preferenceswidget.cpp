#include "preferenceswidget.h"
#include "theme.h"
#include "../../ui/ui_preferenceswidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTimer>

namespace {

constexpr int ORIGINAL_NAME_ROLE = Qt::UserRole;     // Name as stored in the database

QWidget* makeCheckboxCell() {
    // Only selects the row for "Delete selected"; nothing is staged until then
    auto *cell = new QWidget();
    auto *layout = new QHBoxLayout(cell);
    layout->addWidget(new QCheckBox());
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    return cell;
}

} // namespace

PreferencesWidget::PreferencesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PreferencesWidget)
{
    ui->setupUi(this);

    // Saving and discarding is driven by the settings page (see hasPendingChanges())

    // Hints under each table (renamed duplicates, empty names)
    accountTypesNotice = new QLabel(this);
    categoriesNotice = new QLabel(this);
    for (QLabel *notice : {accountTypesNotice, categoriesNotice}) {
        notice->setObjectName(notice == categoriesNotice ? "categoriesNotice" : "accountTypesNotice");
        notice->setWordWrap(true);
        notice->hide();
    }
    ui->accountTypesLayout->addWidget(accountTypesNotice);
    ui->categoriesLayout->addWidget(categoriesNotice);

    setupTables();
    loadAccountTypes();
    loadCategories();
}

PreferencesWidget::~PreferencesWidget() {
    delete ui;
}

void PreferencesWidget::setupTables() {
    // Account Types Table
    ui->accountTypesTable->setColumnCount(2);  // Name + Checkbox
    ui->accountTypesTable->setHorizontalHeaderLabels({"Name", "Select"});
    ui->accountTypesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->accountTypesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->accountTypesTable->verticalHeader()->setVisible(false);  // Holds the IDs, not meant for display
    connect(ui->accountTypesTable, &QTableWidget::cellChanged, this, &PreferencesWidget::stageAccountTypeChange);
    connect(ui->addAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::addAccountType);
    connect(ui->deleteAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::deleteAccountType);

    // Categories Table
    ui->categoriesTable->setColumnCount(3);  // Name + Type + Checkbox
    ui->categoriesTable->setHorizontalHeaderLabels({"Name", "Type", "Select"});
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->categoriesTable->verticalHeader()->setVisible(false);  // Holds the IDs, not meant for display
    connect(ui->categoriesTable, &QTableWidget::cellChanged, this, &PreferencesWidget::stageCategoryChange);
    connect(ui->addCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::addCategory);
    connect(ui->deleteCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::deleteCategory);

    updateButtonStates();
}

// --- Loading -----------------------------------------------------------------

void PreferencesWidget::loadAccountTypes() {
    const QSignalBlocker blocker(ui->accountTypesTable);
    ui->accountTypesTable->setRowCount(0);
    for (const auto &accountType : accountDao.getAllAccountTypes()) {
        addAccountTypeRow(accountType.first.toInt(), accountType.second);
    }
}

void PreferencesWidget::loadCategories() {
    const QSignalBlocker blocker(ui->categoriesTable);
    ui->categoriesTable->setRowCount(0);
    for (const Category &category : categoryDao.getAllCategories()) {
        addCategoryRow(category.getCategoryId(), category.getName(), category.getType());
    }
}

void PreferencesWidget::addAccountTypeRow(int id, const QString& name) {
    QTableWidget *table = ui->accountTypesTable;
    const QSignalBlocker blocker(table);
    const int row = table->rowCount();
    table->insertRow(row);
    table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(id)));

    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setData(ORIGINAL_NAME_ROLE, name);
    table->setItem(row, 0, nameItem);
    table->setCellWidget(row, 1, makeCheckboxCell());
}

void PreferencesWidget::addCategoryRow(int id, const QString& name, const QString& type) {
    QTableWidget *table = ui->categoriesTable;
    const QSignalBlocker blocker(table);
    const int row = table->rowCount();
    table->insertRow(row);
    table->setVerticalHeaderItem(row, new QTableWidgetItem(QString::number(id)));

    auto *nameItem = new QTableWidgetItem(name);
    nameItem->setData(ORIGINAL_NAME_ROLE, name);
    table->setItem(row, 0, nameItem);

    auto *typeCombo = new QComboBox();
    typeCombo->addItems({"income", "expense"});
    typeCombo->setCurrentText(type);
    // Look the row up when the type changes; rows move when others are deleted
    connect(typeCombo, &QComboBox::currentTextChanged, this, [this, typeCombo]() {
        const int currentRow = rowForCellWidget(ui->categoriesTable, 1, typeCombo);
        if (currentRow >= 0) stageCategoryChange(currentRow);
    });
    table->setCellWidget(row, 1, typeCombo);
    table->setCellWidget(row, 2, makeCheckboxCell());
}

// --- Adding and deleting -------------------------------------------------------

void PreferencesWidget::addAccountType() {
    const int id = nextTempId--;
    addAccountTypeRow(id, QString());
    stagedAccounts[id] = QString();
    updateButtonStates();

    // Start typing the name right away
    const int row = ui->accountTypesTable->rowCount() - 1;
    ui->accountTypesTable->setCurrentCell(row, 0);
    if (ui->accountTypesTable->isVisible()) ui->accountTypesTable->editItem(ui->accountTypesTable->item(row, 0));
}

void PreferencesWidget::addCategory() {
    const int id = nextTempId--;
    addCategoryRow(id, QString(), "expense");
    stagedCategories[id] = qMakePair(QString(), QString("expense"));
    updateButtonStates();

    const int row = ui->categoriesTable->rowCount() - 1;
    ui->categoriesTable->setCurrentCell(row, 0);
    if (ui->categoriesTable->isVisible()) ui->categoriesTable->editItem(ui->categoriesTable->item(row, 0));
}

void PreferencesWidget::deleteAccountType() {
    QTableWidget *table = ui->accountTypesTable;
    const QSignalBlocker blocker(table);
    for (int row : checkedRows(table, 1)) {      // Highest row first
        const int id = idForRow(table, row);
        if (id > 0) {
            stagedAccountDeletions[id] = table->item(row, 0)->data(ORIGINAL_NAME_ROLE).toString();
        }
        stagedAccounts.remove(id);
        table->removeRow(row);
    }
    updateButtonStates();
}

void PreferencesWidget::deleteCategory() {
    QTableWidget *table = ui->categoriesTable;
    const QSignalBlocker blocker(table);
    for (int row : checkedRows(table, 2)) {      // Highest row first
        const int id = idForRow(table, row);
        if (id > 0) {
            stagedCategoryDeletions[id] = table->item(row, 0)->data(ORIGINAL_NAME_ROLE).toString();
        }
        stagedCategories.remove(id);
        table->removeRow(row);
    }
    updateButtonStates();
}

// --- Editing -------------------------------------------------------------------

void PreferencesWidget::stageAccountTypeChange(int row) {
    QTableWidget *table = ui->accountTypesTable;
    if (row < 0 || row >= table->rowCount() || !table->item(row, 0)) {
        return;
    }
    const int id = idForRow(table, row);
    QString name = table->item(row, 0)->text().trimmed();

    if (name.isEmpty() && id > 0) {
        // Existing entries keep their name
        const QSignalBlocker blocker(table);
        table->item(row, 0)->setText(table->item(row, 0)->data(ORIGINAL_NAME_ROLE).toString());
        showNotice(accountTypesNotice, "Names can't be empty.");
        stagedAccounts.remove(id);
        updateButtonStates();
        return;
    }
    if (!name.isEmpty() && isNameTaken(table, name, row)) {
        const QString renamed = uniqueName(table, name, row);
        const QSignalBlocker blocker(table);
        table->item(row, 0)->setText(renamed);
        showNotice(accountTypesNotice, QString("\"%1\" already exists, so it was renamed to \"%2\".").arg(name, renamed));
        name = renamed;
    }

    stagedAccounts[id] = name;
    updateButtonStates();
}

void PreferencesWidget::stageCategoryChange(int row) {
    QTableWidget *table = ui->categoriesTable;
    if (row < 0 || row >= table->rowCount() || !table->item(row, 0)) {
        return;
    }
    const int id = idForRow(table, row);
    QString name = table->item(row, 0)->text().trimmed();
    QString type = "expense";
    if (auto *typeCombo = qobject_cast<QComboBox *>(table->cellWidget(row, 1))) {
        type = typeCombo->currentText();
    }

    if (name.isEmpty() && id > 0) {
        const QSignalBlocker blocker(table);
        table->item(row, 0)->setText(table->item(row, 0)->data(ORIGINAL_NAME_ROLE).toString());
        showNotice(categoriesNotice, "Names can't be empty.");
        name = table->item(row, 0)->text();
    } else if (!name.isEmpty() && isNameTaken(table, name, row)) {
        const QString renamed = uniqueName(table, name, row);
        const QSignalBlocker blocker(table);
        table->item(row, 0)->setText(renamed);
        showNotice(categoriesNotice, QString("\"%1\" already exists, so it was renamed to \"%2\".").arg(name, renamed));
        name = renamed;
    }

    stagedCategories[id] = qMakePair(name, type);
    updateButtonStates();
}

// --- Saving --------------------------------------------------------------------

bool PreferencesWidget::saveChanges() {
    errors.clear();

    // Deletions first, so their names can be reused by renames and new entries
    for (auto it = stagedAccountDeletions.cbegin(); it != stagedAccountDeletions.cend(); ++it) {
        if (!accountDao.deleteAccountType(it.key())) {
            errors << QString("Couldn't delete the account type \"%1\". It's still used by an account.").arg(it.value());
        }
    }
    for (auto it = stagedCategoryDeletions.cbegin(); it != stagedCategoryDeletions.cend(); ++it) {
        if (!categoryDao.deleteCategory(it.key())) {
            errors << QString("Couldn't delete the category \"%1\". It's still used by transactions or budgets.").arg(it.value());
        }
    }

    for (auto it = stagedAccounts.cbegin(); it != stagedAccounts.cend(); ++it) {
        const QString name = it.value().trimmed();
        if (name.isEmpty()) {
            continue;                                // A new row nobody typed a name into
        }
        if (it.key() < 0) {
            if (accountDao.addAccountType(name) == -1) {
                errors << QString("Couldn't add the account type \"%1\".").arg(name);
            }
        } else if (!accountDao.editAccountType(it.key(), name)) {
            errors << QString("Couldn't rename the account type to \"%1\".").arg(name);
        }
    }

    for (auto it = stagedCategories.cbegin(); it != stagedCategories.cend(); ++it) {
        const QString name = it.value().first.trimmed();
        const QString type = it.value().second;
        if (name.isEmpty()) {
            continue;
        }
        if (it.key() < 0) {
            if (categoryDao.addCategory(Category(-1, name, type)) == -1) {
                errors << QString("Couldn't add the category \"%1\".").arg(name);
            }
        } else if (!categoryDao.editCategory(it.key(), name, type)) {
            errors << QString("Couldn't change the category \"%1\". Budgets may still use its old name.").arg(name);
        }
    }

    stagedAccounts.clear();
    stagedCategories.clear();
    stagedAccountDeletions.clear();
    stagedCategoryDeletions.clear();

    // Show what the database actually holds now
    loadAccountTypes();
    loadCategories();
    updateButtonStates();
    return errors.isEmpty();
}

void PreferencesWidget::discardChanges() {
    stagedAccounts.clear();
    stagedCategories.clear();
    stagedAccountDeletions.clear();
    stagedCategoryDeletions.clear();
    accountTypesNotice->hide();
    categoriesNotice->hide();
    loadAccountTypes();
    loadCategories();
    updateButtonStates();
}

QStringList PreferencesWidget::lastErrors() const {
    return errors;
}

bool PreferencesWidget::hasPendingChanges() const {
    return !(stagedAccounts.isEmpty() && stagedCategories.isEmpty() &&
             stagedAccountDeletions.isEmpty() && stagedCategoryDeletions.isEmpty());
}

void PreferencesWidget::updateButtonStates() {
    emit pendingChangesChanged(hasPendingChanges());
}

// --- Helpers -------------------------------------------------------------------

int PreferencesWidget::idForRow(QTableWidget *table, int row) {
    const QTableWidgetItem *header = table->verticalHeaderItem(row);
    return header ? header->text().toInt() : 0;
}

int PreferencesWidget::rowForCellWidget(QTableWidget *table, int column, const QWidget *widget) {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->cellWidget(row, column) == widget) {
            return row;
        }
    }
    return -1;
}

QList<int> PreferencesWidget::checkedRows(QTableWidget *table, int checkboxColumn) {
    QList<int> rows;
    for (int row = table->rowCount() - 1; row >= 0; --row) {
        const QWidget *cell = table->cellWidget(row, checkboxColumn);
        const QCheckBox *checkBox = cell ? cell->findChild<QCheckBox *>() : nullptr;
        if (checkBox && checkBox->isChecked()) {
            rows.append(row);
        }
    }
    return rows;
}

bool PreferencesWidget::isNameTaken(QTableWidget *table, const QString& name, int exceptRow) {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (row != exceptRow && table->item(row, 0)
            && table->item(row, 0)->text().trimmed().compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString PreferencesWidget::uniqueName(QTableWidget *table, const QString& baseName, int exceptRow) {
    QString candidate = baseName;
    for (int counter = 2; isNameTaken(table, candidate, exceptRow); ++counter) {
        candidate = QString("%1 %2").arg(baseName).arg(counter);
    }
    return candidate;
}

void PreferencesWidget::showNotice(QLabel *label, const QString& message) {
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, Theme::colors().textSecondary);
    label->setPalette(palette);
    label->setText(message);
    label->show();
    QTimer::singleShot(6000, label, &QLabel::hide);
}
