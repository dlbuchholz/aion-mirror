#include "sidebarwidget.h"
#include "account.h"
#include "budget.h"
#include "settings.h"
#include "theme.h"
#include "user.h"
#include "icons.h"

#include <QAbstractButton>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

constexpr int EXPANDED_WIDTH = 240;
constexpr int COLLAPSED_WIDTH = 52;
constexpr int ICON_X = 9;                            // Left edge of the icon column
constexpr int TEXT_X = 38;                           // Left edge of the label column
constexpr int PADDING_RIGHT = 9;

QString initialsFor(const QString& name) {
    const QStringList parts = name.split(' ', Qt::SkipEmptyParts);
    QString initials;
    for (const QString& part : parts) {
        initials += part.front().toUpper();
        if (initials.size() == 2) {
            break;
        }
    }
    return initials.isEmpty() ? QString("?") : initials;
}

} // namespace

// A custom-painted sidebar entry: navigation link, section header or data row
class SidebarItem : public QAbstractButton {
public:
    enum class Kind {
        Nav,
        SectionHeader,
        Row
    };

    SidebarItem(Kind kind, QWidget* parent = nullptr)
        : QAbstractButton(parent), kind(kind) {
        setAttribute(Qt::WA_Hover);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setIconName(const QString& name) { iconName = name; update(); }
    void setTrailingText(const QString& text, bool negative = false) {
        trailingText = text;
        trailingNegative = negative;
        setAccessibleDescription(text);              // Painted only, so screen readers need it spelled out
        update();
    }
    void setTrailingIcon(const QString& name) { trailingIcon = name; update(); }
    void setProgress(double fraction) { progress = fraction; updateGeometry(); update(); }
    void setAvatarInitials(const QString& initials) { avatarInitials = initials; update(); }
    void setMuted(bool isMuted) {
        muted = isMuted;
        setCursor(muted ? Qt::ArrowCursor : Qt::PointingHandCursor);
        update();
    }

    QSize sizeHint() const override {
        switch (kind) {
        case Kind::Nav:           return QSize(0, 34);
        case Kind::SectionHeader: return QSize(0, 28);
        case Kind::Row:           return QSize(0, progress >= 0 ? 40 : 28);
        }
        return QSize(0, 28);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        const qreal dpr = devicePixelRatioF();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        // Background: selected or hovered
        const bool hovered = underMouse() && !muted;
        if (isChecked() || hovered) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rect()), 6, 6);
            painter.fillPath(path, isChecked() ? colors.itemSelected : colors.itemHover);
        }

        // Rows with a progress bar keep their text in the upper part
        const int lineHeight = (progress >= 0) ? 26 : height();

        QColor textColor = colors.textSecondary;
        if (isChecked()) textColor = colors.textPrimary;
        if (kind == Kind::SectionHeader || muted) textColor = colors.textMuted;

        // Leading icon or avatar
        if (!avatarInitials.isEmpty()) {
            const QRectF circle(ICON_X - 2, (lineHeight - 22) / 2.0, 22, 22);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accent);
            painter.drawEllipse(circle);
            QFont avatarFont = font();
            avatarFont.setPointSizeF(font().pointSizeF() * 0.75);
            avatarFont.setWeight(QFont::DemiBold);
            painter.setFont(avatarFont);
            painter.setPen(colors.onAccent);
            painter.drawText(circle, Qt::AlignCenter, avatarInitials);
        } else if (!iconName.isEmpty()) {
            const int iconSize = (kind == Kind::Nav) ? 18 : 14;
            const int x = ICON_X + (18 - iconSize) / 2;
            painter.drawPixmap(x, (lineHeight - iconSize) / 2, Icons::tinted(iconName, textColor, iconSize, dpr));
        }

        int right = width() - PADDING_RIGHT;

        // Trailing chevron (section headers)
        if (!trailingIcon.isEmpty()) {
            const int iconSize = 14;
            right -= iconSize;
            painter.drawPixmap(right, (lineHeight - iconSize) / 2, Icons::tinted(trailingIcon, colors.textMuted, iconSize, dpr));
            right -= 6;
        }

        QFont labelFont = font();
        if (kind == Kind::SectionHeader) {
            labelFont.setPointSizeF(font().pointSizeF() * 0.9);
            labelFont.setWeight(QFont::DemiBold);
        }
        painter.setFont(labelFont);
        const QFontMetrics metrics(labelFont);

        // Trailing value (balance, budget usage, folded-section summary)
        if (!trailingText.isEmpty()) {
            const int textWidth = metrics.horizontalAdvance(trailingText);
            painter.setPen(trailingNegative ? colors.danger : (kind == Kind::SectionHeader ? colors.textMuted : colors.textSecondary));
            painter.drawText(QRect(right - textWidth, 0, textWidth, lineHeight), Qt::AlignVCenter | Qt::AlignRight, trailingText);
            right -= textWidth + 8;
        }

        // Label (section headers start in the icon column)
        const int textX = (kind == Kind::SectionHeader) ? ICON_X : TEXT_X;
        const int available = right - textX;
        if (available > 12) {
            painter.setPen(textColor);
            painter.drawText(QRect(textX, 0, available, lineHeight), Qt::AlignVCenter | Qt::AlignLeft,
                             metrics.elidedText(text(), Qt::ElideRight, available));
        }

        // Progress bar under the text
        if (progress >= 0) {
            const QRectF track(TEXT_X, height() - 10, width() - TEXT_X - PADDING_RIGHT, 4);
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.track);
            painter.drawRoundedRect(track, 2, 2);
            QRectF fill = track;
            fill.setWidth(track.width() * qBound(0.0, progress, 1.0));
            painter.setBrush(progress > 1.0 ? colors.danger : colors.accent);
            painter.drawRoundedRect(fill, 2, 2);
        }
    }

