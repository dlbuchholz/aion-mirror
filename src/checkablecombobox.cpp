#include "CheckableComboBox.h"
#include <QStandardItem>

CheckableComboBox::CheckableComboBox(QWidget* parent)
    : QComboBox(parent), model(new QStandardItemModel(this)) {
    setModel(model);

    connect(model, &QStandardItemModel::itemChanged, this, &CheckableComboBox::onItemChanged);
}

void CheckableComboBox::onItemChanged(QStandardItem* item) {
    if (item->isCheckable()) {
        emit selectionChanged();
    }
}

QSet<int> CheckableComboBox::checkedItemIds() const {
    QSet<int> checkedIds;
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item->checkState() == Qt::Checked) {
            checkedIds.insert(item->data(Qt::UserRole).toInt());
        }
    }
    return checkedIds;
}

void CheckableComboBox::addItemWithId(const QString& label, int id) {
    QStandardItem* item = new QStandardItem(label);
    item->setCheckable(true);
    item->setCheckState(Qt::Unchecked);
    item->setData(id, Qt::UserRole); // Store the associated ID
    model->appendRow(item);
}

void CheckableComboBox::clearItems() {
    model->clear();
}
