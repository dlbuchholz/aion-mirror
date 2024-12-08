#include "db/databasemanager.h"
#include "addaccountwidget.h"

#include <QSqlQuery>
#include <qsqlerror.h>

AddAccountWidget::AddAccountWidget(QWidget* parent)
    : QWidget(parent) {
    ui.setupUi(this);
    ui.accountCreationWidget->setVisible(false);
    connect(ui.addAccountButton, &QToolButton::clicked, this, &AddAccountWidget::toggleWidgets);
}

void AddAccountWidget::toggleWidgets() {
    widgetsVisible = !widgetsVisible;

    ui.accountCreationWidget->setVisible(widgetsVisible);
}

