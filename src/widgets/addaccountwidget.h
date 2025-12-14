#ifndef ADDACCOUNTWIDGET_H
#define ADDACCOUNTWIDGET_H

#include <QWidget>
#include <QSqlDatabase>
#include <QLineEdit>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

#include "../../ui/ui_addaccountwidget.h"

class AddAccountWidget : public QWidget {
    Q_OBJECT

signals:
    void accountCreated();

public:
    explicit AddAccountWidget(QWidget* parent = nullptr);
    void setUserId(int newUserId);
private slots:
    void toggleWidgets();
    void onCreateAccountClicked();

private:
    QList<QWidget *> widgetsList;
    bool widgetsVisible = false;
    Ui::AddAccountWidget ui;
    int userId = -1;
};

#endif // ADDACCOUNTWIDGET_H
