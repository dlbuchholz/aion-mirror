#include "dashboardpage.h"
#include "budget.h"
#include "categorycolors.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"
#include "user.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>

namespace {

const QLocale english(QLocale::English);

void makePrimary(QPushButton* button) {
    QPalette palette = button->palette();
    palette.setColor(QPalette::Button, Theme::colors().accent);
    palette.setColor(QPalette::ButtonText, Theme::colors().onAccent);
    button->setPalette(palette);
}

void setTextColor(QWidget* widget, const QColor& color) {
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::ButtonText, color);
    widget->setPalette(palette);
}

QString dayLabel(const QDate& date, const QDate& today) {
    if (date == today) return "Today";
    if (date == today.addDays(-1)) return "Yesterday";
    return english.toString(date, date.year() == today.year() ? "d MMMM" : "d MMMM yyyy");
}

// Clickable row with a hover background, keyboard activation and custom painting
class DashboardRow : public QAbstractButton {
public:
    explicit DashboardRow(QWidget* parent) : QAbstractButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintBackground(QPainter& painter) {
        if (underMouse() || hasFocus()) {
            QPainterPath path;
            path.addRoundedRect(QRectF(rect()), 6, 6);
            painter.fillPath(path, Theme::colors().itemHover);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }
};

// Name and value above a thin bar (budgets, spending by category)
class ProgressRow : public DashboardRow {
public:
    ProgressRow(const QString& name, const QString& value, double ratio, QWidget* parent)
        : DashboardRow(parent), value(value), ratio(ratio) {
        setObjectName("progressRow");
        setAccessibleName(name);
        setAccessibleDescription(value);
        setText(name);
        setFixedHeight(44);
    }

    QColor valueColor;                               // Invalid = secondary text
    QColor barColor;
    QColor dotColor;                                 // Category colour before the name; invalid = none

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintBackground(painter);

        const QRect content = rect().adjusted(10, 6, -10, -8);
        const QRect line(content.left(), content.top(), content.width(), 20);
        painter.setPen(valueColor.isValid() ? valueColor : colors.textSecondary);
        painter.drawText(line, Qt::AlignRight | Qt::AlignVCenter, value);
        const int valueWidth = fontMetrics().horizontalAdvance(value);
        int nameLeft = 0;
        if (dotColor.isValid()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(dotColor);
            painter.drawEllipse(QRectF(line.left(), line.center().y() - 3.5, 8, 8));
            nameLeft = 15;
        }
        painter.setPen(colors.textPrimary);
        painter.drawText(line.adjusted(nameLeft, 0, -valueWidth - 12, 0), Qt::AlignLeft | Qt::AlignVCenter,
                         fontMetrics().elidedText(text(), Qt::ElideRight, qMax(0, line.width() - valueWidth - 12 - nameLeft)));

        const QRectF track(content.left(), content.bottom() - 5, content.width(), 5);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, 2.5, 2.5);
        painter.fillPath(trackPath, colors.track);
        if (ratio > 0.0) {
            QRectF bar = track;
            bar.setWidth(qMax(5.0, track.width() * qBound(0.0, ratio, 1.0)));
            QPainterPath barPath;
            barPath.addRoundedRect(bar, 2.5, 2.5);
            painter.fillPath(barPath, barColor.isValid() ? barColor : colors.accent);
        }
    }

private:
    QString value;
    double ratio = 0.0;
};

