#include "rawsettingsdialog.h"
#include "settings.h"

#include <QDialogButtonBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>

RawSettingsDialog::RawSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Advanced settings");
    resize(720, 520);

    auto *layout = new QVBoxLayout(this);

    auto *warning = new QLabel("These are the raw stored values. Editing them directly can break Aion, "
                               "so only change a value if you know what it does.", this);
    warning->setWordWrap(true);
    layout->addWidget(warning);

    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search setting name or value");
    searchEdit->setClearButtonEnabled(true);
    connect(searchEdit, &QLineEdit::textChanged, this, &RawSettingsDialog::applyFilter);
    layout->addWidget(searchEdit);

    table = new QTableWidget(this);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({"Setting", "Value"});
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setAlternatingRowColors(true);
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    connect(table, &QTableWidget::itemChanged, this, &RawSettingsDialog::onItemChanged);
    layout->addWidget(table, 1);

    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Apply | QDialogButtonBox::Cancel, this);
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveSettings();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, &RawSettingsDialog::saveSettings);
    layout->addWidget(buttonBox);

    loadSettings();
}

void RawSettingsDialog::loadSettings()
{
    const QSignalBlocker blocker(table);
    QSettings &settings = Settings::instance().getQSettings();

    QStringList keys = settings.allKeys();
    keys.sort(Qt::CaseInsensitive);

    table->setRowCount(0);
    for (const QString &key : keys) {
        const int row = table->rowCount();
        table->insertRow(row);

        auto *keyItem = new QTableWidgetItem(key);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        table->setItem(row, 0, keyItem);
        table->setItem(row, 1, new QTableWidgetItem(settings.value(key).toString()));
    }

    editedValues.clear();
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(false);
    applyFilter(searchEdit->text());
}

void RawSettingsDialog::applyFilter(const QString &text)
{
    for (int row = 0; row < table->rowCount(); ++row) {
        const bool matches = text.isEmpty()
            || table->item(row, 0)->text().contains(text, Qt::CaseInsensitive)
            || table->item(row, 1)->text().contains(text, Qt::CaseInsensitive);
        table->setRowHidden(row, !matches);
    }
}

void RawSettingsDialog::onItemChanged(QTableWidgetItem *item)
{
    if (item->column() != 1) {
        return;
    }

    const QString key = table->item(item->row(), 0)->text();
    editedValues[key] = item->text();

    // Highlight edited rows until they are saved
    const QSignalBlocker blocker(table);
    for (int column = 0; column < table->columnCount(); ++column) {
        QFont font = table->item(item->row(), column)->font();
        font.setBold(true);
        table->item(item->row(), column)->setFont(font);
    }
    buttonBox->button(QDialogButtonBox::Apply)->setEnabled(true);
}

void RawSettingsDialog::saveSettings()
{
    if (editedValues.isEmpty()) {
        return;
    }

    QSettings &settings = Settings::instance().getQSettings();
    for (auto it = editedValues.cbegin(); it != editedValues.cend(); ++it) {
        settings.setValue(it.key(), it.value());
    }
    settings.sync();

    loadSettings();
    emit settingsChanged();
}
