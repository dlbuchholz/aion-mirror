#include "backupsettings.h"
#include "db/backup.h"
#include "db/databasemanager.h"
#include "theme.h"

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QString databasePath() {
    return DatabaseManager::instance().database().databaseName();
}

}

BackupSettings::BackupSettings(QWidget* parent)
    : QWidget(parent) {
    askBackupPath = [this](const QString& suggested) {
        return QFileDialog::getSaveFileName(this, "Back up database", suggested, "Aion database (*.db)");
    };

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto* heading = new QLabel("Backup", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    introLabel = new QLabel(this);
    introLabel->setObjectName("backupIntroLabel");
    introLabel->setTextFormat(Qt::PlainText);
    introLabel->setWordWrap(true);
    introLabel->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(introLabel);

    auto* buttons = new QHBoxLayout();
    backUpButton = new QPushButton("Back up now…", this);
    backUpButton->setObjectName("backUpNowButton");
    connect(backUpButton, &QPushButton::clicked, this, &BackupSettings::backUpNow);
    buttons->addWidget(backUpButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    resultLabel = new QLabel(this);
    resultLabel->setObjectName("backupResultLabel");
    resultLabel->setTextFormat(Qt::PlainText);
    resultLabel->setWordWrap(true);
    resultLabel->setVisible(false);
    layout->addWidget(resultLabel);
    layout->addStretch();
}

void BackupSettings::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    introLabel->setText(QString("A backup is a copy of %1: all users, accounts and transactions, and the number "
                                "formats. The theme and other settings on this computer aren't included.")
                            .arg(QFileInfo(databasePath()).fileName()));
}

void BackupSettings::backUpNow() {
    const QFileInfo database(databasePath());
    QString path = askBackupPath(database.absoluteDir().filePath(Backup::suggestedName(database.filePath(), QDate::currentDate())));
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".db";
    }

    QString error;
    if (Backup::write(path, &error)) {
        showResult(QString("Saved as %1.").arg(QDir::toNativeSeparators(path)), false);
    } else {
        showResult(error, true);
    }
}

void BackupSettings::showResult(const QString& text, bool failed) {
    const Theme::Colors colors = Theme::colors();
    QPalette palette = resultLabel->palette();
    palette.setColor(QPalette::WindowText, failed ? colors.danger : colors.textSecondary);
    resultLabel->setPalette(palette);
    resultLabel->setText(text);
    resultLabel->setVisible(true);
}
