#ifndef ACCOUNTTYPESSETTINGS_H
#define ACCOUNTTYPESSETTINGS_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QVBoxLayout;

// Settings section for account types: add, rename (by editing the name) and delete unused
// ones. Changes are saved right away.
class AccountTypesSettings : public QWidget {
    Q_OBJECT

public:
    explicit AccountTypesSettings(QWidget* parent = nullptr);

    void reload();

signals:
    void accountTypesChanged();

private:
    void addType();
    void rename(int typeId, QLineEdit* edit, const QString& oldName);
    void remove(int typeId);
    void showError(const QString& message);

    QVBoxLayout* listLayout = nullptr;
    QLineEdit* newTypeEdit = nullptr;
    QLabel* errorLabel = nullptr;
};

#endif // ACCOUNTTYPESSETTINGS_H