// Payee and day, category pill and signed amount
class RecentRow : public DashboardRow {
public:
    RecentRow(const RecentTransaction& transaction, const QDate& today, QWidget* parent)
        : DashboardRow(parent), transaction(transaction), day(dayLabel(transaction.date, today)) {
        setObjectName("recentRow");
        setAccessibleName(transaction.payee.isEmpty() ? QString("No payee") : transaction.payee);
        setAccessibleDescription(Settings::instance().formatCurrency(transaction.signedCents, true));
        setFixedHeight(38);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        paintBackground(painter);

        const QRect content = rect().adjusted(10, 0, -10, 0);
        QFont smallFont = font();
        smallFont.setPointSizeF(font().pointSizeF() * 0.9);
        const QFontMetrics smallMetrics(smallFont);

        const Cents amount = transaction.signedCents;
        const int amountWidth = 120;
        painter.setPen(amount > 0 ? colors.positive : colors.textPrimary);
        painter.drawText(QRect(content.right() - amountWidth, content.top(), amountWidth, content.height()),
                         Qt::AlignRight | Qt::AlignVCenter, Settings::instance().formatCurrency(amount, amount > 0));
        int right = content.right() - amountWidth - 12;

        const int pillArea = 150;
        if (!transaction.categoryName.isEmpty()) {
            painter.setFont(smallFont);
            const QString text = smallMetrics.elidedText(transaction.categoryName, Qt::ElideRight, pillArea - 16);
            const int pillWidth = smallMetrics.horizontalAdvance(text) + 16;
            const QRectF pill(right - pillWidth, content.center().y() - 10, pillWidth, 20);
            QPainterPath pillPath;
            pillPath.addRoundedRect(pill, 10, 10);
            const CategoryColors::Swatch swatch = CategoryColors::swatch(transaction.categoryColor);
            painter.fillPath(pillPath, swatch.background);
            painter.setPen(swatch.text);
            painter.drawText(pill, Qt::AlignCenter, text);
        }
        right -= pillArea + 12;

        // Payee, then the day in muted text
        const QString payee = accessibleName();
        const int available = qMax(0, right - content.left());
        painter.setFont(font());
        const QString shownPayee = fontMetrics().elidedText(payee, Qt::ElideRight, available);
        painter.setPen(transaction.payee.isEmpty() ? colors.textMuted : colors.textPrimary);
        painter.drawText(QRect(content.left(), content.top(), available, content.height()), Qt::AlignLeft | Qt::AlignVCenter, shownPayee);
        const int dayLeft = content.left() + fontMetrics().horizontalAdvance(shownPayee) + 8;
        if (dayLeft < right) {
            painter.setFont(smallFont);
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(dayLeft, content.top(), right - dayLeft, content.height()), Qt::AlignLeft | Qt::AlignVCenter,
                             smallMetrics.elidedText(day, Qt::ElideRight, right - dayLeft));
        }
    }

private:
    RecentTransaction transaction;
    QString day;
};

} // namespace

// Income and expense bars per month; hovering a month shows its numbers
class CashFlowChart : public QWidget {
public:
    explicit CashFlowChart(QWidget* parent) : QWidget(parent) {
        setObjectName("cashFlowChart");
        setMouseTracking(true);
        setMinimumHeight(180);
    }

    void setMonths(const QList<MonthFlow>& newMonths) {
        months = newMonths;
        hovered = -1;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        Cents highest = 0;
        for (const MonthFlow& month : months) highest = qMax(highest, qMax(month.incomeCents, month.expenseCents));
        const QRect plot = plotRect();
        if (highest == 0) {
            painter.setPen(colors.textSecondary);
            painter.drawText(rect(), Qt::AlignCenter, "No income or expenses in these months yet.");
            return;
        }

        const int barWidth = qBound(8, groupWidth() / 5, 22);
        for (int i = 0; i < months.size(); ++i) {
            const QRect group = groupRect(i);
            if (i == hovered) {
                QPainterPath path;
                path.addRoundedRect(QRectF(group.adjusted(4, -4, -4, 22)), 6, 6);
                painter.fillPath(path, colors.itemHover);
            }
            const bool current = i == months.size() - 1;
            const QList<QPair<Cents, QColor>> bars = {{months[i].incomeCents, colors.positive},
                                                      {months[i].expenseCents, colors.textMuted}};
            int x = group.center().x() - barWidth - 2;
            for (const auto& bar : bars) {
                const double height = double(bar.first) / double(highest) * plot.height();
                if (height > 0) {
                    QColor color = bar.second;
                    if (!current && i != hovered) color.setAlphaF(color.alphaF() * 0.75);
                    QPainterPath path;
                    path.addRoundedRect(QRectF(x, plot.bottom() - qMax(height, 3.0), barWidth, qMax(height, 3.0) + 3), 3, 3);
                    painter.save();
                    painter.setClipRect(QRectF(x, plot.top(), barWidth, plot.height() + 1));
                    painter.fillPath(path, color);
                    painter.restore();
                }
                x += barWidth + 4;
            }
            QFont labelFont = font();
            labelFont.setPointSizeF(font().pointSizeF() * 0.9);
            labelFont.setWeight(current ? QFont::DemiBold : QFont::Normal);
            painter.setFont(labelFont);
            painter.setPen(current ? colors.textPrimary : colors.textSecondary);
            painter.drawText(QRect(group.left(), plot.bottom() + 4, group.width(), 18), Qt::AlignCenter,
                             english.toString(months[i].month, "MMM"));
        }
        painter.setPen(colors.sidebarBorder);
        painter.drawLine(plot.bottomLeft() + QPoint(0, 1), plot.bottomRight() + QPoint(0, 1));

        // Legend in the top right corner
        QFont legendFont = font();
        legendFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(legendFont);
        const QFontMetrics metrics(legendFont);
        int right = rect().right();
        for (const auto& entry : QList<QPair<QString, QColor>>{{"Expenses", colors.textMuted}, {"Income", colors.positive}}) {
            const int width = metrics.horizontalAdvance(entry.first);
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(right - width, 0, width, 16), Qt::AlignRight | Qt::AlignVCenter, entry.first);
            QPainterPath swatch;
            swatch.addRoundedRect(QRectF(right - width - 14, 4, 9, 9), 2, 2);
            painter.fillPath(swatch, entry.second);
            right -= width + 30;
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        int index = -1;
        for (int i = 0; i < months.size(); ++i) {
            if (groupRect(i).adjusted(0, -8, 0, 24).contains(event->position().toPoint())) index = i;
        }
        if (index != hovered) {
            hovered = index;
            update();
        }
        if (index >= 0) {
            const Settings& settings = Settings::instance();
            const MonthFlow& month = months[index];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1\nIncome %2\nExpenses %3\nNet %4")
                                   .arg(english.toString(month.month, "MMMM yyyy"), settings.formatCurrency(month.incomeCents),
                                        settings.formatCurrency(month.expenseCents), settings.formatCurrency(month.netCents(), true)),
                               this);
        } else {
            QToolTip::hideText();
        }
    }

    void leaveEvent(QEvent* event) override {
        hovered = -1;
        update();
        QWidget::leaveEvent(event);
    }

