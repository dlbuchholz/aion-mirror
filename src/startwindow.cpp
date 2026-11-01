#include "startwindow.h"
#include "db/databasesession.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"
#include "updatecheck.h"
#include "user.h"
#include "systeminfo.h"
#include "version.h"

#include <QAbstractButton>
#include <QCheckBox>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int CARD_COLUMNS = 3;

QString initialsFor(const QString& name) {
    QString initials;
    for (const QString& part : name.split(' ', Qt::SkipEmptyParts)) {
        initials += part.front().toUpper();
        if (initials.size() == 2) break;
    }
    return initials.isEmpty() ? QString("?") : initials;
}

// Clickable card with an initials avatar, the user's name and username (or "Add user")
class UserCard : public QAbstractButton {
public:
    UserCard(const QString& name, const QString& username, bool addCard, QWidget* parent)
        : QAbstractButton(parent), name(name), username(username), addCard(addCard) {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setFixedHeight(128);
        setMinimumWidth(140);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setToolTip(addCard ? QString("Add a new user") : QString("Open %1").arg(name));
        setAccessibleName(addCard ? QString("Add user") : name);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRectF card = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(card, 10, 10);
        painter.fillPath(path, underMouse() ? colors.itemHover : palette().color(QPalette::Base));
        QColor borderColor = colors.textMuted;
        borderColor.setAlpha(110);
        QPen border(underMouse() || hasFocus() ? colors.accent : borderColor);
        if (addCard) border.setStyle(Qt::DashLine);
        painter.setPen(border);
        painter.drawPath(path);

        // Avatar
        const QRectF avatar(width() / 2.0 - 24, 18, 48, 48);
        painter.setPen(Qt::NoPen);
        painter.setBrush(addCard ? colors.track : colors.accent);
        painter.drawEllipse(avatar);
        if (addCard) {
            painter.drawPixmap(QPointF(avatar.center().x() - 10, avatar.center().y() - 10),
                               Icons::tinted("plus", colors.textSecondary, 20, devicePixelRatioF()));
        } else {
            QFont avatarFont = font();
            avatarFont.setPointSizeF(font().pointSizeF() * 1.2);
            avatarFont.setWeight(QFont::DemiBold);
            painter.setFont(avatarFont);
            painter.setPen(colors.onAccent);
            painter.drawText(avatar, Qt::AlignCenter, initialsFor(name));
        }

        // Name and username
        const QRect textArea(8, 74, width() - 16, 22);
        QFont nameFont = font();
        nameFont.setWeight(QFont::DemiBold);
        painter.setFont(nameFont);
        painter.setPen(addCard ? colors.textSecondary : colors.textPrimary);
        painter.drawText(textArea, Qt::AlignCenter,
                         QFontMetrics(nameFont).elidedText(addCard ? QString("Add user") : name, Qt::ElideRight, textArea.width()));
        if (!addCard) {
            QFont smallFont = font();
            smallFont.setPointSizeF(font().pointSizeF() * 0.9);
            painter.setFont(smallFont);
            painter.setPen(colors.textMuted);
            painter.drawText(textArea.translated(0, 22), Qt::AlignCenter,
                             QFontMetrics(smallFont).elidedText("@" + username, Qt::ElideRight, textArea.width()));
        }
    }

private:
    QString name;
    QString username;
    bool addCard;
};

} // namespace

