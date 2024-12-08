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

#include "../ui/ui_addaccountwidget.h"

class AddAccountWidget : public QWidget {
    Q_OBJECT

public:
    explicit AddAccountWidget(QWidget* parent = nullptr);

private slots:
    void toggleWidgets();
private:
    QList<QWidget *> widgetsList;
    bool widgetsVisible = false;
    Ui::AddAccountWidget ui;
    int userId = -1;
};

#endif // ADDACCOUNTWIDGET_H
