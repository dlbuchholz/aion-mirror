#ifndef CHECKABLECOMBOBOX_H
#define CHECKABLECOMBOBOX_H

#include <QComboBox>
#include <QStandardItemModel>
#include <QSet>

class CheckableComboBox : public QComboBox {
    Q_OBJECT

public:
    explicit CheckableComboBox(QWidget* parent = nullptr);

    QSet<int> checkedItemIds() const;                    // Return the set of checked item IDs
    void addItemWithId(const QString& label, int id);    // Add a labeled item with an associated ID
    void clearItems();                                   // Clear all items from the combo box

    void setItemChecked(int index, bool checked);
    void checkAllItems();
    void uncheckAllItems();

signals:
    void selectionChanged();

private slots:
    void onItemChanged(QStandardItem* item);

private:
    QStandardItemModel* model; // Custom model for managing checkable items
};

#endif // CHECKABLECOMBOBOX_H