private:
    Kind kind;
    QString iconName;
    QString trailingText;
    QString trailingIcon;
    QString avatarInitials;
    double progress = -1.0;
    bool trailingNegative = false;
    bool muted = false;
};

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 12, 8, 12);
    root->setSpacing(2);

    // Navigation
    navGroup.setExclusive(true);
    addNavItem(root, "dashboard", "Dashboard", Page::Dashboard);
    addNavItem(root, "transactions", "Transactions", Page::Transactions);
    addNavItem(root, "budget", "Budget", Page::Budget);
    addNavItem(root, "reports", "Reports", Page::Reports);
    connect(&navGroup, &QButtonGroup::idClicked, this, [this](int id) {
        emit pageRequested(static_cast<Page>(id));
    });

    // Accounts and budget sections, scrollable when they outgrow the window
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->viewport()->setAutoFillBackground(false);

    auto* sectionsWidget = new QWidget();
    auto* sectionsLayout = new QVBoxLayout(sectionsWidget);
    sectionsLayout->setContentsMargins(0, 12, 0, 0);
    sectionsLayout->setSpacing(2);
    accountsSection = createSection(sectionsLayout, "accounts", "Accounts");
    sectionsLayout->addSpacing(10);
    budgetSection = createSection(sectionsLayout, "budget", "Budget");
    sectionsLayout->addStretch();

    scrollArea->setWidget(sectionsWidget);
    sectionsWidget->setAutoFillBackground(false);    // setWidget() turns it on
    root->addWidget(scrollArea, 1);
    root->addStretch(0);                             // Keeps the rail items packed while the sections are hidden
    sectionsArea = scrollArea;

    connect(accountsSection.header, &QAbstractButton::clicked, this, [this]() {
        setSectionExpanded(accountsSection, !accountsSection.expanded);
    });
    connect(budgetSection.header, &QAbstractButton::clicked, this, [this]() {
        setSectionExpanded(budgetSection, !budgetSection.expanded);
    });

    // Bottom: settings, user, collapse
    addNavItem(root, "settings", "Settings", Page::Settings);

    userItem = new SidebarItem(SidebarItem::Kind::Nav, this);
    userItem->setObjectName("userItem");
    root->addWidget(userItem);
    labelledItems.append(userItem);
    connect(userItem, &QAbstractButton::clicked, this, &SidebarWidget::showUserMenu);

    collapseItem = new SidebarItem(SidebarItem::Kind::Nav, this);
    collapseItem->setObjectName("collapseItem");
    root->addWidget(collapseItem);
    labelledItems.append(collapseItem);
    connect(collapseItem, &QAbstractButton::clicked, this, [this]() {
        setCollapsed(!collapsed);
    });

    // Width animation for collapsing to the icon rail
    widthAnimation.setDuration(160);
    widthAnimation.setEasingCurve(QEasingCurve::OutCubic);
    connect(&widthAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        setFixedWidth(value.toInt());
    });
    connect(&widthAnimation, &QVariantAnimation::finished, this, [this]() {
        if (!collapsed) {
            sectionsArea->setVisible(true);
        }
    });

    setCollapsed(Settings::instance().isSidebarCollapsed(), false);
}

