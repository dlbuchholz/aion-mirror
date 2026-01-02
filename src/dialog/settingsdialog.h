#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QSettings>
#include <QMap>
#include <QTableWidget>

namespace Ui {
class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

signals:
    void settingsChanged();

private slots:
    void onCategoryChanged(int index);
    void onAccepted();
    void onRejected();
    void onApplyClicked();
    void onCellChanged(int row, int column);

private:
    void setupCategories();
    void loadSettings();
    void saveSettings();
    void addCategory(const QString &name, const QString &group);
    void populateTable(const QString &group);

    Ui::SettingsDialog *ui;
    QSettings* m_settings;

    QMap<QString, QString> m_categories;
    QMap<QString, QMap<QString, QVariant>> m_originalValues;
    QMap<int, QTableWidget*> m_tables;
};

#endif // SETTINGSDIALOG_H