private:
    QRect plotRect() const { return rect().adjusted(0, 24, 0, -24); }   // Legend above, month names below
    int groupWidth() const { return months.isEmpty() ? 0 : plotRect().width() / int(months.size()); }
    QRect groupRect(int index) const {
        const QRect plot = plotRect();
        return QRect(plot.left() + index * groupWidth(), plot.top(), groupWidth(), plot.height());
    }

    QList<MonthFlow> months;
    int hovered = -1;
};

DashboardPage::DashboardPage(QWidget* parent)
    : QWidget(parent) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto* content = new QWidget(scroll);
    content->setObjectName("dashboardContent");
    auto* root = new QVBoxLayout(content);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);
    scroll->setWidget(content);
    outer->addWidget(scroll);

    // Greeting, date and the quick actions
    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    greetingLabel = new QLabel(content);
    greetingLabel->setObjectName("greetingLabel");
    QFont titleFont = greetingLabel->font();
    titleFont.setPointSize(16);
    greetingLabel->setFont(titleFont);
    dateLabel = new QLabel(content);
    dateLabel->setObjectName("dateLabel");
    dateLabel->setForegroundRole(QPalette::PlaceholderText);
    titles->addWidget(greetingLabel);
    titles->addWidget(dateLabel);
    header->addLayout(titles, 1);
    importButton = new QPushButton("Import", content);
    importButton->setObjectName("dashboardImportButton");
    connect(importButton, &QPushButton::clicked, this, &DashboardPage::importRequested);
    header->addWidget(importButton, 0, Qt::AlignTop);
    addButton = new QPushButton("Add transaction", content);
    addButton->setObjectName("dashboardAddButton");
    makePrimary(addButton);
    connect(addButton, &QPushButton::clicked, this, &DashboardPage::addTransactionRequested);
    header->addWidget(addButton, 0, Qt::AlignTop);
    root->addLayout(header);

    // This month at a glance
    auto* tiles = new QHBoxLayout();
    tiles->setSpacing(10);
    balanceTile = createTile("balance", "Total balance", tiles);
    incomeTile = createTile("income", "Income", tiles);
    expensesTile = createTile("expenses", "Expenses", tiles);
    savedTile = createTile("saved", "Saved", tiles);
    averageTile = createTile("average", "Daily average", tiles);
    root->addLayout(tiles);

    QVBoxLayout* cashFlowBody = nullptr;
    QFrame* cashFlowCard = createCard("Cash flow · last 6 months", &cashFlowBody);
    cashFlowTitle = cashFlowCard->findChild<QLabel*>("cardTitle");
    chart = new CashFlowChart(cashFlowCard);
    cashFlowBody->addWidget(chart);
    root->addWidget(cashFlowCard);

    auto* columns = new QHBoxLayout();
    columns->setSpacing(12);
    QToolButton* budgetLink = nullptr;
    QFrame* budgetCard = createCard("Budget", &budgetLayout, &budgetLink, "Open budget");
    budgetTitle = budgetCard->findChild<QLabel*>("cardTitle");
    columns->addWidget(budgetCard, 1);
    connect(budgetLink, &QToolButton::clicked, this, &DashboardPage::budgetRequested);
    QFrame* categoryCard = createCard("Spending by category", &categoryLayout);
    categoryTitle = categoryCard->findChild<QLabel*>("cardTitle");
    columns->addWidget(categoryCard, 1);
    root->addLayout(columns);

    QToolButton* transactionsLink = nullptr;
    root->addWidget(createCard("Recent transactions", &recentLayout, &transactionsLink, "All transactions"));
    connect(transactionsLink, &QToolButton::clicked, this, &DashboardPage::transactionsRequested);
    root->addStretch();

    updateIcons();
}

