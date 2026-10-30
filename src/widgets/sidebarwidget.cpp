#include "sidebarwidget.h"
#include "account.h"
#include "budget.h"
#include "categorycolors.h"
#include "settings.h"
#include "theme.h"
#include "user.h"
#include "icons.h"

#include <QAbstractButton>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

namespace {

constexpr int EXPANDED_WIDTH = 240;
constexpr int COLLAPSED_WIDTH = 52;
constexpr int ICON_X = 9;                            // Left edge of the icon column
constexpr int TEXT_X = 38;                           // Left edge of the label column
constexpr int PADDING_RIGHT = 9;
constexpr int SIDEBAR_BUDGETS = 5;                   // Budgets shown unless all are wanted

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
    void setDotColor(const QColor& color) { dotColor = color; update(); }   // Before the label; invalid = none
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

        // Category colour dot, then the label (section headers start in the icon column)
        int textX = (kind == Kind::SectionHeader) ? ICON_X : TEXT_X;
        if (dotColor.isValid()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            painter.drawEllipse(QRectF(textX, (lineHeight - 8) / 2.0, 8, 8));
            textX += 16;
        }
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
            painter.setBrush(progress > 1.0 ? colors.danger : progress == 1.0 ? colors.textMuted : colors.accent);   // Over / used up
            painter.drawRoundedRect(fill, 2, 2);
        }
    }

private:
    Kind kind;
    QString iconName;
    QString trailingText;
    QString trailingIcon;
    QString avatarInitials;
    QColor dotColor;
    double progress = -1.0;
    bool trailingNegative = false;
    bool muted = false;
};

