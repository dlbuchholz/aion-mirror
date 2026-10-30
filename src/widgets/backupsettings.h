#ifndef BACKUPSETTINGS_H
#define BACKUPSETTINGS_H

#include <QWidget>
#include <functional>

class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

// The Backup section of the settings. Acts right away, so it doesn't take part in Apply / Revert.
class BackupSettings : public QWidget {
    Q_OBJECT

public:
    explicit BackupSettings(QWidget* parent = nullptr);

    // Asks where to save, starting with this path; empty when cancelled (a dialog; tests replace it)
    std::function<QString(const QString& suggested)> askBackupPath;
    // Asks which file to restore; empty when cancelled (a dialog; tests replace it)
    std::function<QString()> askRestorePath;
    // Asks whether to go ahead, showing this text (a dialog; tests replace it)
    std::function<bool(const QString& text)> confirmRestore;

    void reload();                                   // The name and the list of automatic backups

signals:
    void restored();                                 // The open database was replaced

protected:
    void showEvent(QShowEvent* event) override;

private:
    void backUpNow();
    void restoreFromFile();
    void restore(const QString& path, const QString& description);
    void showResult(const QString& text, bool failed);

    QLabel* introLabel = nullptr;
    QPushButton* backUpButton = nullptr;
    QPushButton* restoreFileButton = nullptr;
    QLabel* resultLabel = nullptr;
    QLabel* listHeading = nullptr;
    QToolButton* openFolderButton = nullptr;
    QVBoxLayout* listLayout = nullptr;
    QWidget* listContent = nullptr;
};

#endif // BACKUPSETTINGS_H