SidebarItem* SidebarWidget::addNavItem(QVBoxLayout* layout, const QString& icon, const QString& text, Page page) {
    auto* item = new SidebarItem(SidebarItem::Kind::Nav, this);
    item->setIconName(icon);
    item->setText(text);
    item->setToolTip(text);
    item->setCheckable(true);
    navGroup.addButton(item, static_cast<int>(page));
    layout->addWidget(item);
    labelledItems.append(item);
    return item;
}

SidebarWidget::Section SidebarWidget::createSection(QVBoxLayout* layout, const QString& key, const QString& title) {
    Section section;
    section.key = key;
    section.header = new SidebarItem(SidebarItem::Kind::SectionHeader);
    section.header->setObjectName(key + "Header");
    section.header->setText(title);
    section.body = new QWidget();
    section.bodyLayout = new QVBoxLayout(section.body);
    section.bodyLayout->setContentsMargins(0, 0, 0, 0);
    section.bodyLayout->setSpacing(1);
    layout->addWidget(section.header);
    layout->addWidget(section.body);
    setSectionExpanded(section, Settings::instance().isSidebarSectionExpanded(key));
    return section;
}

void SidebarWidget::setSectionExpanded(Section& section, bool expanded) {
    section.expanded = expanded;
    section.body->setVisible(expanded);
    section.header->setTrailingIcon(expanded ? "chevron-down" : "chevron-right");
    section.header->setTrailingText(expanded ? QString() : section.summary);
    Settings::instance().setSidebarSectionExpanded(section.key, expanded);
}

