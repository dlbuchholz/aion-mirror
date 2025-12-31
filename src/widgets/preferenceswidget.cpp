#include "preferenceswidget.h"

#include <QCheckBox>
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
    ui->accountTypesTable->setColumnCount(2);  // Name + Checkbox
    ui->accountTypesTable->setHorizontalHeaderLabels({"Name", "Select"});

    // Set column resize modes
    ui->accountTypesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Name column stretches
    ui->accountTypesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);  // Checkbox column fits content
    ui->accountTypesTable->setColumnWidth(1, 30);  // Set a fixed small width for checkbox column

    ui->accountTypesTable->verticalHeader()->setVisible(true);
    connect(ui->accountTypesTable, &QTableWidget::cellChanged, this, &PreferencesWidget::stageAccountTypeChange);
    connect(ui->addAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::addAccountType);
    connect(ui->deleteAccountTypeButton, &QPushButton::clicked, this, &PreferencesWidget::deleteAccountType);

    // Categories Table
    ui->categoriesTable->setColumnCount(3);  // Name + Type + Checkbox
    ui->categoriesTable->setHorizontalHeaderLabels({"Name", "Type", "Select"});

    // Set column resize modes
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);  // Name column stretches
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView:: Stretch);  // Type column stretches
    ui->categoriesTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);  // Checkbox column fits content
    ui->categoriesTable->setColumnWidth(2, 30);  // Set a fixed small width for checkbox column

    ui->categoriesTable->verticalHeader()->setVisible(true);
    connect(ui->categoriesTable, &QTableWidget:: cellChanged, this, &PreferencesWidget::stageCategoryChange);
    connect(ui->addCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::addCategory);
    connect(ui->deleteCategoryButton, &QPushButton::clicked, this, &PreferencesWidget::deleteCategory);

    // Initial set-up of button states
    updateButtonStates();
}

