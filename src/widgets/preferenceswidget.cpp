#include "preferenceswidget.h"

#include <QComboBox>
#include <QInputDialog>
#include <QMessageBox>

PreferencesWidget::PreferencesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::PreferencesWidget)
{
    ui->setupUi(this);

    // Setup Save/Discard Buttons
    connect(ui->saveButton, &QPushButton::clicked, this, &PreferencesWidget::saveChanges);
    connect(ui->discardButton, &QPushButton::clicked, this, &PreferencesWidget::discardChanges);

    // Setup tables and load data
    setupTables();
    loadAccountTypes();
    loadCategories();
}

PreferencesWidget::~PreferencesWidget() {
    delete ui;
}

void PreferencesWidget::setupTables() {
    // Account Types Table
    ui->accountTypesTable->setColumnCount(1);  // Only one column for "Name"
    ui->accountTypesTable->setHorizontalHeaderLabels({"Name"});
    ui->accountTypesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->accountTypesTable->verticalHeader()->setVisible(true);  // Show the vertical header (to display IDs)

    // Connect signal for staging changes
    connect(ui->accountTypesTable, &QTableWidget::cellChanged, this, &PreferencesWidget::stageAccountTypeChange);
    connect(ui->addAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::addAccountType);
    connect(ui->deleteAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::deleteAccountType);

    // Categories Table
    ui->categoriesTable->setColumnCount(2);  // Two columns: "Name", "Type"
    ui->categoriesTable->setHorizontalHeaderLabels({"Name", "Type"});
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->categoriesTable->verticalHeader()->setVisible(true);  // Show the vertical header (to display IDs)

    // Connect for staging changes in the "Name" column immediately after edits
    connect(ui->categoriesTable, &QTableWidget::cellChanged, this, &PreferencesWidget::stageCategoryChange);
    connect(ui->addCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::addCategory);
    connect(ui->deleteCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::deleteCategory);

    // Initial set-up of button states
    updateButtonStates();
}

void PreferencesWidget::loadAccountTypes() {
    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(true);

    QList<QPair<QString, QString>> accountTypes = accountDao.getAllAccountTypes();

    ui->accountTypesTable->setRowCount(0);  // Clear existing rows
    for (const auto &accountType : accountTypes) {
        int row = ui->accountTypesTable->rowCount();
        ui->accountTypesTable->insertRow(row);

        // Set the ID as the vertical header
        QTableWidgetItem *idHeaderItem = new QTableWidgetItem(accountType.first);
        ui->accountTypesTable->setVerticalHeaderItem(row, idHeaderItem);

        // Add Name field
        QTableWidgetItem *nameItem = new QTableWidgetItem(accountType.second);
        ui->accountTypesTable->setItem(row, 0, nameItem);
    }

    // Reconnect the cellChanged signal
    ui->accountTypesTable->blockSignals(false);
}

void PreferencesWidget::loadCategories() {
    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(true);

    QList<Category> categories = categoryDao.getAllCategories();

    ui->categoriesTable->setRowCount(0);  // Clear existing rows
    for (const Category &category : categories) {
        int row = ui->categoriesTable->rowCount();
        ui->categoriesTable->insertRow(row);

        // Set the ID as the vertical header
        QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString::number(category.getCategoryId()));
        ui->categoriesTable->setVerticalHeaderItem(row, idHeaderItem);

        // Add Name field
        QTableWidgetItem *nameItem = new QTableWidgetItem(category.getName());
        ui->categoriesTable->setItem(row, 0, nameItem);

        // Add Type dropdown as a cell widget
        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems({"income", "expense"});
        typeCombo->setCurrentText(category.getType());
        connect(typeCombo, &QComboBox::currentTextChanged, [=]() {
            stageCategoryChange(row);  // Stage edits when Type is changed
        });
        ui->categoriesTable->setCellWidget(row, 1, typeCombo);
    }

    // Reconnect the cellChanged signal
    ui->categoriesTable->blockSignals(false);
}

