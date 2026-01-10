#include "settingsdialog.h"
#include "../../ui/ui_settingsdialog.h"
#include "settings.h"
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QBrush>
#include <QColor>

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
    , m_settings(nullptr)  // Will be set in constructor body
{
    ui->setupUi(this);

    ui->categoryList->setStyleSheet(
        "QListWidget { background-color: #2d2d2d; border: none; }"
        "QListWidget:: item { padding: 8px; color:  #cccccc; }"
        "QListWidget::item: selected { background-color: #3d3d3d; }"
        );

    setupCategories();
    loadSettings();

    connect(ui->categoryList, &QListWidget::currentRowChanged,
            this, &SettingsDialog::onCategoryChanged);
    connect(ui->buttonBox, &QDialogButtonBox::accepted,
            this, &SettingsDialog:: onAccepted);
    connect(ui->buttonBox, &QDialogButtonBox:: rejected,
            this, &SettingsDialog::onRejected);
    connect(ui->buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked,
            this, &SettingsDialog::onApplyClicked);

    if (ui->categoryList->count() > 0) {
        ui->categoryList->setCurrentRow(0);
    }
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::setupCategories()
{
    addCategory("Currency Settings", "currency");
    addCategory("Transactions", "transactions");
    addCategory("Application", "app");
    addCategory("Import Settings", "import");
    // ...
}

void SettingsDialog::addCategory(const QString &name, const QString &group)
{
    ui->categoryList->addItem(name);
    m_categories[name] = group;

    QTableWidget *table = new QTableWidget(this);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Key", "Value"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    int index = ui->stackedWidget->addWidget(table);
    m_tables[index] = table;

    connect(table, &QTableWidget:: cellChanged,
            this, &SettingsDialog:: onCellChanged);
}

void SettingsDialog::loadSettings()
{
    QSettings& settings = Settings::instance().getQSettings();

    for (auto it = m_categories. begin(); it != m_categories. end(); ++it) {
        QString group = it.value();

        settings.beginGroup(group);
        QStringList keys = settings.allKeys();

        QMap<QString, QVariant> originalValues;
        for (const QString &key : keys) {
            originalValues[key] = settings. value(key);
        }
        m_originalValues[group] = originalValues;

        settings. endGroup();
    }

    if (! m_categories.isEmpty()) {
        populateTable(m_categories.first());
    }
}

void SettingsDialog::populateTable(const QString &group)
{
    int currentIndex = ui->stackedWidget->currentIndex();
    QTableWidget *table = m_tables[currentIndex];

    if (!table) return;

    table->blockSignals(true);
    table->setRowCount(0);

    if (m_originalValues. contains(group)) {
        QMap<QString, QVariant> values = m_originalValues[group];

        int row = 0;
        for (auto it = values.begin(); it != values.end(); ++it) {
            table->insertRow(row);

            QTableWidgetItem *keyItem = new QTableWidgetItem(it.key());
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            keyItem->setForeground(QBrush(QColor("#888888")));
            table->setItem(row, 0, keyItem);

            QTableWidgetItem *valueItem = new QTableWidgetItem(it.value().toString());
            table->setItem(row, 1, valueItem);

            row++;
        }
    }

    table->blockSignals(false);
}

void SettingsDialog::onCategoryChanged(int index)
{
    ui->stackedWidget->setCurrentIndex(index);

    QString categoryName = ui->categoryList->item(index)->text();
    QString group = m_categories[categoryName];

    populateTable(group);
}

void SettingsDialog::onCellChanged(int row, int column)
{
    if (column != 1) return;

    int currentIndex = ui->stackedWidget->currentIndex();
    QTableWidget *table = m_tables[currentIndex];
    QString categoryName = ui->categoryList->item(currentIndex)->text();
    QString group = m_categories[categoryName];

    QString key = table->item(row, 0)->text();
    QVariant newValue = table->item(row, 1)->text();

    if (m_originalValues.contains(group)) {
        m_originalValues[group][key] = newValue;
    }
}

void SettingsDialog:: saveSettings()
{
    QSettings& settings = Settings::instance().getQSettings();

    for (auto groupIt = m_originalValues.begin(); groupIt != m_originalValues.end(); ++groupIt) {
        QString group = groupIt.key();
        QMap<QString, QVariant> values = groupIt.value();

        settings.beginGroup(group);
        for (auto it = values.begin(); it != values.end(); ++it) {
            settings.setValue(it.key(), it.value());
        }
        settings.endGroup();
    }

    settings.sync();

    // Notify that settings changed
    emit settingsChanged();
}

void SettingsDialog:: onAccepted()
{
    saveSettings();
    accept();
}

void SettingsDialog::onRejected()
{
    reject();
}

void SettingsDialog::onApplyClicked()
{
    saveSettings();
}