StartWindow::StartWindow(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle("Aion");
    resize(620, 480);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 18);
    root->setSpacing(14);

    // Header: logo, welcome text and the database switcher
    auto* header = new QHBoxLayout();
    header->setSpacing(14);
    auto* logo = new QLabel(this);
    logo->setObjectName("logoLabel");
    logo->setPixmap(Icons::logo(52, devicePixelRatioF()));
    logo->setAccessibleName("Aion logo");
    header->addWidget(logo, 0, Qt::AlignTop);
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    auto* title = new QLabel("Welcome to Aion", this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    title->setFont(titleFont);
    titles->addWidget(title);
    titles->addWidget(new QLabel("Choose who's using Aion", this));
    header->addLayout(titles, 1);

    databaseButton = new QToolButton(this);
    databaseButton->setObjectName("databaseButton");
    databaseButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    databaseButton->setPopupMode(QToolButton::InstantPopup);
    databaseMenu = new QMenu(databaseButton);
    databaseButton->setMenu(databaseMenu);
    connect(databaseMenu, &QMenu::aboutToShow, this, &StartWindow::rebuildDatabaseMenu);
    header->addWidget(databaseButton, 0, Qt::AlignTop);
    root->addLayout(header);

    messageLabel = new QLabel(this);
    messageLabel->setObjectName("startMessageLabel");
    messageLabel->setWordWrap(true);
    messageLabel->hide();
    root->addWidget(messageLabel);

    // User cards
    auto* cards = new QWidget(this);
    cardsLayout = new QGridLayout(cards);
    cardsLayout->setContentsMargins(0, 0, 0, 0);
    cardsLayout->setSpacing(12);
    root->addWidget(cards);

    // Inline form for a new user
    addForm = new QFrame(this);
    addForm->setFrameShape(QFrame::StyledPanel);
    auto* formLayout = new QVBoxLayout(addForm);
    formLayout->setContentsMargins(14, 12, 14, 12);
    auto* formTitle = new QLabel("Add user", addForm);
    QFont formTitleFont = formTitle->font();
    formTitleFont.setBold(true);
    formTitle->setFont(formTitleFont);
    formLayout->addWidget(formTitle);
    auto* fields = new QGridLayout();
    nameEdit = new QLineEdit(addForm);
    nameEdit->setObjectName("nameEdit");
    nameEdit->setPlaceholderText("Alex Example");
    usernameEdit = new QLineEdit(addForm);
    usernameEdit->setObjectName("usernameEdit");
    usernameEdit->setPlaceholderText("alex");
    fields->addWidget(new QLabel("Name", addForm), 0, 0);
    fields->addWidget(new QLabel("Username", addForm), 0, 1);
    fields->addWidget(nameEdit, 1, 0);
    fields->addWidget(usernameEdit, 1, 1);
    formLayout->addLayout(fields);
    formError = new QLabel(addForm);
    QPalette errorPalette = formError->palette();
    errorPalette.setColor(QPalette::WindowText, Theme::colors().danger);
    formError->setPalette(errorPalette);
    formError->hide();
    formLayout->addWidget(formError);
    auto* formButtons = new QHBoxLayout();
    formButtons->addStretch();
    auto* cancelButton = new QPushButton("Cancel", addForm);
    cancelButton->setAutoDefault(false);
    connect(cancelButton, &QPushButton::clicked, this, [this]() { setAddFormVisible(false); });
    auto* saveButton = new QPushButton("Add and open", addForm);
    saveButton->setObjectName("addUserButton");
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &StartWindow::addUser);
    formButtons->addWidget(cancelButton);
    formButtons->addWidget(saveButton);
    formLayout->addLayout(formButtons);
    for (QLineEdit* edit : {nameEdit, usernameEdit}) {
        connect(edit, &QLineEdit::textEdited, formError, &QLabel::hide);
        connect(edit, &QLineEdit::returnPressed, this, &StartWindow::addUser);
    }
    addForm->hide();
    root->addWidget(addForm);
    root->addStretch();

    // Footer: open automatically; version, update check and quit
    rememberCheck = new QCheckBox("Open this user automatically next time", this);
    rememberCheck->setToolTip("Use \"Switch user\" in the sidebar to come back here");
    root->addWidget(rememberCheck);

    auto* footer = new QHBoxLayout();
    auto* versionLabel = new QLabel(QString("Version %1").arg(SystemInfo::versionText()), this);
    versionLabel->setObjectName("versionLabel");
    QPalette versionPalette = versionLabel->palette();
    versionPalette.setColor(QPalette::WindowText, Theme::colors().textMuted);
    versionLabel->setPalette(versionPalette);
    footer->addWidget(versionLabel);
    auto* updateButton = new QToolButton(this);
    updateButton->setText("Check for updates");
    updateButton->setAutoRaise(true);
    updateButton->setCursor(Qt::PointingHandCursor);
    connect(updateButton, &QToolButton::clicked, this, [this]() { UpdateCheck::checkInteractively(this); });
    footer->addWidget(updateButton);
    footer->addStretch();
    auto* quitButton = new QPushButton("Quit", this);
    quitButton->setAutoDefault(false);
    connect(quitButton, &QPushButton::clicked, this, &QDialog::reject);
    footer->addWidget(quitButton);
    root->addLayout(footer);

    askDatabaseToOpen = [this]() {
        return QFileDialog::getOpenFileName(this, "Open database",
                                            QFileInfo(DatabaseSession::currentDatabasePath()).absolutePath(),
                                            "Aion database (*.db);;All files (*)");
    };
    askDatabaseToCreate = [this]() {
        return QFileDialog::getSaveFileName(this, "New database",
                                            QFileInfo(DatabaseSession::currentDatabasePath()).absolutePath() + "/budget.db",
                                            "Aion database (*.db)", nullptr, QFileDialog::DontConfirmOverwrite);
    };

    updateDatabaseButton();
    loadUsers();
}

int StartWindow::selectedUserId() const {
    return userId;
}

