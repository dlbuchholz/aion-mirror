#ifndef BACKUPSETTINGS_H
#define BACKUPSETTINGS_H

#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;

// The Backup section of the settings. Acts right away, so it doesn't take part in Apply / Revert.
class BackupSettings : public QWidget {
    Q_OBJECT

public:
    explicit BackupSettings(QWidget* parent = nullptr);

    // Asks where to save, starting with this path; empty when cancelled (a dialog; tests replace it)
    std::function<QString(const QString& suggested)> askBackupPath;

protected:
    void showEvent(QShowEvent* event) override;

private:
    void backUpNow();
    void showResult(const QString& text, bool failed);

    QLabel* introLabel = nullptr;
    QPushButton* backUpButton = nullptr;
    QLabel* resultLabel = nullptr;
};

#endif // BACKUPSETTINGS_H
