#include "budgetdetail.h"
#include "amountcolor.h"
#include "settings.h"
#include "theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <cmath>

namespace {

QString shortDate(const QDate& date) {
    return QLocale(QLocale::English).toString(date, "d MMM");
}

QString payeeText(const QString& payee) {
    return payee.isEmpty() ? QString("No payee") : payee;
}

bool isCurrentMonth(const QDate& month, const QDate& today) {
    return today.year() == month.year() && today.month() == month.month();
}

// A caption and a value below it
QLabel* addStat(QVBoxLayout* layout, const QString& caption, const QString& value, const QString& name,
                const QColor& color = QColor()) {
    auto* captionLabel = new QLabel(caption);
    captionLabel->setObjectName(name + "Caption");
    captionLabel->setForegroundRole(QPalette::PlaceholderText);
    auto* valueLabel = new QLabel(value);
    valueLabel->setObjectName(name);
    QFont font = valueLabel->font();
    font.setPointSizeF(font.pointSizeF() * 1.15);
    font.setWeight(QFont::DemiBold);
    valueLabel->setFont(font);
    if (color.isValid()) {
        QPalette palette = valueLabel->palette();
        palette.setColor(QPalette::WindowText, color);
        valueLabel->setPalette(palette);
    }
    auto* stat = new QVBoxLayout();
    stat->setSpacing(0);
    stat->addWidget(captionLabel);
    stat->addWidget(valueLabel);
    layout->addLayout(stat);
    return valueLabel;
}

} // namespace

HoverRow::HoverRow(QWidget* parent)
    : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::TabFocus);
    setAttribute(Qt::WA_Hover);
}

void HoverRow::setStrongHighlight(bool strong) {
    strongHighlight = strong;
    update();
}

void HoverRow::paintEvent(QPaintEvent*) {
    if (!underMouse() && !hasFocus()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()), 6, 6);
    painter.fillPath(path, strongHighlight ? Theme::colors().itemSelected : Theme::colors().itemHover);
}

void HoverRow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        click();
        return;
    }
    QAbstractButton::keyPressEvent(event);
}

BudgetMonthChart::BudgetMonthChart(const QDate& month, Cents limitCents, const QList<BudgetTransaction>& transactions,
                                   const QDate& today, QWidget* parent)
    : QWidget(parent), month(month), limitCents(limitCents), transactions(transactions), today(today) {
    setObjectName("budgetMonthChart");
    setMouseTracking(true);
    Cents total = 0;
    highest = qMax<Cents>(1, limitCents);
    for (const BudgetTransaction& transaction : transactions) {
        total += transaction.spentCents;
        totals.append(total);
        lowest = qMin(lowest, total);
        highest = qMax(highest, total);
    }
    highest += (highest - lowest) / 10;                        // Room above the top
}

QSize BudgetMonthChart::sizeHint() const {
    return QSize(380, 170);
}

QSize BudgetMonthChart::minimumSizeHint() const {
    return QSize(200, 140);
}

QRectF BudgetMonthChart::plot() const {
    const int textHeight = fontMetrics().height();
    return QRectF(rect()).adjusted(6, textHeight + 4, -6, -textHeight - 6);
}

double BudgetMonthChart::xFor(double day) const {
    const QRectF area = plot();
    return area.left() + day / month.daysInMonth() * area.width();
}

double BudgetMonthChart::yFor(Cents cents) const {
    const QRectF area = plot();
    return area.bottom() - double(cents - lowest) / double(highest - lowest) * area.height();
}

double BudgetMonthChart::dayOf(int index) const {
    return transactions[index].date.day() - 0.5;
}

double BudgetMonthChart::lastDay() const {
    double end = daysPassed(month, today);
    if (!transactions.isEmpty()) end = qMax(end, dayOf(transactions.size() - 1));   // Dated ahead
    return end;
}

QPointF BudgetMonthChart::dotCenter(int index) const {
    return QPointF(xFor(dayOf(index)), yFor(totals[index]));
}

int BudgetMonthChart::transactionAt(const QPointF& position) const {
    int nearest = -1;
    double nearestDistance = 9.0;                              // Pixels: a little more than a dot
    for (int i = 0; i < transactions.size(); ++i) {
        const QPointF offset = dotCenter(i) - position;
        const double distance = std::hypot(offset.x(), offset.y());
        if (distance < nearestDistance) {
            nearest = i;
            nearestDistance = distance;
        }
    }
    return nearest;
}

QString BudgetMonthChart::dotText(int index) const {
    const BudgetTransaction& transaction = transactions[index];
    return QString("%1 · %2 · %3").arg(shortDate(transaction.date), payeeText(transaction.payee),
                                       Settings::instance().formatSpending(transaction.spentCents));
}

