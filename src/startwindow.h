#ifndef STARTWINDOW_H
#define STARTWINDOW_H

#include <QDialog>

class QCheckBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QMenu;
class QToolButton;

// First window: pick the database file and who is using Aion, or add a new user
class StartWindow : public QDialog {
    Q_OBJECT

public:
    explicit StartWindow(QWidget* parent = nullptr);

    int selectedUserId() const;

protected:
    void changeEvent(QEvent* event) override;

private slots:
    void addUser();
    void openDatabaseFile();
    void createDatabase();

private:
    void loadUsers();
    void chooseUser(int userId);
    void setAddFormVisible(bool visible);
    void switchDatabase(const QString& path);
    void updateDatabaseButton();
    void rebuildDatabaseMenu();
    void showMessage(const QString& message);

    int userId = -1;

    QToolButton* databaseButton = nullptr;
    QMenu* databaseMenu = nullptr;
    QLabel* messageLabel = nullptr;
    QGridLayout* cardsLayout = nullptr;
    QFrame* addForm = nullptr;
    QLineEdit* nameEdit = nullptr;
    QLineEdit* usernameEdit = nullptr;
    QLabel* formError = nullptr;
    QCheckBox* rememberCheck = nullptr;
};

#endif // STARTWINDOW_H
