#ifndef TEST_FILE_DIALOG_H
#define TEST_FILE_DIALOG_H

#include <QApplication>
#include <QFileDialog>
#include <QTimer>

// What a file dialog showed. It's read before the dialog closes and checked after, so a failed
// check can't leave the dialog open and the test waiting for it.
struct FileDialogSeen {
    bool shown = false;
    QString title;
    QString folder;
    QStringList filters;
    QFileDialog::FileMode fileMode = QFileDialog::AnyFile;
    QFileDialog::AcceptMode acceptMode = QFileDialog::AcceptOpen;
    bool confirmsOverwrite = true;
    QString selected;                                // The file it suggested
};

// Answers the file dialog that the next call opens: chooses `choose`, or cancels without one
inline void answerFileDialog(FileDialogSeen* seen, const QString& choose = QString()) {
    // Qt's own dialog: the system's (on Windows, and under Wine) isn't a widget the test can
    // read or close, and would wait for a click that never comes
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QTimer::singleShot(0, qApp, [seen, choose]() {
        QWidget* modal = QApplication::activeModalWidget();
        auto* dialog = qobject_cast<QFileDialog*>(modal);
        if (!dialog) {
            // Some other dialog: closed, so the test goes on and fails on `shown`
            if (auto* other = qobject_cast<QDialog*>(modal)) other->reject();
            return;
        }
        seen->shown = true;
        seen->title = dialog->windowTitle();
        seen->folder = dialog->directory().absolutePath();
        seen->filters = dialog->nameFilters();
        seen->fileMode = dialog->fileMode();
        seen->acceptMode = dialog->acceptMode();
        seen->confirmsOverwrite = !dialog->testOption(QFileDialog::DontConfirmOverwrite);
        seen->selected = dialog->selectedFiles().value(0);
        if (choose.isEmpty()) {
            dialog->reject();
            return;
        }
        dialog->selectFile(choose);
        static_cast<QDialog*>(dialog)->accept();     // QFileDialog::accept is protected
    });
}

#endif // TEST_FILE_DIALOG_H
