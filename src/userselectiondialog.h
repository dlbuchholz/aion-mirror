#ifndef USERSELECTIONDIALOG_H
#define USERSELECTIONDIALOG_H

#include <QDialog>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include "../ui/ui_userselectiondialog.h"
#include "user.h"

class UserSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit UserSelectionDialog(QWidget* parent = nullptr);
    int getSelectedUserId() const;

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void loadUsers();
    void createUser();
    void confirmSelection();
    void toggleUserCreationWidgets();

private:
    Ui::UserSelectionDialog ui;
    int selectedUserId;
    bool mousePressed;
    bool widgetsVisible = false;
    QPoint lastMousePosition;
};

#endif // USERSELECTIONDIALOG_H
