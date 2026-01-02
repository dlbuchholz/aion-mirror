#include "CheckableComboBox.h"
#include <QStandardItem>
#include <QDebug>
#include <QStylePainter>

CheckableComboBox::CheckableComboBox(QWidget* parent)
    : QComboBox(parent), model(new QStandardItemModel(this)) {
    setModel(model);

    connect(model, &QStandardItemModel::itemChanged, this, &CheckableComboBox::onItemChanged);
}

void CheckableComboBox::onItemChanged(QStandardItem* item) {
    if (item->isCheckable()) {
        qDebug() << "CheckableComboBox: Item" << item->text()
        << "changed to" << (item->checkState() == Qt::Checked ? "Checked" :  "Unchecked");
        emit selectionChanged();
    }
}

void CheckableComboBox::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    QStylePainter painter(this);
    painter.setPen(palette().color(QPalette::Text));

    // Draw the combobox frame
    QStyleOptionComboBox opt;
    initStyleOption(&opt);

    // If we have custom display text, use it
    if (!displayText.isEmpty()) {
        opt.currentText = displayText;
    }

    painter.drawComplexControl(QStyle::CC_ComboBox, opt);
    painter.drawControl(QStyle::CE_ComboBoxLabel, opt);
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

int CheckableComboBox::itemCount() const {
    return model->rowCount();
}

void CheckableComboBox::setDisplayText(const QString& text) {
    displayText = text;
    updateWidth();
    update();  // Trigger repaint
}

void CheckableComboBox::updateWidth() {
    if (displayText.isEmpty()) {
        return;
    }

    // Calculate the width needed for the display text
    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(displayText);

    // Add padding for the dropdown arrow and margins
    int padding = 35;  // Space for arrow + margins
    int minWidth = textWidth + padding;

    // Also check the width of all items
    for (int i = 0; i < model->rowCount(); ++i) {
        QStandardItem* item = model->item(i);
        if (item) {
            int itemWidth = fm.horizontalAdvance(item->text()) + padding;
            minWidth = qMax(minWidth, itemWidth);
        }
    }

    setMinimumWidth(minWidth);
}
