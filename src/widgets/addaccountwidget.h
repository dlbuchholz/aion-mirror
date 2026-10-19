#ifndef ADDACCOUNTWIDGET_H
#define ADDACCOUNTWIDGET_H

#include <QWidget>

namespace Ui {
class AddAccountWidget;
}

class AddAccountWidget : public QWidget {
    Q_OBJECT

signals:
    void accountCreated();

public:
    explicit AddAccountWidget(QWidget* parent = nullptr);
    ~AddAccountWidget() override;
    void setUserId(int newUserId);
private slots:
    void onCreateAccountClicked();

private:
    Ui::AddAccountWidget* ui;
    int userId = -1;
};

#endif // ADDACCOUNTWIDGET_H