void BudgetMonthChart::paintEvent(QPaintEvent*) {
    const Theme::Colors colors = Theme::colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF area = plot();
    const int days = month.daysInMonth();
    QFont small = font();
    small.setPointSizeF(small.pointSizeF() * 0.85);
    painter.setFont(small);

    // Even pace: nothing on the first, the limit at the end of the month
    painter.setPen(QPen(colors.textMuted, 1));
    painter.drawLine(QPointF(xFor(0), yFor(0)), QPointF(xFor(days), yFor(limitCents)));

    // The limit, named above its line
    painter.setPen(QPen(colors.textMuted, 1, Qt::DashLine));
    painter.drawLine(QPointF(area.left(), yFor(limitCents)), QPointF(area.right(), yFor(limitCents)));
    painter.setPen(colors.textSecondary);
    painter.drawText(QRectF(area.left(), yFor(limitCents) - fontMetrics().height() - 2, area.width(), fontMetrics().height()),
                     Qt::AlignRight | Qt::AlignBottom,
                     QString("Limit %1").arg(Settings::instance().formatCurrency(limitCents)));

    // Days along the bottom, and today
    const QRectF labels(area.left(), area.bottom() + 4, area.width(), fontMetrics().height());
    painter.drawText(labels, Qt::AlignLeft | Qt::AlignTop, "1");
    painter.drawText(labels, Qt::AlignRight | Qt::AlignTop, QString::number(days));
    if (isCurrentMonth(month, today)) {
        const double x = xFor(today.day() - 0.5);
        painter.setPen(QPen(colors.textMuted, 1, Qt::DotLine));
        painter.drawLine(QPointF(x, area.top()), QPointF(x, area.bottom()));
        painter.setPen(colors.textSecondary);
        painter.drawText(QRectF(x - 40, labels.top(), 80, labels.height()), Qt::AlignHCenter | Qt::AlignTop, "today");
    }

    // The spending, a step up (or down, for money back) with every transaction
    QPainterPath line(QPointF(xFor(0), yFor(0)));
    Cents total = 0;
    for (int i = 0; i < transactions.size(); ++i) {
        line.lineTo(xFor(dayOf(i)), yFor(total));
        total = totals[i];
        line.lineTo(xFor(dayOf(i)), yFor(total));
    }
    line.lineTo(xFor(lastDay()), yFor(total));
    painter.setPen(QPen(colors.accent, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(line);

    painter.setPen(Qt::NoPen);
    for (int i = 0; i < transactions.size(); ++i) {
        const QColor color = transactions[i].spentCents < 0 ? colors.positive
                           : totals[i] > limitCents          ? colors.danger
                                                              : colors.accent;
        painter.setBrush(color);
        const double radius = i == hovered ? 5.5 : 4.0;
        painter.drawEllipse(dotCenter(i), radius, radius);
    }
}

void BudgetMonthChart::mouseMoveEvent(QMouseEvent* event) {
    const int index = transactionAt(event->position());
    if (index != hovered) {
        hovered = index;
        update();
    }
    if (index >= 0) {
        setCursor(Qt::PointingHandCursor);
        QToolTip::showText(event->globalPosition().toPoint(), dotText(index), this);
    } else {
        unsetCursor();
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void BudgetMonthChart::mouseReleaseEvent(QMouseEvent* event) {
    const int index = event->button() == Qt::LeftButton ? transactionAt(event->position()) : -1;
    if (index >= 0) {
        emit transactionClicked(transactions[index].transactionId);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void BudgetMonthChart::leaveEvent(QEvent* event) {
    hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

BudgetDetail::BudgetDetail(const MonthBudget& budget, const QDate& month, const QList<BudgetTransaction>& transactions,
                           Cents lastMonthSpent, Cents lastMonthLimit, const QDate& today, QWidget* parent)
    : QFrame(parent) {
    setObjectName("budgetDetail");
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    const QLocale english(QLocale::English);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 6, 16, 12);
    layout->setSpacing(10);

    // The chart, with how it's going beside it
    auto* top = new QHBoxLayout();
    top->setSpacing(20);
    auto* chart = new BudgetMonthChart(month, budget.limitCents, transactions, today, this);
    connect(chart, &BudgetMonthChart::transactionClicked, this, &BudgetDetail::transactionRequested);
    top->addWidget(chart, 3);

    auto* stats = new QVBoxLayout();
    stats->setSpacing(8);
    const int days = month.daysInMonth();
    const int passed = daysPassed(month, today);
    const bool current = isCurrentMonth(month, today);
    if (passed > 0) {
        addStat(stats, current ? "Per day so far" : "Per day", settings.formatSpending(budget.spentCents / passed), "perDayValue");
    }
    if (current) {
        const Cents projected = budget.spentCents * days / passed;
        addStat(stats, "At this pace by month end", settings.formatSpending(projected), "paceValue",
                projected > budget.limitCents ? colors.danger : QColor());
        const int daysLeft = days - today.day() + 1;
        const Cents left = budget.leftCents();
        addStat(stats, "Left per day until month end",
                left > 0 ? settings.formatCurrency(left / daysLeft) : QString("Nothing left"), "leftPerDayValue",
                left > 0 ? QColor() : colors.danger);
    }
    const QString lastMonthName = english.toString(month.addMonths(-1), "MMMM");
    addStat(stats, lastMonthName,
            lastMonthLimit > 0 ? QString("%1 of %2").arg(settings.formatSpending(lastMonthSpent), settings.formatCurrency(lastMonthLimit))
                               : QString("%1, no budget").arg(settings.formatSpending(lastMonthSpent)),
            "lastMonthValue", lastMonthLimit > 0 && lastMonthSpent > lastMonthLimit ? colors.danger : QColor());
    stats->addStretch();
    top->addLayout(stats, 2);
    layout->addLayout(top);

    // The latest transactions, newest first
    const QString monthName = english.toString(month, "MMMM");
    if (transactions.isEmpty()) {
        auto* none = new QLabel(QString("No transactions in %1 yet.").arg(monthName), this);
        none->setObjectName("budgetDetailEmpty");
        none->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(none);
    }
    auto* list = new QVBoxLayout();
    list->setSpacing(0);
    for (int i = transactions.size() - 1; i >= qMax(0, int(transactions.size()) - ShownTransactions); --i) {
        const BudgetTransaction& transaction = transactions[i];
        auto* row = new HoverRow(this);
        row->setObjectName("budgetDetailTransaction");
        row->setStrongHighlight(true);
        row->setAccessibleName(payeeText(transaction.payee));
        row->setToolTip("Open this transaction");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(8, 4, 8, 4);
        auto* date = new QLabel(shortDate(transaction.date), row);
        date->setForegroundRole(QPalette::PlaceholderText);
        date->setFixedWidth(date->fontMetrics().horizontalAdvance("30 May") + 12);
        auto* payee = new QLabel(payeeText(transaction.payee), row);
        auto* amount = new QLabel(settings.formatCurrency(-transaction.spentCents, true), row);
        amount->setObjectName("budgetDetailAmount");
        QPalette amountPalette = amount->palette();
        amountPalette.setColor(QPalette::WindowText, amountColor(-transaction.spentCents, budget.categoryName));
        amount->setPalette(amountPalette);
        for (QLabel* label : {date, payee, amount}) label->setAttribute(Qt::WA_TransparentForMouseEvents);
        rowLayout->addWidget(date);
        rowLayout->addWidget(payee, 1);
        rowLayout->addWidget(amount);
        const int id = transaction.transactionId;
        connect(row, &QAbstractButton::clicked, this, [this, id]() { emit transactionRequested(id); });
        list->addWidget(row);
    }
    layout->addLayout(list);

    // On to all of them, or to the editor
    auto* footer = new QHBoxLayout();
    if (!transactions.isEmpty()) {
        auto* all = new QToolButton(this);
        all->setObjectName("budgetDetailTransactions");
        all->setAutoRaise(true);
        all->setCursor(Qt::PointingHandCursor);
        all->setText(transactions.size() == 1 ? QString("Show it on the Transactions page ›")
                                              : QString("Show all %1 transactions ›").arg(transactions.size()));
        QPalette linkPalette = all->palette();
        linkPalette.setColor(QPalette::ButtonText, colors.accent);
        all->setPalette(linkPalette);
        connect(all, &QToolButton::clicked, this, &BudgetDetail::transactionsRequested);
        footer->addWidget(all);
    }
    footer->addStretch();
    auto* edit = new QPushButton("Edit budget", this);
    edit->setObjectName("budgetDetailEdit");
    connect(edit, &QPushButton::clicked, this, &BudgetDetail::editRequested);
    footer->addWidget(edit);
    layout->addLayout(footer);
}

void BudgetDetail::paintEvent(QPaintEvent*) {
    // Joins the row above it: square at the top, rounded at the bottom
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(0, 0, 0, -1), 6, 6);
    path.addRect(QRectF(0, 0, width(), 8));
    painter.fillPath(path.simplified(), Theme::colors().itemHover);
}