void StartWindow::loadUsers() {
    rememberCheck->setChecked(Settings::instance().getDefaultUserId(DatabaseSession::currentDatabasePath()) > 0);
    while (QLayoutItem* item = cardsLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    UserDAO userDao;
    const QList<User> users = userDao.getAllUsers();
    int index = 0;
    for (const User& user : users) {
        auto* card = new UserCard(user.getName(), user.getUsername(), false, this);
        const int id = user.getUserId();
        connect(card, &QAbstractButton::clicked, this, [this, id]() { chooseUser(id); });
        cardsLayout->addWidget(card, index / CARD_COLUMNS, index % CARD_COLUMNS);
        ++index;
    }
    auto* addCard = new UserCard(QString(), QString(), true, this);
    addCard->setObjectName("addUserCard");
    connect(addCard, &QAbstractButton::clicked, this, [this]() { setAddFormVisible(true); });
    cardsLayout->addWidget(addCard, index / CARD_COLUMNS, index % CARD_COLUMNS);

    // Keep card widths equal even when a row isn't full
    for (int column = 0; column < CARD_COLUMNS; ++column) {
        cardsLayout->setColumnStretch(column, 1);
    }

    // A new database has nobody in it yet: go straight to adding someone
    setAddFormVisible(users.isEmpty());
}

void StartWindow::chooseUser(int chosenUserId) {
    userId = chosenUserId;
    Settings::instance().setDefaultUserId(DatabaseSession::currentDatabasePath(), rememberCheck->isChecked() ? chosenUserId : -1);
    accept();
}

void StartWindow::setAddFormVisible(bool visible) {
    addForm->setVisible(visible);
    formError->hide();
    if (visible) {
        nameEdit->setFocus();
    } else {
        nameEdit->clear();
        usernameEdit->clear();
    }
}

void StartWindow::addUser() {
    const QString name = nameEdit->text().trimmed();
    const QString username = usernameEdit->text().trimmed();
    if (name.isEmpty() || username.isEmpty()) {
        formError->setText("Enter a name and a username.");
        formError->show();
        (name.isEmpty() ? nameEdit : usernameEdit)->setFocus();
        return;
    }

    UserDAO userDao;
    if (!userDao.createUser(User(0, username, name))) {
        formError->setText("A user with that name or username already exists. Try another.");
        formError->show();
        return;
    }
    chooseUser(userDao.findUserIdByUsername(username));
}

void StartWindow::updateDatabaseButton() {
    const QString path = DatabaseSession::currentDatabasePath();
    databaseButton->setText(QFileInfo(path).fileName());
    databaseButton->setToolTip(QString("Database: %1\nClick to switch").arg(QDir::toNativeSeparators(path)));
    databaseButton->setIcon(Icons::icon("database", palette().color(QPalette::ButtonText)));
}

void StartWindow::rebuildDatabaseMenu() {
    databaseMenu->clear();
    databaseMenu->addSection("Recent databases");
    const QString current = DatabaseSession::currentDatabasePath();
    for (const QString& path : Settings::instance().getRecentDatabases()) {
        if (!QFileInfo::exists(path)) {
            Settings::instance().removeRecentDatabase(path);
            continue;
        }
        QAction* action = databaseMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(QDir::toNativeSeparators(path));
        action->setCheckable(true);
        action->setChecked(path == current);
        connect(action, &QAction::triggered, this, [this, path]() { switchDatabase(path); });
    }
    databaseMenu->setToolTipsVisible(true);
    databaseMenu->addSeparator();
    databaseMenu->addAction("Open database file…", this, &StartWindow::openDatabaseFile);
    databaseMenu->addAction("New database…", this, &StartWindow::createDatabase);
}

void StartWindow::switchDatabase(const QString& path) {
    if (path == DatabaseSession::currentDatabasePath()) {
        return;
    }
    QString error;
    if (!DatabaseSession::open(path, &error)) {
        showMessage(QString("Couldn't open %1. %2").arg(QFileInfo(path).fileName(), error));
        return;
    }
    messageLabel->hide();
    updateDatabaseButton();
    loadUsers();
}

void StartWindow::openDatabaseFile() {
    const QString path = askDatabaseToOpen();
    if (!path.isEmpty()) {
        switchDatabase(path);
    }
}

void StartWindow::createDatabase() {
    QString path = askDatabaseToCreate();
    if (path.isEmpty()) {
        return;
    }
    if (QFileInfo(path).suffix().isEmpty()) {
        path += ".db";
    }
    QString error;
    if (!DatabaseSession::create(path, &error)) {
        showMessage(QString("Couldn't create %1. %2").arg(QFileInfo(path).fileName(), error));
        return;
    }
    messageLabel->hide();
    updateDatabaseButton();
    loadUsers();
}

void StartWindow::showMessage(const QString& message) {
    QPalette errorPalette = messageLabel->palette();
    errorPalette.setColor(QPalette::WindowText, Theme::colors().danger);
    messageLabel->setPalette(errorPalette);
    messageLabel->setText(message);
    messageLabel->show();
}

void StartWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && databaseButton) {
        updateDatabaseButton();
        if (auto* versionLabel = findChild<QLabel*>("versionLabel")) {
            QPalette versionPalette = versionLabel->palette();
            versionPalette.setColor(QPalette::WindowText, Theme::colors().textMuted);
            versionLabel->setPalette(versionPalette);
        }
    }
    QDialog::changeEvent(event);
}