DashboardPage::Tile DashboardPage::createTile(const QString& name, const QString& caption, QLayout* layout) {
    auto* tile = new QFrame(this);
    tile->setObjectName(name + "Tile");
    tile->setFrameShape(QFrame::StyledPanel);
    auto* tileLayout = new QVBoxLayout(tile);
    tileLayout->setContentsMargins(14, 10, 14, 10);
    tileLayout->setSpacing(2);
    auto* captionLabel = new QLabel(caption, tile);
    captionLabel->setObjectName("tileCaption");
    captionLabel->setForegroundRole(QPalette::PlaceholderText);
    Tile result;
    result.value = new QLabel(tile);
    result.value->setObjectName(name + "Value");
    QFont valueFont = result.value->font();
    valueFont.setPointSize(15);
    result.value->setFont(valueFont);
    result.detail = new QLabel(tile);
    result.detail->setObjectName(name + "Detail");
    tileLayout->addWidget(captionLabel);
    tileLayout->addWidget(result.value);
    tileLayout->addWidget(result.detail);
    layout->addWidget(tile);
    return result;
}

QFrame* DashboardPage::createCard(const QString& title, QVBoxLayout** body, QToolButton** link, const QString& linkText) {
    auto* card = new QFrame(this);
    card->setObjectName("dashboardCard");
    card->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(4);

    auto* header = new QHBoxLayout();
    header->setContentsMargins(6, 0, 0, 0);
    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("cardTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setWeight(QFont::DemiBold);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel, 1);
    if (link) {
        *link = new QToolButton(card);
        (*link)->setText(linkText);
        (*link)->setObjectName(linkText == "Open budget" ? "openBudgetLink" : "allTransactionsLink");
        (*link)->setAutoRaise(true);
        (*link)->setCursor(Qt::PointingHandCursor);
        links.append(*link);
        header->addWidget(*link);
    }
    layout->addLayout(header);
    *body = new QVBoxLayout();
    (*body)->setSpacing(0);
    layout->addLayout(*body);
    layout->addStretch();
    return card;
}

void DashboardPage::updateIcons() {
    importButton->setIcon(Icons::icon("import", palette().color(QPalette::ButtonText)));
    addButton->setIcon(Icons::icon("plus", Theme::colors().onAccent));
    for (QToolButton* link : links) {
        setTextColor(link, Theme::colors().accent);
    }
}

void DashboardPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && importButton && recentLayout) {
        updateIcons();
        updateTiles();                               // Coloured details
    }
    QWidget::changeEvent(event);
}

void DashboardPage::setUserId(int newUserId) {
    userId = newUserId;
    const QString name = UserDAO().getUserById(userId).getName().trimmed();
    userName = name.section(' ', 0, 0);              // First name is friendlier
    reload();
}

void DashboardPage::applySettings() {
    reload();
}

