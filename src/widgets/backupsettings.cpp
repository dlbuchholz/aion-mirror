#include "backupsettings.h"
#include "db/backup.h"
#include "db/databasemanager.h"
#include "db/databasesession.h"
#include "settings.h"
#include "theme.h"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString databasePath() {
    return DatabaseManager::instance().database().databaseName();
}

QString counted(int count, const QString& singular) {
    return QString("%1 %2%3").arg(QLocale(QLocale::English).toString(count), singular, count == 1 ? "" : "s");
}

}

BackupSettings::BackupSettings(QWidget* parent)
    : QWidget(parent) {
    askBackupPath = [this](const QString& suggested) {
        return QFileDialog::getSaveFileName(this, "Back up database", suggested, "Aion database (*.db)");
    };
    askRestorePath = [this]() {
        return QFileDialog::getOpenFileName(this, "Restore from file", QFileInfo(databasePath()).absolutePath(),
                                            "Aion database (*.db);;All files (*)");
    };
    confirmRestore = [this](const QString& text) {
        QMessageBox box(QMessageBox::Warning, "Restore this backup?", text, QMessageBox::NoButton, this);
        auto* restoreButton = box.addButton("Restore", QMessageBox::DestructiveRole);
        box.setDefaultButton(box.addButton(QMessageBox::Cancel));
        box.exec();
        return box.clickedButton() == restoreButton;
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
    restoreFileButton = new QPushButton("Restore from file…", this);
    restoreFileButton->setObjectName("restoreFromFileButton");
    connect(restoreFileButton, &QPushButton::clicked, this, &BackupSettings::restoreFromFile);
    buttons->addWidget(restoreFileButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    resultLabel = new QLabel(this);
    resultLabel->setObjectName("backupResultLabel");
    resultLabel->setTextFormat(Qt::PlainText);
    resultLabel->setWordWrap(true);
    resultLabel->setVisible(false);
    layout->addWidget(resultLabel);

    Settings& settings = Settings::instance();
    auto* scheduleForm = new QFormLayout();
    scheduleForm->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    scheduleCombo = new QComboBox(this);
    scheduleCombo->setObjectName("backupScheduleCombo");
    scheduleCombo->addItem("Every day", static_cast<int>(Settings::BackupSchedule::Daily));
    scheduleCombo->addItem("Every week", static_cast<int>(Settings::BackupSchedule::Weekly));
    scheduleCombo->addItem("Every month", static_cast<int>(Settings::BackupSchedule::Monthly));
    scheduleCombo->addItem("Off", static_cast<int>(Settings::BackupSchedule::Off));
    scheduleCombo->setCurrentIndex(scheduleCombo->findData(static_cast<int>(settings.getBackupSchedule())));
    scheduleForm->addRow("Back up", scheduleCombo);

    auto* keepRow = new QHBoxLayout();
    keepSpin = new QSpinBox(this);
    keepSpin->setObjectName("backupKeepSpin");
    keepSpin->setRange(1, 99);
    keepSpin->setValue(settings.getScheduledBackupsToKeep());
    keepRow->addWidget(keepSpin);
    keepRow->addWidget(new QLabel("scheduled backups", this));
    scheduleForm->addRow("Keep the last", keepRow);
    layout->addLayout(scheduleForm);

    keepSpin->setEnabled(settings.getBackupSchedule() != Settings::BackupSchedule::Off);
    connect(scheduleCombo, &QComboBox::currentIndexChanged, this, [this]() {
        const auto schedule = static_cast<Settings::BackupSchedule>(scheduleCombo->currentData().toInt());
        Settings::instance().setBackupSchedule(schedule);
        keepSpin->setEnabled(schedule != Settings::BackupSchedule::Off);
    });
    connect(keepSpin, &QSpinBox::valueChanged, this, [](int count) {
        Settings::instance().setScheduledBackupsToKeep(count);
    });

    auto* scheduleHint = new QLabel("Runs when a database is opened, if it has changed since the last one. "
                                    "Applies to every database on this computer.", this);
    scheduleHint->setWordWrap(true);
    scheduleHint->setForegroundRole(QPalette::PlaceholderText);
    layout->addWidget(scheduleHint);

    auto* listHeader = new QHBoxLayout();
    listHeading = new QLabel("Automatic backups", this);
    listHeading->setObjectName("automaticBackupsHeading");
    listHeading->setForegroundRole(QPalette::PlaceholderText);
    listHeader->addWidget(listHeading);
    listHeader->addStretch();
    openFolderButton = new QToolButton(this);
    openFolderButton->setText("Open folder");
    openFolderButton->setAutoRaise(true);
    openFolderButton->setCursor(Qt::PointingHandCursor);
    connect(openFolderButton, &QToolButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl::fromLocalFile(Backup::folderFor(databasePath())));
    });
    listHeader->addWidget(openFolderButton);
    layout->addLayout(listHeader);

    listLayout = new QVBoxLayout();
    listLayout->setSpacing(0);
    layout->addLayout(listLayout);
    layout->addStretch();
}