void SidebarWidget::clearSection(Section& section) {
    while (QLayoutItem* item = section.bodyLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
}

void SidebarWidget::setUserId(int newUserId) {
    userId = newUserId;
    refresh();
}

void SidebarWidget::refresh() {
    if (userId == -1) {
        return;
    }
    loadUser();
    loadAccounts();
    loadBudgets();
}

void SidebarWidget::setCurrentPage(Page page) {
    if (QAbstractButton* button = navGroup.button(static_cast<int>(page))) {
        button->setChecked(true);
    }
}

bool SidebarWidget::isCollapsed() const {
    return collapsed;
}

void SidebarWidget::setCollapsed(bool shouldCollapse, bool animate) {
    collapsed = shouldCollapse;
    Settings::instance().setSidebarCollapsed(collapsed);

    collapseItem->setIconName(collapsed ? "chevrons-right" : "chevrons-left");
    collapseItem->setText(collapsed ? "Expand" : "Collapse");
    collapseItem->setToolTip(collapseItem->text());

    // Sections disappear before shrinking and reappear once the sidebar is wide again
    if (collapsed) {
        sectionsArea->setVisible(false);
    }

    const int target = collapsed ? COLLAPSED_WIDTH : EXPANDED_WIDTH;
    widthAnimation.stop();
    if (animate && isVisible()) {
        widthAnimation.setStartValue(width());
        widthAnimation.setEndValue(target);
        widthAnimation.start();
    } else {
        setFixedWidth(target);
        sectionsArea->setVisible(!collapsed);
    }
}

void SidebarWidget::loadUser() {
    UserDAO userDao;
    const QString name = userDao.getUserById(userId).getName();
    userItem->setText(name.isEmpty() ? QString("Unknown user") : name);
    userItem->setToolTip(userItem->text());
    userItem->setAvatarInitials(initialsFor(name));
}

void SidebarWidget::loadAccounts() {
    clearSection(accountsSection);

    AccountDAO accountDao;
    const QList<Account> accounts = accountDao.getAllAccountsForUser(userId);
    const Settings& settings = Settings::instance();

    double total = 0.0;
    for (const Account& account : accounts) {
        total += account.getBalance();
    }

    auto* allItem = new SidebarItem(SidebarItem::Kind::Row);
    allItem->setText("All accounts");
    allItem->setTrailingText(settings.formatCurrency(total), total < 0);
    accountsSection.bodyLayout->addWidget(allItem);
    connect(allItem, &QAbstractButton::clicked, this, [this]() {
        emit accountRequested(ALL_ACCOUNTS_ID);
    });

    for (const Account& account : accounts) {
        auto* item = new SidebarItem(SidebarItem::Kind::Row);
        item->setText(account.getName());
        item->setToolTip(QString("%1 (%2)").arg(account.getName(), account.getType()));
        const double balance = account.getBalance();
        // formatNumber() formats the absolute value, so add the sign here
        item->setTrailingText((balance < 0 ? "-" : "") + settings.formatNumber(balance), balance < 0);
        accountsSection.bodyLayout->addWidget(item);
        const int accountId = account.getAccountId();
        connect(item, &QAbstractButton::clicked, this, [this, accountId]() {
            emit accountRequested(accountId);
        });
    }

    auto* addItem = new SidebarItem(SidebarItem::Kind::Row);
    addItem->setIconName("plus");
    addItem->setText("Add account");
    accountsSection.bodyLayout->addWidget(addItem);
    connect(addItem, &QAbstractButton::clicked, this, &SidebarWidget::addAccountRequested);

    accountsSection.summary = settings.formatCurrency(total);
    setSectionExpanded(accountsSection, accountsSection.expanded);
}

void SidebarWidget::loadBudgets() {
    clearSection(budgetSection);

    const QDate today = QDate::currentDate();
    budgetSection.header->setText(QString("Budget · %1").arg(today.toString("MMMM")));

    BudgetDAO budgetDao;
    const Settings& settings = Settings::instance();
    double totalSpent = 0.0;
    double totalLimit = 0.0;

    for (const Budget& budget : budgetDao.getAllBudgets(userId)) {
        if (today < budget.getStartDate() || today > budget.getEndDate()) {
            continue;
        }
        const double spent = budgetDao.getSpentAmount(budget);
        totalSpent += spent;
        totalLimit += budget.getAmount();

        auto* item = new SidebarItem(SidebarItem::Kind::Row);
        item->setText(budget.getCategory());
        item->setTrailingText(QString("%1 / %2").arg(settings.formatNumber(spent, 0), settings.formatNumber(budget.getAmount(), 0)));
        item->setProgress(budget.getAmount() > 0 ? spent / budget.getAmount() : 0.0);
        budgetSection.bodyLayout->addWidget(item);
        connect(item, &QAbstractButton::clicked, this, [this]() {
            setCurrentPage(Page::Budget);
            emit pageRequested(Page::Budget);
        });
    }

    if (totalLimit <= 0) {
        auto* emptyItem = new SidebarItem(SidebarItem::Kind::Row);
        emptyItem->setText("No budgets this month");
        emptyItem->setMuted(true);
        budgetSection.bodyLayout->addWidget(emptyItem);
        budgetSection.summary.clear();
    } else {
        budgetSection.summary = QString("%1%").arg(qRound(totalSpent / totalLimit * 100.0));
    }
    setSectionExpanded(budgetSection, budgetSection.expanded);
}

void SidebarWidget::showUserMenu() {
    QMenu menu(this);
    menu.addAction("Switch user…", this, &SidebarWidget::switchUserRequested);
    menu.exec(userItem->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void SidebarWidget::paintEvent(QPaintEvent*) {
    const Theme::Colors colors = Theme::colors();
    QPainter painter(this);
    painter.fillRect(rect(), colors.sidebarBackground);
    painter.fillRect(QRect(width() - 1, 0, 1, height()), colors.sidebarBorder);
}