void DashboardPage::reload() {
    if (userId == -1) {
        return;
    }
    const QDate today = QDate::currentDate();
    data = Dashboard::load(userId, today);

    const QString greeting = Dashboard::greeting(QTime::currentTime());
    greetingLabel->setText(userName.isEmpty() ? greeting : QString("%1, %2").arg(greeting, userName));
    const int left = data.daysLeftInMonth;
    dateLabel->setText(QString("%1 · %2").arg(english.toString(today, "dddd, d MMMM"),
                                             left == 0 ? QString("Last day of the month")
                                                       : QString("%1 %2 left this month").arg(left).arg(left == 1 ? "day" : "days")));

    updateTiles();
    chart->setMonths(data.cashFlow);
    updateBudgets();
    updateCategories();
    updateRecent();
}

void DashboardPage::setDetail(QLabel* label, const QString& text, const QColor& color) {
    label->setText(text);
    setTextColor(label, color.isValid() ? color : Theme::colors().textSecondary);
}

void DashboardPage::updateTiles() {
    if (userId == -1) {
        return;
    }
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    const MonthFlow& month = data.thisMonth;
    const QString monthName = english.toString(month.month, "MMMM");
    const QString lastMonthName = english.toString(data.lastMonth.month, "MMMM");
    auto count = [](int n, const char* what) { return QString("%1 %2%3").arg(n).arg(what).arg(n == 1 ? "" : "s"); };

    balanceTile.value->setText(settings.formatCurrency(data.totalBalanceCents));
    const Cents net = month.netCents();
    setDetail(balanceTile.detail, net == 0 ? QString("No change this month")
                                           : QString("%1 this month").arg(settings.formatCurrency(net, true)),
              net > 0 ? colors.positive : net < 0 ? colors.danger : QColor());

    incomeTile.value->setText(settings.formatCurrency(month.incomeCents));
    setTextColor(incomeTile.value, month.incomeCents > 0 ? colors.positive : palette().color(QPalette::WindowText));
    setDetail(incomeTile.detail, QString("%1 in %2").arg(count(month.incomeCount, "transaction"), monthName));

    expensesTile.value->setText(settings.formatCurrency(month.expenseCents));
    const Cents before = data.lastMonthToDateExpenseCents;
    if (before > 0) {
        const int change = qRound((double(month.expenseCents) - double(before)) / double(before) * 100.0);
        setDetail(expensesTile.detail, change == 0 ? QString("Same as %1").arg(lastMonthName)
                                                   : QString("%1%2% vs. %3").arg(change > 0 ? "+" : "−").arg(qAbs(change)).arg(lastMonthName));
        expensesTile.detail->setToolTip(QString("Compared with %1 by the same day: %2")
                                            .arg(lastMonthName, settings.formatCurrency(before)));
    } else {
        setDetail(expensesTile.detail, QString("%1 in %2").arg(count(month.expenseCount, "transaction"), monthName));
        expensesTile.detail->setToolTip(QString());
    }

    savedTile.value->setText(settings.formatCurrency(net));
    setTextColor(savedTile.value, net < 0 ? colors.danger : palette().color(QPalette::WindowText));
    if (net < 0) {
        setDetail(savedTile.detail, "More spent than earned");
    } else if (month.incomeCents > 0) {
        setDetail(savedTile.detail, QString("%1% of income").arg(qRound(double(net) / double(month.incomeCents) * 100.0)));
    } else {
        setDetail(savedTile.detail, "No income this month yet");
    }

    averageTile.value->setText(settings.formatCurrency(data.dailyAverageCents));
    setDetail(averageTile.detail, "Spent per day so far");
}