void BackupSettings::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    reload();
}

void BackupSettings::reload() {
    introLabel->setText(QString("A backup is a copy of %1: all users, accounts and transactions, and the number "
                                "formats. The theme and other settings on this computer aren't included.")
                            .arg(QFileInfo(databasePath()).fileName()));

    // Replaced, not reused: this can run while one of the old rows' buttons is still being clicked
    if (listContent) {
        listContent->hide();
        listContent->setParent(nullptr);
        listContent->deleteLater();
    }
    listContent = new QWidget(this);
    listLayout->addWidget(listContent);
    auto* rows = new QVBoxLayout(listContent);
    rows->setContentsMargins(0, 0, 0, 0);
    rows->setSpacing(4);

    const QLocale english(QLocale::English);
    const QList<Backup::AutomaticBackup> backups = Backup::automaticBackups(databasePath());
    listHeading->setVisible(!backups.isEmpty());
    openFolderButton->setVisible(!backups.isEmpty());
    for (const Backup::AutomaticBackup& backup : backups) {
        const QString when = english.toString(backup.when, "d MMM yyyy, HH:mm");
        auto* row = new QHBoxLayout();
        auto* whenLabel = new QLabel(when, listContent);
        whenLabel->setObjectName("backupWhenLabel");
        row->addWidget(whenLabel, 1);
        auto* kindLabel = new QLabel(backup.kind, listContent);
        kindLabel->setForegroundRole(QPalette::PlaceholderText);
        kindLabel->setMinimumWidth(110);
        row->addWidget(kindLabel);
        auto* sizeLabel = new QLabel(english.formattedDataSize(backup.sizeBytes, 0), listContent);
        sizeLabel->setForegroundRole(QPalette::PlaceholderText);
        sizeLabel->setMinimumWidth(60);
        row->addWidget(sizeLabel);
        auto* restoreButton = new QPushButton("Restore…", listContent);
        restoreButton->setObjectName("restoreBackupButton");
        const QString path = backup.path;
        connect(restoreButton, &QPushButton::clicked, this, [this, path, when]() {
            restore(path, QString("From %1").arg(when));
        });
        row->addWidget(restoreButton);
        rows->addLayout(row);
    }
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

void BackupSettings::restoreFromFile() {
    const QString path = askRestorePath();
    if (!path.isEmpty()) {
        restore(path, QFileInfo(path).fileName());
    }
}

void BackupSettings::restore(const QString& path, const QString& description) {
    QString error;
    const std::optional<Backup::Contents> contents = Backup::inspect(path, &error);
    if (!contents) {
        showResult(QString("This can't be restored. %1").arg(error), true);
        return;
    }

    const QString text = QString("%1: %2, %3, %4.\n\nEverything in the open database is replaced. Your current data "
                                 "is saved as a backup first, and Aion returns to the start window.")
                             .arg(description, counted(contents->users, "user"), counted(contents->accounts, "account"),
                                  counted(contents->transactions, "transaction"));
    if (!confirmRestore(text)) {
        return;
    }

    if (!DatabaseSession::restore(path, &error)) {
        showResult(error, true);
        reload();
        return;
    }
    showResult("Restored.", false);
    reload();
    emit restored();
}

void BackupSettings::showResult(const QString& text, bool failed) {
    const Theme::Colors colors = Theme::colors();
    QPalette palette = resultLabel->palette();
    palette.setColor(QPalette::WindowText, failed ? colors.danger : colors.textSecondary);
    resultLabel->setPalette(palette);
    resultLabel->setText(text);
    resultLabel->setVisible(true);
}