void PreferencesWidget::loadAccountTypes() {
    // Temporarily block signals
    ui->accountTypesTable->blockSignals(true);
    ui->accountTypesTable->setRowCount(0);  // Clear previous rows

    QList<QPair<QString, QString>> accountTypes = accountDao.getAllAccountTypes();
    for (const auto &accountType : accountTypes) {
        int row = ui->accountTypesTable->rowCount();
        ui->accountTypesTable->insertRow(row);

        // Set the ID in the vertical header (read-only)
        QTableWidgetItem *idHeaderItem = new QTableWidgetItem(accountType.first);
        idHeaderItem->setFlags(idHeaderItem->flags() & ~Qt::ItemIsEditable);  // ID column is read-only
        ui->accountTypesTable->setVerticalHeaderItem(row, idHeaderItem);

        // Add Name column (editable)
        QTableWidgetItem *nameItem = new QTableWidgetItem(accountType.second);
        nameItem->setFlags(Qt::ItemIsEditable | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        ui->accountTypesTable->setItem(row, 0, nameItem);

        // Add a checkbox in the first column (selection column)
            QCheckBox *checkBox = new QCheckBox();
        connect(checkBox, &QCheckBox::clicked, [=]() {
            handleAccountTypeCheckboxStateChange(row, checkBox->isChecked());
        });

        QWidget *checkBoxWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
        layout->addWidget(checkBox);
        layout->setAlignment(Qt::AlignCenter);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->accountTypesTable->setCellWidget(row, 1, checkBoxWidget);
    }

    // Reconnect signals
    ui->accountTypesTable->blockSignals(false);
}

void PreferencesWidget::loadCategories() {
    // Temporarily block signals
    ui->categoriesTable->blockSignals(true);
    ui->categoriesTable->setRowCount(0);  // Clear previous rows

    QList<Category> categories = categoryDao.getAllCategories();
    for (const Category &category : categories) {
        int row = ui->categoriesTable->rowCount();
        ui->categoriesTable->insertRow(row);

        // Set the ID in the vertical header (read-only)
        QTableWidgetItem *idHeaderItem = new QTableWidgetItem(QString::number(category.getCategoryId()));
        idHeaderItem->setFlags(idHeaderItem->flags() & ~Qt::ItemIsEditable);  // ID column is read-only
        ui->categoriesTable->setVerticalHeaderItem(row, idHeaderItem);

        // Add Name column (editable)
        QTableWidgetItem *nameItem = new QTableWidgetItem(category.getName());
        ui->categoriesTable->setItem(row, 0, nameItem);

        // Add Type dropdown
        QComboBox *typeCombo = new QComboBox();
        typeCombo->addItems({"income", "expense"});
        typeCombo->setCurrentText(category.getType());
        connect(typeCombo, &QComboBox::currentTextChanged, [=]() {
            stageCategoryChange(row);
        });
        ui->categoriesTable->setCellWidget(row, 1, typeCombo);

        // Add a checkbox in the last column
        QCheckBox *checkBox = new QCheckBox();
        connect(checkBox, &QCheckBox::clicked, [=]() {
            handleCategoryCheckboxStateChange(row, checkBox->isChecked());
        });
        QWidget *checkBoxWidget = new QWidget();
        QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
        layout->addWidget(checkBox);
        layout->setAlignment(Qt::AlignCenter);
        layout->setContentsMargins(0, 0, 0, 0);
        ui->categoriesTable->setCellWidget(row, 2, checkBoxWidget);
    }

    // Reconnect signals
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

    // Add checkbox
    QCheckBox *checkBox = new QCheckBox();
    connect(checkBox, &QCheckBox::clicked, [=]() {
        handleAccountTypeCheckboxStateChange(row, checkBox->isChecked());
    });
    QWidget *checkBoxWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
    layout->addWidget(checkBox);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    ui->accountTypesTable->setCellWidget(row, 1, checkBoxWidget);

    // Immediately stage the new row with default values
    stagedAccounts[-1] = "";  // Staged as (TempID -> Empty Name)
    updateButtonStates();     // Enable buttons

    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(false);
}

void PreferencesWidget::deleteAccountType() {
    // Temporarily block the cellChanged signal
    ui->accountTypesTable->blockSignals(true);

    // Check all rows for checked checkboxes and stage them for deletion
    for (int row = ui->accountTypesTable->rowCount() - 1; row >= 0; --row) {
        QWidget *widget = ui->accountTypesTable->cellWidget(row, 1);
        if (widget) {
            QCheckBox *checkBox = widget->findChild<QCheckBox *>();
            if (checkBox && checkBox->isChecked()) {
                QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();
                int accountId = id.toInt();

                if (accountId != -1) {
                    stagedAccountDeletions.insert(accountId);
                }
                stagedAccounts.remove(accountId);

                // Remove the row from the UI
                ui->accountTypesTable->removeRow(row);
            }
        }
    }

    // Update Save/Discard button visibility
    updateButtonStates();

    // Re-enable signals
    ui->accountTypesTable->blockSignals(false);
}

bool PreferencesWidget:: isAccountTypeNameDuplicate(const QString& name, int excludeId) {
    // Check in the table
    for (int row = 0; row < ui->accountTypesTable->rowCount(); ++row) {
        QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();
        if (id.toInt() == excludeId) continue;  // Skip the row we're currently editing

        QString existingName = ui->accountTypesTable->item(row, 0)->text();
        if (existingName. toLower() == name.toLower()) {
            return true;
        }
    }
    return false;
}



QString PreferencesWidget::generateUniqueAccountTypeName(const QString& baseName) {
    QString uniqueName = baseName;
    int counter = 2;

    while (isAccountTypeNameDuplicate(uniqueName)) {
        uniqueName = baseName + " " + QString::number(counter);
        counter++;
    }

    return uniqueName;
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

    // Add checkbox
    QCheckBox *checkBox = new QCheckBox();
    connect(checkBox, &QCheckBox::clicked, [=]() {
        handleCategoryCheckboxStateChange(row, checkBox->isChecked());
    });
    QWidget *checkBoxWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(checkBoxWidget);
    layout->addWidget(checkBox);
    layout->setAlignment(Qt::AlignCenter);
    layout->setContentsMargins(0, 0, 0, 0);
    ui->categoriesTable->setCellWidget(row, 2, checkBoxWidget);

    stagedCategories[-1] = qMakePair("", "income");  // Default staged entry (TempID -> <Empty, income>)
    updateButtonStates();  // Enable buttons

    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(false);
}

void PreferencesWidget::deleteCategory() {
    // Temporarily block the cellChanged signal
    ui->categoriesTable->blockSignals(true);

    // Check all rows for checked checkboxes and stage them for deletion
    for (int row = ui->categoriesTable->rowCount() - 1; row >= 0; --row) {
        QWidget *widget = ui->categoriesTable->cellWidget(row, 2);
        if (widget) {
            QCheckBox *checkBox = widget->findChild<QCheckBox *>();
            if (checkBox && checkBox->isChecked()) {
                QString id = ui->categoriesTable->verticalHeaderItem(row)->text();
                int categoryId = id.toInt();

                if (categoryId != -1) {
                    stagedCategoryDeletions.insert(categoryId);
                }
                stagedCategories. remove(categoryId);

                // Remove the row from the UI
                ui->categoriesTable->removeRow(row);
            }
        }
    }

    // Update Save/Discard button visibility
    updateButtonStates();

    // Re-enable signals
    ui->categoriesTable->blockSignals(false);
}

bool PreferencesWidget::isCategoryNameDuplicate(const QString& name, int excludeId) {
    // Check in the table
    for (int row = 0; row < ui->categoriesTable->rowCount(); ++row) {
        QString id = ui->categoriesTable->verticalHeaderItem(row)->text();
        if (id.toInt() == excludeId) continue;  // Skip the row we're currently editing

        QString existingName = ui->categoriesTable->item(row, 0)->text();
        if (existingName.toLower() == name.toLower()) {
            return true;
        }
    }
    return false;
}

QString PreferencesWidget::generateUniqueCategoryName(const QString& baseName) {
    QString uniqueName = baseName;
    int counter = 2;

    while (isCategoryNameDuplicate(uniqueName)) {
        uniqueName = baseName + " " + QString::number(counter);
        counter++;
    }

    return uniqueName;
}

void PreferencesWidget::handleAccountTypeCheckboxStateChange(int row, bool checked) {
    QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();
    int accountId = id.toInt();

    if (checked) {
        // Add to staged deletions
        if (accountId != -1) {
            stagedAccountDeletions.insert(accountId);
        }
    } else {
        // Remove from staged deletions
        stagedAccountDeletions.remove(accountId);
    }

    // Update Save/Discard button visibility
    //updateButtonStates();
}

void PreferencesWidget::handleCategoryCheckboxStateChange(int row, bool checked) {
    QString id = ui->categoriesTable->verticalHeaderItem(row)->text();
    int categoryId = id.toInt();

    if (checked) {
        // Add to staged deletions
        if (categoryId != -1) {
            stagedCategoryDeletions.insert(categoryId);
        }
    } else {
        // Remove from staged deletions
        stagedCategoryDeletions.remove(categoryId);
    }

    // Update Save/Discard button visibility
    //updateButtonStates();
}

void PreferencesWidget::stageAccountTypeChange(int row) {
    QString id = ui->accountTypesTable->verticalHeaderItem(row)->text();  // ID from vertical header
    QString name = ui->accountTypesTable->item(row, 0)->text();           // Name from column 0

    // Check for duplicate names
    if (isAccountTypeNameDuplicate(name, id.toInt())) {
        // Temporarily block signals to avoid recursive calls
        ui->accountTypesTable->blockSignals(true);

        // Generate a unique name
        QString uniqueName = generateUniqueAccountTypeName(name);
        ui->accountTypesTable->item(row, 0)->setText(uniqueName);

        // Show a warning to the user
        QMessageBox:: warning(this, "Duplicate Name",
                             QString("An account type with the name '%1' already exists. The name has been changed to '%2'.")
                                 .arg(name).arg(uniqueName));

        name = uniqueName;

        // Re-enable signals
        ui->accountTypesTable->blockSignals(false);
    }

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

    // Check for duplicate names
    if (isCategoryNameDuplicate(name, id.toInt())) {
        // Temporarily block signals to avoid recursive calls
        ui->categoriesTable->blockSignals(true);

        // Generate a unique name
        QString uniqueName = generateUniqueCategoryName(name);
        ui->categoriesTable->item(row, 0)->setText(uniqueName);

        // Show a warning to the user
        QMessageBox::warning(this, "Duplicate Name",
                             QString("A category with the name '%1' already exists. The name has been changed to '%2'.")
                                 .arg(name).arg(uniqueName));

        name = uniqueName;

        // Re-enable signals
        ui->categoriesTable->blockSignals(false);
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
    stagedCategories. clear();
    stagedAccountDeletions.clear();
    stagedCategoryDeletions.clear();
    loadAccountTypes();
    loadCategories();
    updateButtonStates();
}
void PreferencesWidget::updateButtonStates() {
    // Check if there are any changes: added, edited, or deleted
    bool hasChanges = !(stagedAccounts.isEmpty() && stagedCategories.isEmpty() &&
                        stagedAccountDeletions.isEmpty() && stagedCategoryDeletions.isEmpty());

    // Show or hide the Save and Discard buttons based on changes
    ui->saveButton->setVisible(hasChanges);
    ui->discardButton->setVisible(hasChanges);
}