void DashboardPage::updateBudgets() {
    while (QLayoutItem* item = budgetLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    QList<MonthBudget> budgets = BudgetDAO().budgetsForMonth(userId, data.thisMonth.month);
    budgetTitle->setText(QString("Budget · %1").arg(english.toString(data.thisMonth.month, "MMMM")));

    if (budgets.isEmpty()) {
        auto* empty = new QLabel("No budgets this month. Set limits per category to see how much is left.", this);
        empty->setObjectName("budgetEmptyLabel");
        empty->setWordWrap(true);
        empty->setContentsMargins(6, 8, 6, 8);
        empty->setForegroundRole(QPalette::PlaceholderText);
        budgetLayout->addWidget(empty);
        return;
    }

    // The ones closest to their limit (or past it) first
    std::sort(budgets.begin(), budgets.end(), [](const MonthBudget& a, const MonthBudget& b) {
        return double(a.spentCents) / double(a.limitCents) > double(b.spentCents) / double(b.limitCents);
    });
    Cents limit = 0;
    Cents spent = 0;
    for (const MonthBudget& budget : budgets) {
        limit += budget.limitCents;
        spent += budget.spentCents;
    }
    const int shown = qMin(3, int(budgets.size()));
    for (int i = 0; i < shown; ++i) {
        const MonthBudget& budget = budgets[i];
        const Cents left = budget.leftCents();
        const QString value = left < 0 ? QString("%1 over").arg(settings.formatCurrency(-left))
                                       : QString("%1 left").arg(settings.formatCurrency(left));
        auto* row = new ProgressRow(budget.categoryName, value, double(budget.spentCents) / double(budget.limitCents), this);
        row->setObjectName("budgetProgressRow");
        if (left < 0) row->valueColor = colors.danger;
        row->barColor = left < 0 ? colors.danger : left == 0 ? colors.textMuted : colors.accent;
        if (settings.isCategoryColorsInOverviewsEnabled() && !budget.categoryColor.isEmpty()) {
            row->dotColor = CategoryColors::swatch(budget.categoryColor).solid;
        }
        connect(row, &QAbstractButton::clicked, this, &DashboardPage::budgetRequested);
        budgetLayout->addWidget(row);
    }
    const int more = int(budgets.size()) - shown;
    auto* summary = new QLabel(QString("%1 of %2 spent%3").arg(settings.formatCurrency(spent), settings.formatCurrency(limit),
                                                              more > 0 ? QString(" · %1 more %2").arg(more).arg(more == 1 ? "budget" : "budgets")
                                                                       : QString()), this);
    summary->setObjectName("budgetSummary");
    summary->setContentsMargins(10, 6, 10, 2);
    summary->setForegroundRole(QPalette::PlaceholderText);
    budgetLayout->addWidget(summary);
}

void DashboardPage::updateCategories() {
    while (QLayoutItem* item = categoryLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    categoryTitle->setText(QString("Spending by category · %1").arg(english.toString(data.thisMonth.month, "MMMM")));
    if (data.spendingByCategory.isEmpty()) {
        auto* empty = new QLabel("No expenses this month yet.", this);
        empty->setObjectName("categoryEmptyLabel");
        empty->setContentsMargins(6, 8, 6, 8);
        empty->setForegroundRole(QPalette::PlaceholderText);
        categoryLayout->addWidget(empty);
        return;
    }
    // Refunded categories (below zero) come last and get no bar
    const Cents largest = qMax<Cents>(1, data.spendingByCategory.first().spentCents);
    for (const CategorySpending& spending : data.spendingByCategory.mid(0, 5)) {
        auto* row = new ProgressRow(spending.categoryName, Settings::instance().formatSpending(spending.spentCents),
                                    double(spending.spentCents) / double(largest), this);
        row->setObjectName("categoryRow");
        if (Settings::instance().isCategoryColorsInOverviewsEnabled()) {
            const QColor solid = CategoryColors::swatch(spending.categoryColor).solid;
            row->barColor = solid;
            row->dotColor = solid;
        } else {
            row->barColor = Theme::colors().textMuted;   // Neutral bars, no dots
        }
        row->setToolTip(QString("Show %1 transactions this month").arg(spending.categoryName));
        const int categoryId = spending.categoryId;
        connect(row, &QAbstractButton::clicked, this, [this, categoryId]() { emit categoryRequested(categoryId); });
        categoryLayout->addWidget(row);
    }
}

void DashboardPage::updateRecent() {
    while (QLayoutItem* item = recentLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    if (data.recent.isEmpty()) {
        auto* empty = new QLabel("No transactions yet. Add one or import a bank export to get started.", this);
        empty->setObjectName("recentEmptyLabel");
        empty->setContentsMargins(6, 8, 6, 8);
        empty->setForegroundRole(QPalette::PlaceholderText);
        recentLayout->addWidget(empty);
        return;
    }
    const QDate today = QDate::currentDate();
    for (const RecentTransaction& transaction : data.recent) {
        auto* row = new RecentRow(transaction, today, this);
        const int transactionId = transaction.transactionId;
        connect(row, &QAbstractButton::clicked, this, [this, transactionId]() { emit transactionRequested(transactionId); });
        recentLayout->addWidget(row);
    }
}
