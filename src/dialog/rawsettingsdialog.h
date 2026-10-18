#ifndef RAWSETTINGSDIALOG_H
#define RAWSETTINGSDIALOG_H

#include <QDialog>
#include <QMap>

class QDialogButtonBox;
class QLineEdit;
class QTableWidget;
class QTableWidgetItem;

// Developer view of every stored setting as raw key/value text (like about:config)
class RawSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RawSettingsDialog(QWidget *parent = nullptr);

signals:
    void settingsChanged();

private:
    void loadSettings();
    void applyFilter(const QString &text);
    void onItemChanged(QTableWidgetItem *item);
    void saveSettings();

    QLineEdit *searchEdit = nullptr;
    QTableWidget *table = nullptr;
    QDialogButtonBox *buttonBox = nullptr;
    QMap<QString, QString> editedValues;             // Key -> new value, not yet saved
};

#endif // RAWSETTINGSDIALOG_H