void PreferencesWidget::addAccountType() {
    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(true);

    int row = ui->accountTypesTable->rowCount();
    ui->accountTypesTable->insertRow(row);

    // Add vertical header (ID) for the new row (set to "-1" for new entries)
    QTableWidgetItem *idHeaderItem = new QTableWidgetItem("-1");  // Default ID for new rows
    ui->accountTypesTable->setVerticalHeaderItem(row, idHeaderItem);

    // Add editable Name field
    QTableWidgetItem *nameItem = new QTableWidgetItem();
    nameItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);  // Editable flag
    ui->accountTypesTable->setItem(row, 0, nameItem);

    // Immediately stage the new row with default values
    stagedAccounts[-1] = "";  // Staged as (TempID -> Empty Name)
    updateButtonStates();     // Enable buttons

    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(false);
}

void PreferencesWidget::deleteAccountType() {
    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(true);

    int row = ui->accountTypesTable->currentRow();
    if (row == -1) {
        QMessageBox::warning(this, "No Selection", "Please select a row to delete.");
        return;
    }

    // Fetch the ID from the vertical header
    QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();
    int accountId = id.toInt();

    // If the ID is not -1 (i.e., it's not a new row), stage it for deletion
    if (accountId != -1) {
        stagedAccountDeletions.insert(accountId);
        stagedAccounts.remove(accountId);  // Remove from staging, since it's being deleted
    } else {
        // If it's a new row that hasn't been saved, just remove it from staging
        stagedAccounts.remove(accountId);
    }

    // Remove the row from the UI
    ui->accountTypesTable->removeRow(row);

    // Update Save/Discard button visibility
    updateButtonStates();

    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(false);
}

void PreferencesWidget::addCategory() {
    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(true);

    int row = ui->categoriesTable->rowCount();
    ui->categoriesTable->insertRow(row);

    // Add ID to vertical header (default to "-1")
    QTableWidgetItem *idHeaderItem = new QTableWidgetItem("-1");
    ui->categoriesTable->setVerticalHeaderItem(row, idHeaderItem);

    // Add editable Name field
    QTableWidgetItem *nameItem = new QTableWidgetItem();
    nameItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
    ui->categoriesTable->setItem(row, 0, nameItem);

    // Add Type dropdown (default to "income")
    QComboBox *typeCombo = new QComboBox();
    typeCombo->addItems({"income", "expense"});
    connect(typeCombo, &QComboBox::currentTextChanged, [=]() {
        stageCategoryChange(row);
    });
    ui->categoriesTable->setCellWidget(row, 1, typeCombo);

    stagedCategories[-1] = qMakePair("", "income");  // Default staged entry (TempID -> <Empty, income>)
    updateButtonStates();  // Enable buttons

    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(false);
}

void PreferencesWidget::deleteCategory() {
    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(true);

    int row = ui->categoriesTable->currentRow();
    if (row == -1) {
        QMessageBox::warning(this, "No Selection", "Please select a row to delete.");
        return;
    }

    // Fetch the ID from the vertical header
    QString id = ui->categoriesTable->verticalHeaderItem(row)->text();
    int categoryId = id.toInt();

    // If the ID is not -1 (i.e., it's not a new row), stage it for deletion
    if (categoryId != -1) {
        stagedCategoryDeletions.insert(categoryId);
        stagedCategories.remove(categoryId);  // Remove from staging, since it's being deleted
    } else {
        // If it's a new row that hasn't been saved, just remove it from staging
        stagedCategories.remove(categoryId);
    }

    // Remove the row from the UI
    ui->categoriesTable->removeRow(row);

    // Update Save/Discard button visibility
    updateButtonStates();

    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(false);
}

