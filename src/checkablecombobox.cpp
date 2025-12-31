#include "CheckableComboBox.h"
#include <QStandardItem>
#include <QDebug>

CheckableComboBox::CheckableComboBox(QWidget* parent)
    : QComboBox(parent), model(new QStandardItemModel(this)) {
    setModel(model);

    connect(model, &QStandardItemModel::itemChanged, this, &CheckableComboBox::onItemChanged);
}

void CheckableComboBox:: onItemChanged(QStandardItem* item) {
    if (item->isCheckable()) {
        qDebug() << "CheckableComboBox: Item" << item->text()
        << "changed to" << (item->checkState() == Qt::Checked ? "Checked" :  "Unchecked");
        emit selectionChanged();
    }
}

QSet<int> CheckableComboBox::checkedItemIds() const {
    QSet<int> checkedIds;
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item && item->checkState() == Qt::Checked) {
            checkedIds.insert(item->data(Qt::UserRole).toInt());
        }
    }
    qDebug() << "CheckableComboBox: checkedItemIds() returning" << checkedIds;
    return checkedIds;
}

void CheckableComboBox::addItemWithId(const QString& label, int id) {
    QStandardItem* item = new QStandardItem(label);
    item->setCheckable(true);
    item->setCheckState(Qt::Unchecked);
    item->setData(id, Qt::UserRole); // Store the associated ID
    model->appendRow(item);
    qDebug() << "CheckableComboBox: Added item" << label << "with ID" << id;
}

void CheckableComboBox::clearItems() {
    model->clear();
}

void CheckableComboBox:: setItemChecked(int index, bool checked) {
    if (index >= 0 && index < model->rowCount()) {
        QStandardItem* item = model->item(index);
        if (item) {
            item->setCheckState(checked ? Qt:: Checked : Qt::Unchecked);
        }
    }
}

void CheckableComboBox::checkAllItems() {
    qDebug() << "CheckableComboBox:  Checking all items";
    // Block signals temporarily to avoid multiple selectionChanged emissions
    model->blockSignals(true);

    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item) {
            item->setCheckState(Qt::Checked);
        }
    }

    model->blockSignals(false);
    emit selectionChanged();  // Emit once after all items are checked
}

void CheckableComboBox::uncheckAllItems() {
    qDebug() << "CheckableComboBox: Unchecking all items";
    model->blockSignals(true);

    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item) {
            item->setCheckState(Qt::Unchecked);
        }
    }

    model->blockSignals(false);
    emit selectionChanged();
}