SidebarWidget::SidebarWidget(QWidget* parent)
    : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 12, 8, 12);
    root->setSpacing(2);

    // Logo and name; the collapsed rail keeps only the logo
    auto* brand = new QWidget(this);
    brand->setObjectName("sidebarBrand");
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(4, 0, 0, 12);
    brandLayout->setSpacing(10);
    auto* logo = new QLabel(brand);
    logo->setObjectName("sidebarLogo");
    logo->setPixmap(Icons::logo(28, devicePixelRatioF()));
    logo->setAccessibleName("Aion logo");
    brandLayout->addWidget(logo);
    brandName = new QLabel("Aion", brand);
    brandName->setObjectName("sidebarBrandName");
    QFont brandFont = brandName->font();
    brandFont.setPointSize(13);
    brandFont.setWeight(QFont::DemiBold);
    brandName->setFont(brandFont);
    brandLayout->addWidget(brandName, 1);
    root->addWidget(brand);

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
    budgetSection.summaryWhenExpanded = true;        // The month's total, while only some budgets show
    sectionsLayout->addStretch();

    scrollArea->setWidget(sectionsWidget);
    sectionsWidget->setAutoFillBackground(false);    // setWidget() turns it on
    root->addWidget(scrollArea, 1);
    root->addStretch(0);                             // Keeps the rail items packed while the sections are hidden
    sectionsArea = scrollArea;
    styleScrollBar();

    connect(accountsSection.header, &QAbstractButton::clicked, this, [this]() {
        setSectionExpanded(accountsSection, !accountsSection.expanded);
    });
    connect(budgetSection.header, &QAbstractButton::clicked, this, [this]() {
        setSectionExpanded(budgetSection, !budgetSection.expanded);
    });
    budgetSection.header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(budgetSection.header, &QWidget::customContextMenuRequested, this, &SidebarWidget::showBudgetSortMenu);

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
            brandName->setVisible(true);
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
    section.header->setTrailingText(expanded && !section.summaryWhenExpanded ? QString() : section.summary);
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
        brandName->setVisible(false);
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
        brandName->setVisible(!collapsed);
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

    Cents total = 0;
    for (const Account& account : accounts) {
        total += account.getBalanceCents();
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
        const Cents balance = account.getBalanceCents();
        item->setTrailingText(settings.formatCurrency(balance), balance < 0);
        accountsSection.bodyLayout->addWidget(item);
        const int accountId = account.getAccountId();
        connect(item, &QAbstractButton::clicked, this, [this, accountId]() {
            emit accountRequested(accountId);
        });
        item->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(item, &QWidget::customContextMenuRequested, this, [this, item, accountId](const QPoint& position) {
            QMenu menu(this);
            menu.addAction("Show transactions", this, [this, accountId]() { emit accountRequested(accountId); });
            menu.addAction("Correct balance…", this, [this, accountId]() { emit correctBalanceRequested(accountId); });
            menu.addSeparator();
            menu.addAction("Edit account…", this, [this, accountId]() { emit editAccountRequested(accountId); });
            menu.exec(item->mapToGlobal(position));
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
    Cents totalSpent = 0;
    Cents totalLimit = 0;
    const auto showBudgetPage = [this]() {
        setCurrentPage(Page::Budget);
        emit pageRequested(Page::Budget);
    };

    // In the chosen order, and only the first few unless all are wanted; the Budget page has the rest
    QList<MonthBudget> budgets = budgetDao.budgetsForMonth(userId, today);
    const bool showAll = settings.isSidebarShowingAllBudgets();
    const Settings::BudgetSort sort = settings.getSidebarBudgetSort();
    std::stable_sort(budgets.begin(), budgets.end(), [sort](const MonthBudget& a, const MonthBudget& b) {
        switch (sort) {
        case Settings::BudgetSort::Spent:
            return a.spentCents > b.spentCents;
        case Settings::BudgetSort::Budgeted:
            return a.limitCents > b.limitCents;
        case Settings::BudgetSort::Name:
            return QString::compare(a.categoryName, b.categoryName, Qt::CaseInsensitive) < 0;
        case Settings::BudgetSort::Percentage:
            break;
        }
        return double(a.spentCents) / double(a.limitCents) > double(b.spentCents) / double(b.limitCents);
    });
    for (int i = 0; i < budgets.size(); ++i) {
        const MonthBudget& budget = budgets[i];
        totalSpent += budget.spentCents;
        totalLimit += budget.limitCents;
        if (!showAll && i >= SIDEBAR_BUDGETS) continue;

        auto* item = new SidebarItem(SidebarItem::Kind::Row);
        item->setObjectName("budgetItem");
        item->setText(budget.categoryName);
        item->setTrailingText(QString("%1 / %2").arg(settings.formatCurrency(budget.spentCents, false, 0),
                                                     settings.formatCurrency(budget.limitCents, false, 0)));
        // Limits are positive; refunds can take the spending below zero, which shows an empty bar
        item->setProgress(qMax(0.0, double(budget.spentCents) / double(budget.limitCents)));
        if (settings.isCategoryColorsInOverviewsEnabled() && !budget.categoryColor.isEmpty()) {
            item->setDotColor(CategoryColors::swatch(budget.categoryColor).solid);
        }
        budgetSection.bodyLayout->addWidget(item);
        connect(item, &QAbstractButton::clicked, this, showBudgetPage);
    }
    if (!showAll && budgets.size() > SIDEBAR_BUDGETS) {
        auto* moreItem = new SidebarItem(SidebarItem::Kind::Row);
        moreItem->setObjectName("moreBudgetsItem");
        moreItem->setText(QString("+ %1 more on the Budget page").arg(budgets.size() - SIDEBAR_BUDGETS));
        budgetSection.bodyLayout->addWidget(moreItem);
        connect(moreItem, &QAbstractButton::clicked, this, showBudgetPage);
    }

    if (totalLimit <= 0) {
        auto* emptyItem = new SidebarItem(SidebarItem::Kind::Row);
        emptyItem->setText("No budgets this month");
        emptyItem->setMuted(true);
        budgetSection.bodyLayout->addWidget(emptyItem);
        budgetSection.summary.clear();
    } else {
        budgetSection.summary = QString("%1%").arg(qRound(double(totalSpent) / double(totalLimit) * 100.0));
    }
    setSectionExpanded(budgetSection, budgetSection.expanded);
}

void SidebarWidget::showUserMenu() {
    QMenu menu(this);
    menu.addAction("Switch user…", this, &SidebarWidget::switchUserRequested);
    menu.exec(userItem->mapToGlobal(QPoint(0, -menu.sizeHint().height())));
}

void SidebarWidget::showBudgetSortMenu(const QPoint& position) {
    using Sort = Settings::BudgetSort;
    const QList<QPair<Sort, QString>> sorts = {{Sort::Percentage, "Sort by percentage used"},
                                               {Sort::Spent, "Sort by amount spent"},
                                               {Sort::Budgeted, "Sort by budgeted amount"},
                                               {Sort::Name, "Sort by name"}};
    QMenu menu(this);
    const Sort current = Settings::instance().getSidebarBudgetSort();
    for (const auto& [sort, text] : sorts) {
        QAction* action = menu.addAction(text, this, [this, sort]() {
            Settings::instance().setSidebarBudgetSort(sort);
            loadBudgets();
            emit budgetSortChanged();
        });
        action->setObjectName("budgetSortAction");
        action->setCheckable(true);
        action->setChecked(sort == current);
    }
    menu.exec(budgetSection.header->mapToGlobal(position));
}

void SidebarWidget::styleScrollBar() {
    // Fusion's scroll bar, with its track and arrows, is too heavy for the narrow sidebar
    const Theme::Colors colors = Theme::colors();
    QColor handle = colors.textMuted;
    handle.setAlpha(110);
    QColor hovered = colors.textMuted;
    hovered.setAlpha(200);
    sectionsArea->verticalScrollBar()->setStyleSheet(
        QString("QScrollBar:vertical { background: transparent; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background: %1; border-radius: 3px; min-height: 24px; }"
                "QScrollBar::handle:vertical:hover, QScrollBar::handle:vertical:pressed { background: %2; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; border: none; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: none; }")
            .arg(handle.name(QColor::HexArgb), hovered.name(QColor::HexArgb)));
}

void SidebarWidget::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && sectionsArea) {
        styleScrollBar();
    }
    QWidget::changeEvent(event);
}

void SidebarWidget::paintEvent(QPaintEvent*) {
    const Theme::Colors colors = Theme::colors();
    QPainter painter(this);
    painter.fillRect(rect(), colors.sidebarBackground);
    painter.fillRect(QRect(width() - 1, 0, 1, height()), colors.sidebarBorder);
}