void PreferencesWidget::stageAccountTypeChange(int row) {
    QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();  // ID from vertical header
    QString name = ui->accountTypesTable->item(row, 0)->text();           // Name from column 0
    stagedAccounts[id.toInt()] = name;  // Staged as (ID -> Name)
    updateButtonStates();  // Enable buttons since a change was made
}

void PreferencesWidget::stageCategoryChange(int row) {
    QString id = ui->categoriesTable->verticalHeaderItem(row)->text();  // ID from vertical header
    QString name = ui->categoriesTable->item(row, 0)->text();           // Name from column 0
    QString type;
    if (QComboBox *typeCombo = qobject_cast<QComboBox *>(ui->categoriesTable->cellWidget(row, 1))) {
        type = typeCombo->currentText();
    }
    stagedCategories[id.toInt()] = qMakePair(name, type);  // Staged as (ID -> <Name, Type>)
    updateButtonStates();  // Enable buttons since a change was made
}

void PreferencesWidget::saveChanges() {
    // Save staged Account Type changes
    for (auto it = stagedAccounts.begin(); it != stagedAccounts.end(); ++it) {
        int id = it.key();
        QString name = it.value();

        if (id == -1) {
            // New account type (ID == -1), use the "add" method
            int newId = accountDao.addAccountType(name);
            if (newId != -1) {
                qDebug() << "Added new Account Type to DB with ID:" << newId;
            } else {
                qWarning() << "Failed to add new Account Type to DB";
            }
        } else {
            // Existing account type, update the name
            if (!accountDao.editAccountType(id, name)) {
                qWarning() << "Failed to update Account Type with ID:" << id;
            }
        }
    }

    // Save staged Category changes
    for (auto it = stagedCategories.begin(); it != stagedCategories.end(); ++it) {
        int id = it.key();
        QString name = it.value().first;
        QString type = it.value().second;

        if (id == -1) {
            // New category (ID == -1), use the "create" method
            Category newCategory(-1, name, type);  // Use a temporary ID for insertion
            int newId = categoryDao.addCategory(newCategory);
            if (newId != -1) {
                qDebug() << "Added new Category to DB with ID:" << newId;
            } else {
                qWarning() << "Failed to add new Category to DB";
            }
        } else {
            // Existing category, update the name and type
            if (!categoryDao.editCategory(id, name, type)) {
                qWarning() << "Failed to update Category with ID:" << id;
            }
        }
    }

    // Handle account deletions
    for (int accountId : stagedAccountDeletions) {
        if (!accountDao.deleteAccountType(accountId)) {
            qWarning() << "Failed to delete Account Type with ID:" << accountId;
        } else {
            qDebug() << "Deleted Account Type with ID:" << accountId;
        }
    }

    // Handle category deletions
    for (int categoryId : stagedCategoryDeletions) {
        if (!categoryDao.deleteCategory(categoryId)) {
            qWarning() << "Failed to delete Category with ID:" << categoryId;
        } else {
            qDebug() << "Deleted Category with ID:" << categoryId;
        }
    }

    // Clear staging for accounts and categories
    stagedAccounts.clear();
    stagedCategories.clear();
    stagedAccountDeletions.clear();
    stagedCategoryDeletions.clear();

    // Reload tables to reflect the actual database state
    loadAccountTypes();
    loadCategories();
    updateButtonStates();
}

void PreferencesWidget::discardChanges() {
    stagedAccounts.clear();
    stagedCategories.clear();
    loadAccountTypes();
    loadCategories();
    updateButtonStates();  // Disable buttons now that changes are discarded
}

void PreferencesWidget::updateButtonStates() {
    // Check if there are any changes: added, edited, or deleted
    bool hasChanges = !(stagedAccounts.isEmpty() && stagedCategories.isEmpty() &&
                        stagedAccountDeletions.isEmpty() && stagedCategoryDeletions.isEmpty());

    // Show or hide the Save and Discard buttons based on changes
    ui->saveButton->setVisible(hasChanges);
    ui->discardButton->setVisible(hasChanges);
}
