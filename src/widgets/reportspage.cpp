#include "reportspage.h"
#include "account.h"
#include "budgetcharts.h"
#include "category.h"
#include "categorycolors.h"
#include "icons.h"
#include "periodpicker.h"
#include "settings.h"
#include "theme.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

namespace {

const QLocale english(QLocale::English);

const QList<ReportsPage::Report> AllReports = {
    ReportsPage::Report::SpendingByCategory, ReportsPage::Report::IncomeVsSpending, ReportsPage::Report::Balance,
    ReportsPage::Report::CategoryTrend, ReportsPage::Report::TopPayees, ReportsPage::Report::BudgetVsActual,
    ReportsPage::Report::MoneyFlow,
};

void setTextColor(QWidget* widget, const QColor& color) {
    QPalette palette = widget->palette();
    palette.setColor(QPalette::WindowText, color);
    palette.setColor(QPalette::ButtonText, color);
    widget->setPalette(palette);
}

// "+12%", "−6%", "±0%", or "new" when there was nothing before. Up is more spending, so it's the bad direction.
QString changeText(Cents now, Cents before, int& direction) {
    const std::optional<int> change = Reports::changePercent(now, before);
    if (!change) {
        direction = 0;
        return now > 0 ? QString("new") : QString("—");
    }
    direction = *change > 0 ? 1 : *change < 0 ? -1 : 0;
    if (*change == 0) return "±0%";
    return QString("%1%2%").arg(*change > 0 ? "+" : "−").arg(qAbs(*change));
}

QColor changeColor(int direction) {
    const Theme::Colors colors = Theme::colors();
    return direction > 0 ? colors.danger : direction < 0 ? colors.positive : colors.textSecondary;
}

// Part of the total in percent; "<1%" for tiny parts, a dash for refunds
QString shareText(Cents part, Cents total) {
    if (part <= 0 || total <= 0) return "—";
    const int percent = qRound(double(part) * 100.0 / double(total));
    return percent == 0 ? QString("<1%") : QString("%1%").arg(percent);
}

// A week or month of a report: "Week 40 · 1 Oct – 4 Oct 2026", "October 2026"
QString periodTitle(const ReportRange& period, const ReportRange& range) {
    if (range.flowUnit() == ReportRange::Unit::Week) return QString("%1 · %2").arg(period.weekName(), period.label());
    return period.label();
}

// "1st", "2nd", "3rd", "11th", "22nd"
QString ordinal(int day) {
    const char* suffix = "th";
    if (day % 100 < 11 || day % 100 > 13) {
        if (day % 10 == 1) suffix = "st";
        else if (day % 10 == 2) suffix = "nd";
        else if (day % 10 == 3) suffix = "rd";
    }
    return QString::number(day) + suffix;
}

// Above the budget chart: how the month is going against an even pace, or how many months were over
QString budgetSummary(const BudgetReport& report, const QDate& today) {
    const Settings& settings = Settings::instance();
    if (report.months.size() != 1) {
        int withBudget = 0;
        int over = 0;
        for (const BudgetMonth& month : report.months) {
            if (month.limitCents <= 0) continue;
            ++withBudget;
            if (month.spentCents > month.limitCents) ++over;
        }
        if (withBudget == 0) return QString();
        if (over == 0) return "Within the budgets every month";
        return QString("Over the budgets in %1 of %2 %3").arg(over).arg(withBudget).arg(withBudget == 1 ? "month" : "months");
    }
    const BudgetMonth& month = report.months.first();
    if (month.limitCents <= 0 || report.spentByDay.isEmpty()) return QString();
    const Cents soFar = report.spentByDay.last();
    if (today >= month.month.last) {
        return QString("%1 of %2 spent").arg(settings.formatCurrency(soFar), settings.formatCurrency(month.limitCents));
    }
    const int day = int(report.spentByDay.size());
    const Cents pace = month.limitCents * day / month.month.days();
    const Cents difference = soFar - pace;
    QString text = QString("%1 spent by the %2").arg(settings.formatCurrency(soFar), ordinal(day));
    if (difference == 0) return text + ", right on an even pace";
    return text + QString(", %1 %2 an even pace").arg(settings.formatCurrency(qAbs(difference)), difference > 0 ? "over" : "under");
}

QString payeeName(const QString& payee) {
    return payee.isEmpty() ? QString("No payee") : payee;
}

// The colour of a category's bar, or a neutral one when category colours are off in overviews
QColor barColor(const QString& categoryColor) {
    if (!Settings::instance().isCategoryColorsInOverviewsEnabled()) return Theme::colors().textMuted;
    return CategoryColors::swatch(categoryColor).solid;
}

// A line in a report's number list: a name (above a bar, if it has one), then numbers in
// right-aligned columns. The same columns make the heading and the total. Rows that lead
// somewhere are clickable.
class ReportRow : public QAbstractButton {
public:
    enum class Kind { Heading, Item, Total };
    // How a number is coloured; Change follows `direction`
    enum class Tone { Normal, Secondary, Bad, Change };
    struct Column {
        QString text;
        int width = 120;
        Tone tone = Tone::Normal;
    };

    ReportRow(Kind rowKind, const QString& rowName, const QList<Column>& numbers, bool withBar, QWidget* parent)
        : QAbstractButton(parent), kind(rowKind), name(rowName), columns(numbers), hasBar(withBar && rowKind == Kind::Item) {
        setFixedHeight(kind == Kind::Heading ? 24 : hasBar ? 44 : kind == Kind::Total ? 36 : 32);
        setAccessibleName(name);
        setFocusPolicy(Qt::NoFocus);
    }

    void makeClickable(const QString& toolTip) {
        clickable = true;
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
        setToolTip(toolTip);
    }

    double ratio = 0.0;                              // Bar length against the largest; none at 0 or below
    QString categoryColor;                           // The bar's; empty = neutral
    bool showsUsage = false;                         // The bar is part of a limit: on a track, red past it
    int direction = 0;                               // For Tone::Change: 1 bad (red), -1 good (green)

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (kind != Kind::Heading) {
            painter.setPen(colors.sidebarBorder);
            painter.drawLine(QPointF(0, 0.5), QPointF(width(), 0.5));
        }
        if (clickable && (underMouse() || hasFocus())) {
            QPainterPath hover;
            hover.addRoundedRect(QRectF(rect()).adjusted(0, 1, 0, 0), 6, 6);
            painter.fillPath(hover, colors.itemHover);
        }
        QFont textFont = font();
        if (kind == Kind::Total) textFont.setWeight(QFont::DemiBold);
        if (kind == Kind::Heading) textFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(textFont);
        const QFontMetrics metrics(textFont);
        const QColor normal = kind == Kind::Heading ? colors.textSecondary : colors.textPrimary;

        // The numbers, right-aligned in their columns, last one rightmost
        int right = width();
        for (auto it = columns.crbegin(); it != columns.crend(); ++it) {
            QColor color = normal;
            if (kind != Kind::Heading) {
                switch (it->tone) {
                case Tone::Normal: break;
                case Tone::Secondary: color = colors.textSecondary; break;
                case Tone::Bad: color = colors.danger; break;
                case Tone::Change: color = changeColor(direction); break;
                }
            }
            painter.setPen(color);
            painter.drawText(QRect(right - it->width, 0, it->width, height()), Qt::AlignRight | Qt::AlignVCenter, it->text);
            right -= it->width;
        }

        // The name, above its bar if it has one
        const int nameWidth = qMax(0, right - 24);
        const QString shownName = metrics.elidedText(name, Qt::ElideRight, nameWidth);
        painter.setPen(normal);
        if (!hasBar) {
            painter.drawText(QRect(0, 0, nameWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, shownName);
            return;
        }
        painter.drawText(QRect(0, 4, nameWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, shownName);
        if (showsUsage) {
            QPainterPath track;
            track.addRoundedRect(QRectF(0, 26, nameWidth, 10), 3, 3);
            painter.fillPath(track, colors.track);
        }
        if (ratio > 0.0) {
            QPainterPath bar;
            bar.addRoundedRect(QRectF(0, 26, qMax(4.0, nameWidth * qMin(ratio, 1.0)), 10), 3, 3);
            painter.fillPath(bar, showsUsage ? (ratio > 1.0 ? colors.danger : colors.accent) : barColor(categoryColor));
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (clickable && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

private:
    Kind kind;
    QString name;
    QList<Column> columns;
    bool hasBar = false;
    bool clickable = false;
};

} // namespace

// Income and spending bars per week or month; hovering one shows its numbers. Compact is the
// card's preview: no legend, labels or hovering.
class FlowChart : public QWidget {
public:
    FlowChart(bool compactChart, QWidget* parent) : QWidget(parent), compact(compactChart) {
        setObjectName(compact ? "flowPreview" : "flowChart");
        setMouseTracking(!compact);
        if (compact) {
            setFixedHeight(40);
        } else {
            setMinimumHeight(200);
        }
    }

    void setReport(const FlowReport& newReport) {
        report = newReport;
        hovered = -1;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        Cents highest = 0;
        for (const PeriodFlow& flow : std::as_const(report.periods)) {
            highest = qMax(highest, qMax(flow.incomeCents, flow.spendingCents));
        }
        if (highest <= 0) {
            if (!compact) {
                painter.setPen(colors.textSecondary);
                painter.drawText(rect(), Qt::AlignCenter, "No income or spending in this range.");
            }
            return;
        }

        const QRect plot = plotRect();
        const int barWidth = qBound(compact ? 3 : 6, groupWidth() / 4, compact ? 10 : 22);
        const QColor spendingColor = Settings::instance().isExpensesInRed() ? colors.danger : colors.textMuted;
        QFont labelFont = font();
        labelFont.setPointSizeF(font().pointSizeF() * 0.9);
        const QFontMetrics labelMetrics(labelFont);
        // Leave out labels that wouldn't fit, evenly
        const int labelWidth = labelMetrics.horizontalAdvance("30 Sep") + 8;
        const int labelEvery = qMax(1, (labelWidth + groupWidth() - 1) / qMax(1, groupWidth()));
        for (int i = 0; i < report.periods.size(); ++i) {
            const PeriodFlow& flow = report.periods[i];
            const QRect group = groupRect(i);
            if (i == hovered) {
                QPainterPath path;
                path.addRoundedRect(QRectF(group.adjusted(2, -4, -2, 22)), 6, 6);
                painter.fillPath(path, colors.itemHover);
            }
            int x = group.center().x() - barWidth - 1;
            for (const auto& [cents, color] : QList<QPair<Cents, QColor>>{{flow.incomeCents, colors.positive},
                                                                          {flow.spendingCents, spendingColor}}) {
                const double barHeight = double(cents) / double(highest) * plot.height();
                if (barHeight > 0) {
                    const double shown = qMax(barHeight, 2.0);
                    QPainterPath path;
                    path.addRoundedRect(QRectF(x, plot.bottom() + 1 - shown, barWidth, shown + 3), 2, 2);
                    painter.save();
                    painter.setClipRect(QRectF(x, plot.top(), barWidth, plot.height() + 1));
                    painter.fillPath(path, color);
                    painter.restore();
                }
                x += barWidth + 2;
            }
            if (!compact && i % labelEvery == 0) {
                painter.setFont(labelFont);
                painter.setPen(i == hovered ? colors.textPrimary : colors.textSecondary);
                painter.drawText(QRect(group.center().x() - labelWidth / 2, plot.bottom() + 4, labelWidth, 18), Qt::AlignCenter,
                                 periodName(flow.period));
            }
        }
        if (compact) return;
        painter.setPen(colors.sidebarBorder);
        painter.drawLine(plot.bottomLeft() + QPoint(0, 1), plot.bottomRight() + QPoint(0, 1));

        // Legend in the top right corner
        painter.setFont(labelFont);
        int right = rect().right();
        for (const auto& [label, color] : QList<QPair<QString, QColor>>{{"Spending", spendingColor}, {"Income", colors.positive}}) {
            const int textWidth = labelMetrics.horizontalAdvance(label);
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(right - textWidth, 0, textWidth, 16), Qt::AlignRight | Qt::AlignVCenter, label);
            QPainterPath swatch;
            swatch.addRoundedRect(QRectF(right - textWidth - 14, 4, 9, 9), 2, 2);
            painter.fillPath(swatch, color);
            right -= textWidth + 30;
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        int index = -1;
        for (int i = 0; i < report.periods.size(); ++i) {
            if (groupRect(i).adjusted(0, -8, 0, 24).contains(event->position().toPoint())) index = i;
        }
        if (index != hovered) {
            hovered = index;
            update();
        }
        if (index >= 0) {
            const Settings& settings = Settings::instance();
            const PeriodFlow& flow = report.periods[index];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1\nIncome %2\nSpending %3\nNet %4")
                                   .arg(periodTitle(flow.period, report.range), settings.formatCurrency(flow.incomeCents),
                                        settings.formatSpending(flow.spendingCents), settings.formatCurrency(flow.netCents(), true)),
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
    // "Oct" for months, "W40" (the calendar week) for weeks
    QString periodName(const ReportRange& period) const {
        if (report.range.flowUnit() == ReportRange::Unit::Week) return QString("W%1").arg(period.first.weekNumber());
        return english.toString(period.first, "MMM");
    }
    QRect plotRect() const { return compact ? rect() : rect().adjusted(0, 24, 0, -24); }   // Legend above, labels below
    int groupWidth() const { return report.periods.isEmpty() ? 0 : plotRect().width() / int(report.periods.size()); }
    QRect groupRect(int index) const {
        const QRect plot = plotRect();
        return QRect(plot.left() + index * groupWidth(), plot.top(), groupWidth(), plot.height());
    }

    bool compact = false;
    FlowReport report;
    int hovered = -1;
};

// The balance as a line over the whole range (so a running range stops at today); hovering
// shows the day's balance. Compact is the card's preview: just the line.
class BalanceChart : public QWidget {
public:
    BalanceChart(bool compactChart, QWidget* parent) : QWidget(parent), compact(compactChart) {
        setObjectName(compact ? "balancePreview" : "balanceChart");
        setMouseTracking(!compact);
        if (compact) {
            setFixedHeight(40);
        } else {
            setMinimumHeight(220);
        }
    }

    void setReport(const BalanceReport& newReport) {
        report = newReport;
        low = high = report.days.isEmpty() ? 0 : report.days.first().balanceCents;
        for (const BalancePoint& point : std::as_const(report.days)) {
            low = qMin(low, point.balanceCents);
            high = qMax(high, point.balanceCents);
        }
        hovered = -1;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (report.days.isEmpty()) return;

        const QRect plot = plotRect();
        QFont labelFont = font();
        labelFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(labelFont);

        // Lines across with amounts, and zero when the balance goes below it
        if (!compact) {
            QList<Cents> marks = {highest(), (highest() + lowest()) / 2, lowest()};
            if (lowest() < 0 && highest() > 0) marks = {highest(), 0, lowest()};
            for (const Cents mark : std::as_const(marks)) {
                const double y = yFor(mark);
                painter.setPen(QPen(mark == 0 ? colors.textMuted : colors.sidebarBorder, 1));
                painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
                painter.setPen(colors.textSecondary);
                painter.drawText(QRect(0, int(y) - 9, plot.left() - 8, 18), Qt::AlignRight | Qt::AlignVCenter,
                                 Settings::instance().formatCurrency(mark, false, 0));
            }
            // Where each week or month starts
            painter.setPen(colors.textSecondary);
            const QList<ReportRange> periods = report.range.periods(report.range.flowUnit());
            const QFontMetrics metrics(labelFont);
            int nextFree = plot.left();
            for (const ReportRange& period : periods) {
                const QString label = report.range.flowUnit() == ReportRange::Unit::Week ? QString("W%1").arg(period.first.weekNumber())
                                                                                        : english.toString(period.first, "MMM");
                const int x = int(xFor(period.first));
                if (x < nextFree) continue;
                painter.drawText(QRect(x, plot.bottom() + 6, metrics.horizontalAdvance(label) + 2, 18), Qt::AlignLeft | Qt::AlignVCenter, label);
                nextFree = x + metrics.horizontalAdvance(label) + 12;
            }
        }

        // The line, with a light fill below
        QPainterPath line;
        for (int i = 0; i < report.days.size(); ++i) {
            const QPointF point(xFor(report.days[i].date) + dayWidth() / 2, yFor(report.days[i].balanceCents));
            if (i == 0) {
                line.moveTo(point);
            } else {
                line.lineTo(point);
            }
        }
        QPainterPath area = line;
        area.lineTo(line.currentPosition().x(), plot.bottom());
        area.lineTo(line.elementAt(0).x, plot.bottom());
        area.closeSubpath();
        QColor fill = colors.accent;
        fill.setAlpha(30);
        painter.fillPath(area, fill);
        painter.setPen(QPen(colors.accent, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(line);

        if (hovered >= 0) {
            const BalancePoint& point = report.days[hovered];
            const QPointF dot(xFor(point.date) + dayWidth() / 2, yFor(point.balanceCents));
            painter.setPen(QPen(colors.textMuted, 1, Qt::DashLine));
            painter.drawLine(QPointF(dot.x(), plot.top()), QPointF(dot.x(), plot.bottom()));
            painter.setPen(Qt::NoPen);
            painter.setBrush(colors.accent);
            painter.drawEllipse(dot, 4, 4);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        int index = -1;
        const QRect plot = plotRect();
        if (!report.days.isEmpty() && event->position().x() >= plot.left() - 4 && event->position().x() <= plot.right() + 4) {
            const int day = int((event->position().x() - plot.left()) / dayWidth());
            index = qBound(0, day, int(report.days.size()) - 1);
        }
        if (index != hovered) {
            hovered = index;
            update();
        }
        if (index >= 0) {
            const BalancePoint& point = report.days[index];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1\n%2").arg(english.toString(point.date, "d MMMM yyyy"),
                                                     Settings::instance().formatCurrency(point.balanceCents)),
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
    // Amounts on the left, period names below
    QRect plotRect() const { return compact ? rect().adjusted(1, 2, -1, -2) : rect().adjusted(84, 10, -4, -26); }
    double dayWidth() const { return double(plotRect().width()) / double(qMax<qint64>(1, report.range.days())); }
    double xFor(const QDate& date) const { return plotRect().left() + report.range.first.daysTo(date) * dayWidth(); }
    Cents highest() const { return high; }
    Cents lowest() const { return low; }
    double yFor(Cents cents) const {
        const QRect plot = plotRect();
        if (high == low) return plot.center().y();
        return plot.bottom() - double(cents - low) / double(high - low) * plot.height();
    }

    bool compact = false;
    BalanceReport report;
    Cents low = 0;
    Cents high = 0;
    int hovered = -1;
};

// One category's spending as a bar per week or month, with its average as a dashed line.
// Hovering a bar shows its numbers and clicking it calls periodClicked. Compact is the card's
// preview: just the bars.
class TrendChart : public QWidget {
public:
    TrendChart(bool compactChart, QWidget* parent) : QWidget(parent), compact(compactChart) {
        setObjectName(compact ? "trendPreview" : "trendChart");
        setMouseTracking(!compact);
        if (compact) {
            setFixedHeight(40);
        } else {
            setMinimumHeight(200);
        }
    }

    std::function<void(int index)> periodClicked;

    void setTrend(const CategoryTrend& newTrend) {
        trend = newTrend;
        hovered = -1;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const Cents highest = highestCents();
        if (highest <= 0) {
            if (!compact) {
                painter.setPen(colors.textSecondary);
                painter.drawText(rect(), Qt::AlignCenter, QString("Nothing spent on %1 in this range.").arg(trend.categoryName));
            }
            return;
        }

        const QRect plot = plotRect();
        const int barWidth = qBound(compact ? 3 : 8, groupWidth() / 2, compact ? 12 : 36);
        QFont labelFont = font();
        labelFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(labelFont);
        const QFontMetrics labelMetrics(labelFont);
        const int labelWidth = labelMetrics.horizontalAdvance("W53") + 12;
        const int labelEvery = qMax(1, (labelWidth + groupWidth() - 1) / qMax(1, groupWidth()));
        const QColor color = barColor(trend.categoryColor);
        for (int i = 0; i < trend.periods.size(); ++i) {
            const QRect group = groupRect(i);
            if (i == hovered) {
                QPainterPath path;
                path.addRoundedRect(QRectF(group.adjusted(2, -4, -2, 22)), 6, 6);
                painter.fillPath(path, colors.itemHover);
            }
            const double barHeight = double(trend.periods[i].spentCents) / double(highest) * plot.height();
            if (barHeight > 0) {
                const double shown = qMax(barHeight, 2.0);
                QPainterPath path;
                path.addRoundedRect(QRectF(group.center().x() - barWidth / 2.0, plot.bottom() + 1 - shown, barWidth, shown + 3), 3, 3);
                painter.save();
                painter.setClipRect(QRectF(group.left(), plot.top(), group.width(), plot.height() + 1));
                painter.fillPath(path, color);
                painter.restore();
            }
            if (!compact && i % labelEvery == 0) {
                painter.setPen(i == hovered ? colors.textPrimary : colors.textSecondary);
                const ReportRange& period = trend.periods[i].period;
                const QString label = trend.range.flowUnit() == ReportRange::Unit::Week ? QString("W%1").arg(period.first.weekNumber())
                                                                                       : english.toString(period.first, "MMM");
                painter.drawText(QRect(group.center().x() - labelWidth / 2, plot.bottom() + 4, labelWidth, 18), Qt::AlignCenter, label);
            }
        }
        if (compact) return;
        painter.setPen(colors.sidebarBorder);
        painter.drawLine(plot.bottomLeft() + QPoint(0, 1), plot.bottomRight() + QPoint(0, 1));

        // The average across, labelled in the top right corner
        if (trend.averageCents > 0) {
            const double y = plot.bottom() - double(trend.averageCents) / double(highest) * plot.height();
            painter.setPen(QPen(colors.textSecondary, 1, Qt::DashLine));
            painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
            const QString text = QString("Average %1").arg(Settings::instance().formatCurrency(trend.averageCents));
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(rect().right() - labelMetrics.horizontalAdvance(text), 0, labelMetrics.horizontalAdvance(text), 16),
                             Qt::AlignRight | Qt::AlignVCenter, text);
        }
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const int index = indexAt(event->position().toPoint());
        if (index != hovered) {
            hovered = index;
            setCursor(index >= 0 && periodClicked ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
        if (index >= 0) {
            const PeriodSpent& spent = trend.periods[index];
            QToolTip::showText(event->globalPosition().toPoint(),
                               QString("%1\n%2 · %3 %4%5")
                                   .arg(periodTitle(spent.period, trend.range), Settings::instance().formatSpending(spent.spentCents))
                                   .arg(spent.count)
                                   .arg(spent.count == 1 ? "transaction" : "transactions",
                                        periodClicked ? QString("\nClick to show them") : QString()),
                               this);
        } else {
            QToolTip::hideText();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        const int index = indexAt(event->position().toPoint());
        if (event->button() == Qt::LeftButton && index >= 0 && periodClicked) periodClicked(index);
    }

    void leaveEvent(QEvent* event) override {
        hovered = -1;
        update();
        QWidget::leaveEvent(event);
    }

private:
    Cents highestCents() const {
        Cents highest = 0;
        for (const PeriodSpent& spent : trend.periods) highest = qMax(highest, spent.spentCents);
        return highest;
    }
    int indexAt(const QPoint& position) const {
        for (int i = 0; i < trend.periods.size(); ++i) {
            if (groupRect(i).adjusted(0, -8, 0, 24).contains(position)) return i;
        }
        return -1;
    }
    QRect plotRect() const { return compact ? rect() : rect().adjusted(0, 24, 0, -24); }   // Average above, labels below
    int groupWidth() const { return trend.periods.isEmpty() ? 0 : plotRect().width() / int(trend.periods.size()); }
    QRect groupRect(int index) const {
        const QRect plot = plotRect();
        return QRect(plot.left() + index * groupWidth(), plot.top(), groupWidth(), plot.height());
    }

    bool compact = false;
    CategoryTrend trend;
    int hovered = -1;
};

// Budget vs. actual over the budgets together. A single month: the spending added up day by day
// against an even pace to the month's limit (dashed), so mid-month shows whether it's on track.
// Longer: a bar per month with a mark at its limit, red when over.
class BudgetChart : public QWidget {
public:
    explicit BudgetChart(QWidget* parent) : QWidget(parent) {
        setObjectName("budgetChart");
        setMinimumHeight(200);
    }

    void setReport(const BudgetReport& newReport) {
        report = newReport;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QFont labelFont = font();
        labelFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(labelFont);
        if (report.months.size() == 1) {
            paintPace(painter);
        } else {
            paintMonths(painter);
        }
    }

private:
    void paintPace(QPainter& painter) const {
        const Theme::Colors colors = Theme::colors();
        const Settings& settings = Settings::instance();
        const BudgetMonth& month = report.months.first();
        const int days = int(month.month.days());
        if (month.limitCents <= 0 || days < 2) return;
        const Cents soFar = report.spentByDay.isEmpty() ? 0 : report.spentByDay.last();
        const double highest = double(qMax(month.limitCents, soFar));
        const QFontMetrics metrics(painter.font());
        const QString limitText = settings.formatCurrency(month.limitCents, false, 0);
        const QRect plot = rect().adjusted(metrics.horizontalAdvance(limitText) + 8, 8, -8, -24);
        const auto x = [&](int day) { return plot.left() + double(day) / (days - 1) * plot.width(); };
        const auto y = [&](Cents cents) { return plot.bottom() - double(cents) / highest * plot.height(); };

        // The limit, the ground and the even pace between them
        painter.setPen(colors.sidebarBorder);
        painter.drawLine(QPointF(plot.left(), y(month.limitCents)), QPointF(plot.right(), y(month.limitCents)));
        painter.drawLine(plot.bottomLeft(), plot.bottomRight());
        painter.setPen(QPen(colors.textSecondary, 1, Qt::DashLine));
        painter.drawLine(QPointF(x(0), y(0)), QPointF(x(days - 1), y(month.limitCents)));
        painter.setPen(colors.textSecondary);
        painter.drawText(QRect(0, int(y(month.limitCents)) - 8, plot.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, limitText);
        for (const int day : {0, days / 2, days - 1}) {
            const QString label = day == 0 ? english.toString(month.month.first, "d MMM") : QString::number(day + 1);
            painter.drawText(QRect(int(x(day)) - 40, plot.bottom() + 4, 80, 18), Qt::AlignCenter, label);
        }

        // The spending so far, red once past the limit
        if (report.spentByDay.isEmpty()) return;
        QPolygonF line;
        for (int day = 0; day < report.spentByDay.size(); ++day) line << QPointF(x(day), y(report.spentByDay[day]));
        const QColor color = soFar > month.limitCents ? colors.danger : colors.accent;
        painter.setPen(QPen(color, 2));
        painter.drawPolyline(line);
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        painter.drawEllipse(line.last(), 3.5, 3.5);
    }

    void paintMonths(QPainter& painter) const {
        const Theme::Colors colors = Theme::colors();
        Cents highest = 0;
        for (const BudgetMonth& month : report.months) highest = qMax(highest, qMax(month.limitCents, month.spentCents));
        if (highest <= 0 || report.months.isEmpty()) return;
        const QRect plot = rect().adjusted(0, 8, 0, -24);
        const int groupWidth = plot.width() / int(report.months.size());
        const int barWidth = qBound(8, groupWidth / 2, 36);
        const auto y = [&](Cents cents) { return plot.bottom() + 1 - double(cents) / double(highest) * plot.height(); };
        for (int i = 0; i < report.months.size(); ++i) {
            const BudgetMonth& month = report.months[i];
            const double center = plot.left() + (i + 0.5) * groupWidth;
            if (month.spentCents > 0) {
                QPainterPath bar;
                const double top = qMin(y(month.spentCents), plot.bottom() - 1.0);
                bar.addRoundedRect(QRectF(center - barWidth / 2.0, top, barWidth, plot.bottom() + 1 - top + 3), 3, 3);
                painter.save();
                painter.setClipRect(QRectF(plot.left(), plot.top(), plot.width(), plot.height() + 1));
                painter.fillPath(bar, month.spentCents > month.limitCents && month.limitCents > 0 ? colors.danger : colors.accent);
                painter.restore();
            }
            if (month.limitCents > 0) {                  // The limit as a mark across the bar
                painter.setPen(QPen(colors.textPrimary, 2));
                painter.drawLine(QPointF(center - barWidth * 0.8, y(month.limitCents)), QPointF(center + barWidth * 0.8, y(month.limitCents)));
            }
            painter.setPen(colors.textSecondary);
            painter.drawText(QRect(int(center) - 40, plot.bottom() + 4, 80, 18), Qt::AlignCenter, english.toString(month.month.first, "MMM"));
        }
        painter.setPen(colors.sidebarBorder);
        painter.drawLine(plot.bottomLeft() + QPoint(0, 1), plot.bottomRight() + QPoint(0, 1));
    }

    BudgetReport report;
};

// The money flow as a Sankey diagram: income (and money from savings or back) on the left, all of
// it in the middle, spending and what was kept on the right, each band as wide as its money.
// Hovering a band shows its numbers and clicking a category's calls partClicked. Compact is the
// card's preview: just the bands.
class SankeyChart : public QWidget {
public:
    SankeyChart(bool compactChart, QWidget* parent) : QWidget(parent), compact(compactChart) {
        setObjectName(compact ? "moneyFlowPreview" : "moneyFlowChart");
        setMouseTracking(!compact);
        if (compact) setFixedHeight(40);
    }

    std::function<void(int categoryId)> partClicked;

    void setFlow(const MoneyFlow& newFlow) {
        flow = newFlow;
        hovered = -1;
        if (!compact) setMinimumHeight(qMax(240, int(qMax(flow.income.size() + 1, flow.spending.size() + 1)) * 30 + 40));
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QList<Node> all = nodes();
        if (all.isEmpty()) {
            if (!compact) {
                painter.setPen(colors.textSecondary);
                painter.drawText(rect(), Qt::AlignCenter, "No income or spending in this range.");
            }
            return;
        }
        for (int i = 0; i < all.size(); ++i) {
            QColor band = all[i].color;
            band.setAlphaF(i == hovered ? 0.6 : 0.3);
            painter.fillPath(all[i].band, band);
        }
        const QRectF middle = middleRect();
        painter.fillRect(middle, colors.textSecondary);
        for (const Node& node : all) painter.fillRect(node.box, node.color);
        if (compact) return;

        const Settings& settings = Settings::instance();
        QFont labelFont = font();
        labelFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(labelFont);
        const QFontMetrics metrics(labelFont);
        for (int i = 0; i < all.size(); ++i) {
            const Node& node = all[i];
            const QString text = metrics.elidedText(QString("%1 · %2").arg(node.name, settings.formatCurrency(node.cents, false, 0)),
                                                    Qt::ElideRight, labelWidth() - 8);
            painter.setPen(i == hovered || node.isKept ? colors.textPrimary : colors.textSecondary);
            const QRectF area = node.left ? QRectF(0, node.box.top(), node.box.left() - 8, node.box.height())
                                          : QRectF(node.box.right() + 8, node.box.top(), labelWidth(), node.box.height());
            painter.drawText(area.adjusted(0, -10, 0, 10), (node.left ? Qt::AlignRight : Qt::AlignLeft) | Qt::AlignVCenter, text);
        }
        const bool onlyIncome = flow.fromSavingsCents() == 0 && std::none_of(flow.income.cbegin(), flow.income.cend(),
                                                                              [](const FlowPart& part) { return part.isMoneyBack; });
        painter.setPen(colors.textPrimary);
        painter.drawText(QRectF(middle.center().x() - 150, middle.bottom() + 4, 300, 18), Qt::AlignCenter,
                         QString("%1 %2").arg(onlyIncome ? "Income" : "Total", settings.formatCurrency(flow.totalCents(), false, 0)));
    }

    void mouseMoveEvent(QMouseEvent* event) override {
        const QList<Node> all = nodes();
        int index = -1;
        for (int i = 0; i < all.size(); ++i) {
            if (all[i].band.contains(event->position()) || all[i].box.adjusted(-2, -2, 2, 2).contains(event->position())) index = i;
        }
        if (index != hovered) {
            hovered = index;
            setCursor(index >= 0 && all[index].categoryId && partClicked ? Qt::PointingHandCursor : Qt::ArrowCursor);
            update();
        }
        if (index < 0) {
            QToolTip::hideText();
            return;
        }
        const Node& node = all[index];
        const Settings& settings = Settings::instance();
        QString text = QString("%1\n%2").arg(node.name, settings.formatCurrency(node.cents));
        if (flow.incomeCents > 0) text += QString(", %1 of the income").arg(shareText(node.cents, flow.incomeCents));
        if (node.categoryId && partClicked) text += "\nClick to show its transactions";
        QToolTip::showText(event->globalPosition().toPoint(), text, this);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (event->button() != Qt::LeftButton || hovered < 0 || !partClicked) return;
        const QList<Node> all = nodes();
        if (hovered < all.size() && all[hovered].categoryId) partClicked(*all[hovered].categoryId);
    }

    void leaveEvent(QEvent* event) override {
        hovered = -1;
        update();
        QWidget::leaveEvent(event);
    }

private:
    struct Node {
        QString name;
        Cents cents = 0;
        QColor color;
        std::optional<int> categoryId;               // None for what isn't one category
        bool left = false;
        bool isKept = false;
        QRectF box;
        QPainterPath band;                           // To or from the middle
    };

    static constexpr double NodeWidth = 10;
    static constexpr double Gap = 8;

    int labelWidth() const { return compact ? 0 : qMin(220, width() * 3 / 10); }
    QRectF plotRect() const { return compact ? QRectF(rect()) : QRectF(rect()).adjusted(0, 8, 0, -28); }   // Middle's label below
    QRectF middleRect() const {
        const QRectF plot = plotRect();
        const double height = flow.totalCents() > 0 ? scale() * double(flow.totalCents()) : 0;
        return QRectF(plot.center().x() - NodeWidth / 2, plot.center().y() - height / 2, NodeWidth, height);
    }
    // Pixels per cent, so the side with more parts fits with its gaps
    double scale() const {
        const int most = int(qMax(flow.income.size() + 1, flow.spending.size() + 1));
        const double gap = compact ? 2 : Gap;
        return qMax(0.0, plotRect().height() - gap * (most - 1)) / double(qMax<Cents>(1, flow.totalCents()));
    }

    // The parts in drawing order, with their boxes and bands
    QList<Node> nodes() const {
        if (flow.totalCents() <= 0) return {};
        const Theme::Colors colors = Theme::colors();
        QList<Node> left;
        for (const FlowPart& part : flow.income) {
            left.append({part.name, part.cents, part.isMoneyBack ? barColor(part.color) : colors.positive, part.categoryId, true});
            if (part.isMoneyBack) left.last().name = QString("Back from %1").arg(part.name);
        }
        if (flow.fromSavingsCents() > 0) left.append({"From savings", flow.fromSavingsCents(), colors.textMuted, std::nullopt, true});
        QList<Node> right;
        for (const FlowPart& part : flow.spending) {
            right.append({part.name, part.cents, part.isRest ? colors.textMuted : barColor(part.color),
                          part.isRest ? std::nullopt : std::optional<int>(part.categoryId), false});
        }
        if (flow.keptCents() > 0) {
            right.append({"Kept", flow.keptCents(), colors.accent, std::nullopt, false, true});
        }

        const QRectF plot = plotRect();
        const QRectF middle = middleRect();
        const double perCent = scale();
        const double gap = compact ? 2 : Gap;
        const double leftX = plot.left() + labelWidth();
        const double rightX = plot.right() - labelWidth() - NodeWidth;
        for (QList<Node>* side : {&left, &right}) {
            double height = gap * (side->size() - 1);
            for (const Node& node : std::as_const(*side)) height += perCent * double(node.cents);
            double y = plot.center().y() - height / 2;
            double middleY = middle.top();                   // Where its band meets the middle
            const bool isLeft = side == &left;
            for (Node& node : *side) {
                const double h = perCent * double(node.cents);
                node.box = QRectF(isLeft ? leftX : rightX, y, NodeWidth, qMax(1.0, h));
                const double from = isLeft ? node.box.right() : middle.right();
                const double to = isLeft ? middle.left() : node.box.left();
                const double fromTop = isLeft ? y : middleY;
                const double toTop = isLeft ? middleY : y;
                const double bend = (to - from) / 2;
                node.band.moveTo(from, fromTop);
                node.band.cubicTo(from + bend, fromTop, to - bend, toTop, to, toTop);
                node.band.lineTo(to, toTop + h);
                node.band.cubicTo(to - bend, toTop + h, from + bend, fromTop + h, from, fromTop + h);
                node.band.closeSubpath();
                y += h + gap;
                middleY += h;
            }
        }
        return left + right;
    }

    bool compact = false;
    MoneyFlow flow;
    int hovered = -1;
};

namespace {

bool askWithDialog(QWidget* parent, QDate& from, QDate& to) {
    QDialog dialog(parent);
    dialog.setWindowTitle("Custom range");
    auto* form = new QFormLayout(&dialog);
    auto* fromEdit = new QDateEdit(from, &dialog);
    auto* toEdit = new QDateEdit(to, &dialog);
    for (QDateEdit* edit : {fromEdit, toEdit}) {
        edit->setCalendarPopup(true);
        edit->setDisplayFormat("d MMM yyyy");
    }
    form->addRow("From", fromEdit);
    form->addRow("To", toEdit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    form->addRow(buttons);
    if (dialog.exec() != QDialog::Accepted) return false;
    from = fromEdit->date();
    to = toEdit->date();
    return true;
}

// One or more lines that take any width they're given, so a long payee or category name never
// widens its card: a line that doesn't fit ends in "…". text() stays the whole text.
class ElidedLabel : public QLabel {
public:
    explicit ElidedLabel(QWidget* parent) : QLabel(parent) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    }

    QSize minimumSizeHint() const override { return {0, QLabel::minimumSizeHint().height()}; }

    bool isElided() const {
        const QStringList lines = text().split('\n');
        return std::any_of(lines.cbegin(), lines.cend(),
                           [this](const QString& line) { return fontMetrics().horizontalAdvance(line) > contentsRect().width(); });
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        const QRect area = contentsRect();
        const int lineHeight = fontMetrics().lineSpacing();
        int y = area.top();
        for (const QString& line : text().split('\n')) {
            painter.drawText(QRect(area.left(), y, area.width(), lineHeight), Qt::AlignLeft | Qt::AlignVCenter,
                             fontMetrics().elidedText(line, Qt::ElideRight, area.width()));
            y += lineHeight;
        }
    }
};

} // namespace

// A report in the overview: its title, the number that matters most, a line about it, and a
// small preview below. Clicking it opens the report.
class ReportCard : public QAbstractButton {
public:
    explicit ReportCard(const QString& title, QWidget* parent) : QAbstractButton(parent) {
        setObjectName("reportCard");
        setAccessibleName(title);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
        setMinimumHeight(140);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(14, 12, 14, 12);
        layout->setSpacing(2);
        titleLabel = new QLabel(title, this);
        titleLabel->setObjectName("reportCardTitle");
        titleLabel->setForegroundRole(QPalette::PlaceholderText);
        valueLabel = new ElidedLabel(this);
        valueLabel->setObjectName("reportCardValue");
        QFont valueFont = valueLabel->font();
        valueFont.setPointSize(15);
        valueLabel->setFont(valueFont);
        detailLabel = new QLabel(this);
        detailLabel->setObjectName("reportCardDetail");
        detailLabel->setWordWrap(true);
        previewArea = new QWidget(this);
        previewArea->setObjectName("reportCardPreview");
        previewArea->setMinimumHeight(44);
        new QVBoxLayout(previewArea);
        previewArea->layout()->setContentsMargins(0, 6, 0, 0);
        layout->addWidget(titleLabel);
        layout->addWidget(valueLabel);
        layout->addWidget(detailLabel);
        layout->addWidget(previewArea, 1);
        // Clicks anywhere on the card are the card's
        for (QWidget* child : {static_cast<QWidget*>(titleLabel), static_cast<QWidget*>(valueLabel),
                               static_cast<QWidget*>(detailLabel), previewArea}) {
            child->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
        setValue(QString(), "Not built yet");
    }

    void setValue(const QString& value, const QString& detail, const QColor& detailColor = QColor()) {
        valueLabel->setText(value);
        valueLabel->setVisible(!value.isEmpty());
        detailLabel->setText(detail);
        if (detailColor.isValid()) {
            setTextColor(detailLabel, detailColor);
        } else {
            detailLabel->setPalette(QPalette());     // Back to the page's, so it follows the theme
            detailLabel->setForegroundRole(QPalette::PlaceholderText);
        }
        setAccessibleDescription(value.isEmpty() ? detail : QString("%1, %2").arg(value, detail));
    }

    QWidget* preview() const { return previewArea; }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        // A card like the others (Theme::makeCard), lighter while the mouse is over it
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), Theme::cardRadius, Theme::cardRadius);
        painter.fillPath(path, Theme::cardBackground(Theme::isDark()));
        if (underMouse()) painter.fillPath(path, colors.itemHover);
        painter.setPen(QPen(hasFocus() ? colors.accent : Theme::hairline(Theme::isDark()), hasFocus() ? 2 : 1));
        painter.drawPath(path);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

    // What was cut short, in full
    bool event(QEvent* event) override {
        if (event->type() == QEvent::ToolTip) {
            QStringList full;
            for (const QLabel* child : findChildren<QLabel*>()) {
                const auto* label = dynamic_cast<const ElidedLabel*>(child);
                if (label && label->isVisible() && label->isElided()) full.append(label->text());
            }
            if (!full.isEmpty()) {
                QToolTip::showText(static_cast<QHelpEvent*>(event)->globalPos(), full.join('\n'), this);
                return true;
            }
        }
        return QAbstractButton::event(event);
    }

private:
    QLabel* titleLabel = nullptr;
    ElidedLabel* valueLabel = nullptr;
    QLabel* detailLabel = nullptr;
    QWidget* previewArea = nullptr;
};

// How full all budgets are together, for the budget card
class UsageBar : public QWidget {
public:
    explicit UsageBar(QWidget* parent) : QWidget(parent) {
        setObjectName("usageBar");
        setFixedHeight(8);
    }

    void setRatio(double newRatio) {
        ratio = newRatio;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath track;
        track.addRoundedRect(QRectF(rect()), 4, 4);
        painter.fillPath(track, colors.track);
        if (ratio > 0.0) {
            QPainterPath bar;
            bar.addRoundedRect(QRectF(0, 0, qMax(8.0, width() * qMin(ratio, 1.0)), height()), 4, 4);
            painter.fillPath(bar, ratio > 1.0 ? colors.danger : colors.accent);
        }
    }

private:
    double ratio = 0.0;
};

// The largest categories as thin bars, for the spending card
class CategoryBars : public QWidget {
public:
    explicit CategoryBars(QWidget* parent) : QWidget(parent) {
        setObjectName("categoryBars");
        setFixedHeight(4 * 6 + 3 * 4);
    }

    void setCategories(const QList<CategorySpent>& categories) {
        shown = categories.mid(0, 4);
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        if (shown.isEmpty()) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const double largest = double(qMax<Cents>(1, shown.first().spentCents));
        double y = 0;
        for (const CategorySpent& spent : std::as_const(shown)) {
            if (spent.spentCents > 0) {
                QPainterPath bar;
                bar.addRoundedRect(QRectF(0, y, qMax(6.0, width() * 0.9 * double(spent.spentCents) / largest), 6), 3, 3);
                painter.fillPath(bar, barColor(spent.categoryColor));
            }
            y += 10;
        }
    }

private:
    QList<CategorySpent> shown;
};

ReportsPage::ReportsPage(QWidget* parent)
    : QWidget(parent) {
    askCustomRange = [this](QDate& from, QDate& to) { return askWithDialog(this, from, to); };

    // Everything sits in one column: the full width, or centred (Settings, Appearance; updateWidth)
    column = new QWidget(this);
    column->setObjectName("reportsColumn");
    auto* root = new QVBoxLayout(column);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(12);

    // Title (or the way back), the range, and the choices that apply to every report
    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    backLink = new QToolButton(this);
    backLink->setObjectName("reportsBackLink");
    backLink->setText("Reports");
    backLink->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    backLink->setAutoRaise(true);
    backLink->setCursor(Qt::PointingHandCursor);
    connect(backLink, &QToolButton::clicked, this, &ReportsPage::showOverview);
    titles->addWidget(backLink, 0, Qt::AlignLeft);
    titleLabel = new QLabel("Reports", this);
    titleLabel->setObjectName("reportsTitle");
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleLabel->setFont(titleFont);
    titles->addWidget(titleLabel);
    rangeLabel = new QLabel(this);
    rangeLabel->setObjectName("reportsRangeLabel");
    rangeLabel->setForegroundRole(QPalette::PlaceholderText);
    titles->addWidget(rangeLabel);
    header->addLayout(titles, 1);

    // The range's length, the stepper through ranges of that length, the account and the width, in
    // the theme's control look
    headerControls = new QWidget(this);
    headerControls->setObjectName("reportsHeaderControls");
    auto* choices = new QHBoxLayout(headerControls);
    choices->setContentsMargins(0, 0, 0, 0);
    choices->setSpacing(8);
    rangeGroup = new QButtonGroup(this);
    rangeGroup->setExclusive(true);
    auto* lengths = new QFrame(headerControls);
    lengths->setObjectName("reportsLengths");
    lengths->setProperty("segmented", true);
    lengths->setFixedHeight(Theme::controlHeight);
    auto* lengthsLayout = new QHBoxLayout(lengths);
    lengthsLayout->setContentsMargins(2, 2, 2, 2);
    lengthsLayout->setSpacing(2);
    addChip("1M", ReportRange::Preset::Month, lengthsLayout)->setToolTip("This month");
    addChip("3M", ReportRange::Preset::ThreeMonths, lengthsLayout)->setToolTip("The last 3 months");
    addChip("6M", ReportRange::Preset::SixMonths, lengthsLayout)->setToolTip("The last 6 months");
    addChip("1Y", ReportRange::Preset::Year, lengthsLayout)->setToolTip("The last 12 months");
    connect(rangeGroup, &QButtonGroup::idClicked, this, &ReportsPage::onRangeClicked);
    choices->addWidget(lengths);
    periodPicker = new PeriodPicker(headerControls);
    periodPicker->setObjectName("reportsPeriod");
    periodPicker->today = [this]() { return today(); };
    periodPicker->setWidestName("September 0000 – December 0000");   // Two years' months: the arrows stay put
    periodPicker->findChild<QToolButton*>("periodName")->setToolTip("Choose the first and last day");
    periodPicker->chooser = [this](const ReportRange& shown) -> std::optional<ReportRange> {
        QDate from = shown.first;                    // Starts from what's shown; cancelling keeps it
        QDate to = shown.last;
        if (!askCustomRange(from, to)) return std::nullopt;
        return ReportRange::custom(from, to);
    };
    connect(periodPicker, &PeriodPicker::periodChanged, this, [this](const ReportRange& range) {
        setRange(presetFor(range), range);
    });
    choices->addWidget(periodPicker);
    accountCombo = new QComboBox(this);
    accountCombo->setObjectName("reportsAccountCombo");
    accountCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    accountCombo->setFixedHeight(Theme::controlHeight);
    connect(accountCombo, &QComboBox::activated, this, [this]() {
        shownAccountId = accountCombo->currentData().toInt();
        refresh();
    });
    choices->addWidget(accountCombo);
    // The full width or the centred column, the same as in Settings, Appearance
    widthButton = new QToolButton(this);
    widthButton->setObjectName("reportsWidthButton");
    widthButton->setProperty("button", true);
    widthButton->setProperty("noIndicator", true);   // No padding: just its icon, square
    widthButton->setFixedSize(Theme::controlHeight, Theme::controlHeight);
    widthButton->setCursor(Qt::PointingHandCursor);
    connect(widthButton, &QToolButton::clicked, this, [this]() {
        Settings::instance().setReportsCentred(!Settings::instance().isReportsCentred());
        updateWidth();
        updateIcons();
        emit widthChanged();
    });
    choices->addWidget(widthButton);
    header->addWidget(headerControls, 0, Qt::AlignTop);
    root->addLayout(header);

    stack = new QStackedWidget(this);
    root->addWidget(stack, 1);

    // Overview: a card per report
    auto* overviewScroll = new QScrollArea(stack);
    overviewScroll->setObjectName("reportsOverview");
    overviewScroll->setFrameShape(QFrame::NoFrame);
    overviewScroll->setWidgetResizable(true);
    auto* overview = new QWidget(overviewScroll);
    auto* grid = new QGridLayout(overview);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(12);
    for (int gridColumn = 0; gridColumn < 3; ++gridColumn) grid->setColumnStretch(gridColumn, 1);   // Every card as wide, whatever it shows
    for (int i = 0; i < AllReports.size(); ++i) {
        const Report report = AllReports[i];
        auto* card = new ReportCard(title(report), overview);
        connect(card, &QAbstractButton::clicked, this, [this, report]() { openReport(report); });
        grid->addWidget(card, i / 3, i % 3);
        cards.append(card);
    }
    spendingPreview = new CategoryBars(cards[0]->preview());
    cards[0]->preview()->layout()->addWidget(spendingPreview);
    cards[0]->preview()->layout()->setAlignment(spendingPreview, Qt::AlignTop);
    ReportCard* flowCard = cards[static_cast<int>(Report::IncomeVsSpending)];
    flowPreview = new FlowChart(true, flowCard->preview());
    flowCard->preview()->layout()->addWidget(flowPreview);
    flowCard->preview()->layout()->setAlignment(flowPreview, Qt::AlignTop);
    ReportCard* balanceCard = cards[static_cast<int>(Report::Balance)];
    balancePreview = new BalanceChart(true, balanceCard->preview());
    balanceCard->preview()->layout()->addWidget(balancePreview);
    balanceCard->preview()->layout()->setAlignment(balancePreview, Qt::AlignTop);
    ReportCard* trendCard = cards[static_cast<int>(Report::CategoryTrend)];
    trendPreview = new TrendChart(true, trendCard->preview());
    trendCard->preview()->layout()->addWidget(trendPreview);
    trendCard->preview()->layout()->setAlignment(trendPreview, Qt::AlignTop);
    ReportCard* payeeCard = cards[static_cast<int>(Report::TopPayees)];
    payeePreview = new ElidedLabel(payeeCard->preview());
    payeePreview->setObjectName("payeePreview");
    payeePreview->setForegroundRole(QPalette::PlaceholderText);
    payeeCard->preview()->layout()->addWidget(payeePreview);
    payeeCard->preview()->layout()->setAlignment(payeePreview, Qt::AlignTop);
    ReportCard* budgetCard = cards[static_cast<int>(Report::BudgetVsActual)];
    budgetPreview = new UsageBar(budgetCard->preview());
    budgetCard->preview()->layout()->addWidget(budgetPreview);
    budgetCard->preview()->layout()->setAlignment(budgetPreview, Qt::AlignTop);
    ReportCard* moneyCard = cards[static_cast<int>(Report::MoneyFlow)];
    moneyFlowPreview = new SankeyChart(true, moneyCard->preview());
    moneyCard->preview()->layout()->addWidget(moneyFlowPreview);
    moneyCard->preview()->layout()->setAlignment(moneyFlowPreview, Qt::AlignTop);
    grid->setRowStretch(int(AllReports.size() + 2) / 3, 1);
    overviewScroll->setWidget(overview);
    stack->addWidget(overviewScroll);

    // One report in detail
    auto* detailScroll = new QScrollArea(stack);
    detailScroll->setObjectName("reportsDetail");
    detailScroll->setFrameShape(QFrame::NoFrame);
    detailScroll->setWidgetResizable(true);
    auto* detail = new QWidget(detailScroll);
    detailLayout = new QVBoxLayout(detail);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    detailLayout->addStretch();
    detailScroll->setWidget(detail);
    stack->addWidget(detailScroll);

    // The remembered range
    const Settings& settings = Settings::instance();
    const ReportRange::Preset saved = ReportRange::presetFromKey(settings.getReportsRange()).value_or(ReportRange::Preset::SixMonths);
    const ReportRange custom = ReportRange::custom(settings.getReportsFrom(), settings.getReportsTo());
    if (saved == ReportRange::Preset::Custom && custom.isValid()) {
        setRange(saved, custom);
    } else {
        const ReportRange::Preset start = saved == ReportRange::Preset::Custom ? ReportRange::Preset::SixMonths : saved;
        setRange(start, ReportRange::forPreset(start, today()));
    }
    showOverview();
    updateIcons();
    updateWidth();
}

QString ReportsPage::title(Report report) {
    switch (report) {
    case Report::SpendingByCategory: return "Spending by category";
    case Report::IncomeVsSpending: return "Income vs. spending";
    case Report::Balance: return "Balance over time";
    case Report::CategoryTrend: return "Category trend";
    case Report::TopPayees: return "Top payees";
    case Report::BudgetVsActual: return "Budget vs. actual";
    case Report::MoneyFlow: return "Money flow";
    }
    return QString();  // GCOVR_EXCL_LINE: every value returns above
}

// A segment of the length track
QToolButton* ReportsPage::addChip(const QString& text, ReportRange::Preset chipPreset, QLayout* layout) {
    auto* chip = new QToolButton(this);
    chip->setObjectName("reportsRange_" + ReportRange::key(chipPreset));
    chip->setText(text);
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    chip->setFixedHeight(Theme::controlHeight - 6);  // Inside the track's border and margins
    rangeGroup->addButton(chip, static_cast<int>(chipPreset));
    layout->addWidget(chip);
    return chip;
}

void ReportsPage::onRangeClicked(int presetId) {
    const auto clicked = static_cast<ReportRange::Preset>(presetId);
    setRange(clicked, ReportRange::forPreset(clicked, today()));
}

// A length's range while it ends with this month, so it moves on with the date; any other range is custom
ReportRange::Preset ReportsPage::presetFor(const ReportRange& range) const {
    for (const ReportRange::Preset length : {ReportRange::Preset::Month, ReportRange::Preset::ThreeMonths,
                                             ReportRange::Preset::SixMonths, ReportRange::Preset::Year}) {
        if (range == ReportRange::forPreset(length, today())) return length;
    }
    return ReportRange::Preset::Custom;
}

// The length a range has, also when stepped back: whole months, 1, 3, 6 or 12 of them
std::optional<ReportRange::Preset> ReportsPage::lengthOf(const ReportRange& range) {
    if (!range.isWholeMonths()) return std::nullopt;
    switch (range.months()) {
    case 1: return ReportRange::Preset::Month;
    case 3: return ReportRange::Preset::ThreeMonths;
    case 6: return ReportRange::Preset::SixMonths;
    case 12: return ReportRange::Preset::Year;
    default: return std::nullopt;
    }
}

void ReportsPage::setRange(ReportRange::Preset newPreset, const ReportRange& range) {
    preset = newPreset;
    shownRange = range;
    periodPicker->setPeriod(range);
    if (const std::optional<ReportRange::Preset> length = lengthOf(range)) {
        rangeGroup->button(static_cast<int>(*length))->setChecked(true);
    } else if (QAbstractButton* checked = rangeGroup->checkedButton()) {
        rangeGroup->setExclusive(false);             // An exclusive group can't have none checked
        checked->setChecked(false);
        rangeGroup->setExclusive(true);
    }
    Settings& settings = Settings::instance();
    settings.setReportsRange(ReportRange::key(preset));
    if (preset == ReportRange::Preset::Custom) settings.setReportsCustomDays(range.first, range.last);
    refresh();
}

void ReportsPage::openBudgetMonth(const QDate& month) {
    const ReportRange thisMonth = ReportRange::forPreset(ReportRange::Preset::Month, today());
    const QDate first(month.year(), month.month(), 1);
    if (first == thisMonth.first) {
        setRange(ReportRange::Preset::Month, thisMonth);
    } else {
        setRange(ReportRange::Preset::Custom, ReportRange::custom(first, first.addMonths(1).addDays(-1)));
    }
    openReport(Report::BudgetVsActual);
}

void ReportsPage::setUserId(int newUserId) {
    userId = newUserId;
    shownAccountId = ALL_ACCOUNTS;
    reload();
}

void ReportsPage::reload() {
    // The month ranges move on with the date
    if (preset != ReportRange::Preset::Custom) {
        shownRange = ReportRange::forPreset(preset, today());
        periodPicker->setPeriod(shownRange);
    }
    accountCombo->clear();
    accountCombo->addItem("All accounts", ALL_ACCOUNTS);
    if (userId > 0) {
        for (const Account& account : AccountDAO().getAllAccountsForUser(userId)) {
            accountCombo->addItem(account.getName(), account.getAccountId());
        }
    }
    const int index = accountCombo->findData(shownAccountId);
    if (index < 0) shownAccountId = ALL_ACCOUNTS;    // Deleted meanwhile
    accountCombo->setCurrentIndex(qMax(0, index));
    refresh();
}

void ReportsPage::applySettings() {
    updateWidth();
    updateIcons();                                   // The width button's
    refresh();
}

// The column is placed by hand, not by a layout: a layout would count centred margins into the
// page's minimum size, and the window could no longer get narrower. Centred, it's CentredWidth wide
// (or what the header needs) with the rest half on either side.
void ReportsPage::updateWidth() {
    const QRect area = rect().adjusted(Margin.left(), Margin.top(), -Margin.right(), -Margin.bottom());
    int columnWidth = area.width();
    if (Settings::instance().isReportsCentred()) {
        columnWidth = qMin(area.width(), qMax(CentredWidth, column->minimumSizeHint().width()));
    }
    column->setGeometry(area.left() + (area.width() - columnWidth) / 2, area.top(), columnWidth, area.height());
}

QSize ReportsPage::minimumSizeHint() const {
    return column->minimumSizeHint().grownBy(Margin);
}

QSize ReportsPage::sizeHint() const {
    return column->sizeHint().grownBy(Margin);
}

void ReportsPage::resizeEvent(QResizeEvent* event) {
    updateWidth();
    QWidget::resizeEvent(event);
}

// The column asks for this when it needs more or less room, for a longer title or a larger font
bool ReportsPage::event(QEvent* event) {
    if (event->type() == QEvent::LayoutRequest) {
        updateGeometry();                            // The page's own minimum follows the column's
        updateWidth();
        return true;
    }
    return QWidget::event(event);
}

void ReportsPage::refresh() {
    updateHeader();
    if (userId <= 0) return;
    spending = Reports::spendingByCategory(userId, shownAccountId, shownRange, today());
    flow = Reports::incomeVsSpending(userId, shownAccountId, shownRange);
    balance = Reports::balanceOverTime(userId, shownAccountId, shownRange, today());
    payees = Reports::topPayees(userId, shownAccountId, shownRange);
    budgets = Reports::budgetVsActual(userId, shownRange, today());
    moneyFlow = Reports::moneyFlow(userId, shownAccountId, shownRange);
    if (const std::optional<int> category = trendCategory()) {
        trend = Reports::categoryTrend(userId, shownAccountId, *category, shownRange, today());
    } else {
        trend.reset();
    }
    updateCards();
    updateDetail();
}

std::optional<int> ReportsPage::trendCategory() const {
    if (chosenTrendCategory) return chosenTrendCategory;
    for (const CategorySpent& spent : spending.categories) {
        if (spent.spentCents > 0) return spent.categoryId;
    }
    return std::nullopt;
}

void ReportsPage::openCategoryTrend(int categoryId) {
    chosenTrendCategory = categoryId;
    if (userId > 0) trend = Reports::categoryTrend(userId, shownAccountId, categoryId, shownRange, today());
    updateCards();
    openReport(Report::CategoryTrend);
}

void ReportsPage::openReport(Report report) {
    opened = report;
    stack->setCurrentIndex(1);
    updateHeader();
    updateDetail();
}

void ReportsPage::showOverview() {
    opened.reset();
    stack->setCurrentIndex(0);
    updateHeader();
    updateDetail();
}

void ReportsPage::updateCards() {
    const Settings& settings = Settings::instance();
    ReportCard* spendingCard = cards[static_cast<int>(Report::SpendingByCategory)];
    if (spending.categories.isEmpty()) {
        spendingCard->setValue(QString(), "No spending in this range");
    } else if (Reports::changePercent(spending.totalCents, spending.beforeTotalCents)) {
        int direction = 0;
        const QString change = changeText(spending.totalCents, spending.beforeTotalCents, direction);
        spendingCard->setValue(settings.formatSpending(spending.totalCents),
                               QString("%1 vs. %2").arg(change, shownRange.comparisonText(today())),
                               changeColor(direction));
    } else {
        const int count = int(spending.categories.size());
        spendingCard->setValue(settings.formatSpending(spending.totalCents),
                               QString("In %1 %2").arg(count).arg(count == 1 ? "category" : "categories"));
    }
    spendingPreview->setCategories(spending.categories);

    // Income vs. spending: how much of the income was kept
    ReportCard* flowCard = cards[static_cast<int>(Report::IncomeVsSpending)];
    const Cents net = flow.netCents();
    if (flow.incomeCents == 0 && flow.spendingCents == 0) {
        flowCard->setValue(QString(), "No income or spending in this range");
    } else if (net < 0) {
        flowCard->setValue("Spent more", QString("%1 more than came in").arg(settings.formatCurrency(-net)), Theme::colors().danger);
    } else {
        flowCard->setValue(QString("Kept %1%").arg(flow.keptPercent().value_or(0)),
                           QString("%1 of %2 income").arg(settings.formatCurrency(net), settings.formatCurrency(flow.incomeCents)));
    }
    flowPreview->setReport(flow);

    // Balance over time: where it ended up, and how far it moved
    ReportCard* balanceCard = cards[static_cast<int>(Report::Balance)];
    const Cents moved = balance.changeCents();
    const QDate since = balance.range.first;
    balanceCard->setValue(settings.formatCurrency(balance.endCents()),
                          QString("%1 since %2").arg(settings.formatCurrency(moved, true),
                                                     english.toString(since, since.year() == today().year() ? "d MMMM" : "d MMMM yyyy")),
                          moved > 0 ? Theme::colors().positive : moved < 0 ? Theme::colors().danger : QColor());
    balancePreview->setReport(balance);

    // Category trend: the chosen category, or the largest one, and its average
    ReportCard* trendCard = cards[static_cast<int>(Report::CategoryTrend)];
    if (trend) {
        const bool byWeek = shownRange.flowUnit() == ReportRange::Unit::Week;
        trendCard->setValue(trend->categoryName, QString("%1 a %2 on average").arg(settings.formatSpending(trend->averageCents),
                                                                                  byWeek ? "week" : "month"));
        trendPreview->setTrend(*trend);
    } else {
        trendCard->setValue(QString(), "No spending in this range");
        trendPreview->setTrend(CategoryTrend());
    }

    // Top payees: the largest, and the next three below
    ReportCard* payeeCard = cards[static_cast<int>(Report::TopPayees)];
    if (payees.payees.isEmpty() || payees.payees.first().spentCents <= 0) {
        payeeCard->setValue(QString(), "No spending in this range");
        payeePreview->clear();
    } else {
        const PayeeSpent& top = payees.payees.first();
        payeeCard->setValue(payeeName(top.payee), QString("%1 in %2 %3").arg(settings.formatSpending(top.spentCents))
                                                      .arg(top.count).arg(top.count == 1 ? "transaction" : "transactions"));
        QStringList lines;
        for (const PayeeSpent& next : payees.payees.mid(1, 3)) {
            if (next.spentCents > 0) lines.append(QString("%1 · %2").arg(payeeName(next.payee), settings.formatSpending(next.spentCents)));
        }
        payeePreview->setText(lines.join('\n'));
    }

    // Budget vs. actual: how many were kept, and how full they are together
    ReportCard* budgetCard = cards[static_cast<int>(Report::BudgetVsActual)];
    if (budgets.categories.isEmpty()) {
        budgetCard->setValue(QString(), "No budgets in this range");
        budgetPreview->setRatio(0.0);
    } else {
        budgetCard->setValue(QString("%1 of %2 kept").arg(budgets.keptCount()).arg(budgets.categories.size()),
                             QString("%1 of %2 budgeted").arg(settings.formatSpending(budgets.spentCents),
                                                              settings.formatCurrency(budgets.limitCents)),
                             budgets.spentCents > budgets.limitCents ? Theme::colors().danger : QColor());
        budgetPreview->setRatio(double(budgets.spentCents) / double(budgets.limitCents));
    }

    // Money flow: where most of the income went
    ReportCard* moneyCard = cards[static_cast<int>(Report::MoneyFlow)];
    if (moneyFlow.spending.isEmpty()) {
        moneyCard->setValue(QString(), moneyFlow.incomeCents > 0 ? "All of the income kept" : "No income or spending in this range");
    } else {
        const FlowPart& largest = moneyFlow.spending.first();
        moneyCard->setValue(largest.name, moneyFlow.incomeCents > 0
                                              ? QString("%1, %2 of the income").arg(settings.formatSpending(largest.cents),
                                                                                    shareText(largest.cents, moneyFlow.incomeCents))
                                              : settings.formatSpending(largest.cents));
    }
    moneyFlowPreview->setFlow(moneyFlow);
}

void ReportsPage::updateDetail() {
    // Off the page at once, deleted later: the row or picker that asked for this may still be busy
    if (detailContent) {
        detailContent->hide();
        detailContent->setParent(nullptr);
        detailContent->deleteLater();
    }
    detailContent = nullptr;
    if (!opened) return;
    QWidget* parent = detailLayout->parentWidget();
    if (*opened == Report::SpendingByCategory) {
        detailContent = spendingDetail(parent);
    } else if (*opened == Report::IncomeVsSpending) {
        detailContent = flowDetail(parent);
    } else if (*opened == Report::Balance) {
        detailContent = balanceDetail(parent);
    } else if (*opened == Report::CategoryTrend) {
        detailContent = trendDetail(parent);
    } else if (*opened == Report::TopPayees) {
        detailContent = payeeDetail(parent);
    } else if (*opened == Report::MoneyFlow) {
        detailContent = moneyFlowDetail(parent);
    } else {
        detailContent = budgetDetail(parent);
    }
    detailLayout->insertWidget(0, detailContent);
}

QWidget* ReportsPage::spendingDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("spendingDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    if (spending.categories.isEmpty()) {
        auto* empty = new QLabel("No spending in this range.", widget);
        empty->setObjectName("reportsEmptyLabel");
        empty->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(empty);
        return widget;
    }

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const auto columns = [](const QString& spent, const QString& share, const QString& change) {
        return QList<Column>{{spent, 130, Tone::Normal}, {share, 64, Tone::Secondary}, {change, 80, Tone::Change}};
    };
    const Settings& settings = Settings::instance();
    auto* heading = new ReportRow(ReportRow::Kind::Heading, "Category", columns("Spent", "Share", "Change"), false, widget);
    heading->setObjectName("spendingHeading");
    layout->addWidget(heading);
    // Refunded categories (below zero) come last and get no bar
    const double largest = double(qMax<Cents>(1, spending.categories.first().spentCents));
    for (const CategorySpent& spent : spending.categories) {
        int direction = 0;
        const QString amount = settings.formatSpending(spent.spentCents);
        const QString share = shareText(spent.spentCents, spending.totalCents);
        const QString change = changeText(spent.spentCents, spent.beforeCents, direction);
        auto* row = new ReportRow(ReportRow::Kind::Item, spent.categoryName, columns(amount, share, change), true, widget);
        row->setObjectName("spendingRow");
        row->setAccessibleDescription(QString("%1, %2 of all spending, %3").arg(amount, share, change));
        row->ratio = double(spent.spentCents) / largest;
        row->categoryColor = spent.categoryColor;
        row->direction = direction;
        row->makeClickable(QString("%1\nClick for its trend")
                               .arg(spent.beforeCents != 0 ? QString("%1 in %2").arg(settings.formatSpending(spent.beforeCents),
                                                                                     spending.before.label())
                                                           : QString("Nothing in %1").arg(spending.before.label())));
        const int categoryId = spent.categoryId;
        connect(row, &QAbstractButton::clicked, this, [this, categoryId]() { openCategoryTrend(categoryId); });
        layout->addWidget(row);
    }
    int direction = 0;
    const QString change = changeText(spending.totalCents, spending.beforeTotalCents, direction);
    auto* total = new ReportRow(ReportRow::Kind::Total, "Total",
                                columns(settings.formatSpending(spending.totalCents),
                                        spending.totalCents > 0 ? QString("100%") : QString("—"), change),
                                false, widget);
    total->setObjectName("spendingTotal");
    total->direction = direction;
    total->setToolTip(QString("%1 in %2").arg(settings.formatSpending(spending.beforeTotalCents), spending.before.label()));
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::flowDetail(QWidget* parent) const {
    auto* widget = new QWidget(parent);
    widget->setObjectName("flowDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    auto* chart = new FlowChart(false, widget);
    chart->setReport(flow);
    layout->addWidget(chart);
    layout->addSpacing(16);

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const Settings& settings = Settings::instance();
    const auto kept = [](Cents income, Cents net) {
        const std::optional<int> percent = Reports::keptPercent(income, net);
        return percent ? QString("%1%").arg(*percent) : QString("—");
    };
    const auto columns = [](const QString& income, const QString& spent, const QString& net, bool short_, const QString& keptText) {
        return QList<Column>{{income, 120, Tone::Normal}, {spent, 120, Tone::Normal},
                             {net, 120, short_ ? Tone::Bad : Tone::Normal}, {keptText, 64, Tone::Secondary}};
    };
    const bool byWeek = flow.range.flowUnit() == ReportRange::Unit::Week;
    auto* heading = new ReportRow(ReportRow::Kind::Heading, byWeek ? "Week" : "Month",
                                  columns("Income", "Spending", "Net", false, "Kept"), false, widget);
    heading->setObjectName("flowHeading");
    layout->addWidget(heading);
    for (const PeriodFlow& period : flow.periods) {
        const Cents net = period.netCents();
        auto* row = new ReportRow(ReportRow::Kind::Item, periodTitle(period.period, flow.range),
                                  columns(settings.formatCurrency(period.incomeCents), settings.formatSpending(period.spendingCents),
                                          settings.formatCurrency(net, true), net < 0, kept(period.incomeCents, net)),
                                  false, widget);
        row->setObjectName("flowRow");
        row->setAccessibleDescription(QString("Income %1, spending %2, net %3")
                                          .arg(settings.formatCurrency(period.incomeCents), settings.formatSpending(period.spendingCents),
                                               settings.formatCurrency(net, true)));
        layout->addWidget(row);
    }
    const Cents net = flow.netCents();
    auto* total = new ReportRow(ReportRow::Kind::Total, "Total",
                                columns(settings.formatCurrency(flow.incomeCents), settings.formatSpending(flow.spendingCents),
                                        settings.formatCurrency(net, true), net < 0, kept(flow.incomeCents, net)),
                                false, widget);
    total->setObjectName("flowTotal");
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::balanceDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("balanceDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    auto* chart = new BalanceChart(false, widget);
    chart->setReport(balance);
    layout->addWidget(chart);
    layout->addSpacing(16);

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const Settings& settings = Settings::instance();
    const auto columns = [](const QString& amount, const QString& change) {
        return QList<Column>{{amount, 140, Tone::Normal}, {change, 140, Tone::Change}};
    };
    // A higher balance is the good direction
    const auto direction = [](Cents change) { return change > 0 ? -1 : change < 0 ? 1 : 0; };
    const ReportRange::Unit unit = balance.range.flowUnit();
    auto* heading = new ReportRow(ReportRow::Kind::Heading, unit == ReportRange::Unit::Week ? "Week" : "Month",
                                  columns("Balance at the end", "Change"), false, widget);
    heading->setObjectName("balanceHeading");
    layout->addWidget(heading);
    const QDate lastShown = balance.days.isEmpty() ? balance.range.last : balance.days.last().date;
    for (const ReportRange& period : balance.range.periods(unit)) {
        if (period.first > lastShown) break;                 // Still to come
        const Cents end = balance.balanceOn(period.last);
        const Cents change = end - balance.balanceOn(period.first.addDays(-1));
        auto* row = new ReportRow(ReportRow::Kind::Item, periodTitle(period, balance.range),
                                  columns(settings.formatCurrency(end), settings.formatCurrency(change, true)), false, widget);
        row->setObjectName("balanceRow");
        row->direction = direction(change);
        row->setAccessibleDescription(QString("%1, %2").arg(settings.formatCurrency(end), settings.formatCurrency(change, true)));
        layout->addWidget(row);
    }
    auto* total = new ReportRow(ReportRow::Kind::Total, "Whole range",
                                columns(settings.formatCurrency(balance.endCents()), settings.formatCurrency(balance.changeCents(), true)),
                                false, widget);
    total->setObjectName("balanceTotal");
    total->direction = direction(balance.changeCents());
    total->setToolTip(QString("%1 at the start").arg(settings.formatCurrency(balance.startCents)));
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::payeeDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("payeeDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    if (payees.payees.isEmpty()) {
        auto* empty = new QLabel("No spending in this range.", widget);
        empty->setObjectName("reportsEmptyLabel");
        empty->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(empty);
        return widget;
    }

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const auto columns = [](const QString& spent, const QString& share, const QString& count) {
        return QList<Column>{{spent, 130, Tone::Normal}, {share, 64, Tone::Secondary}, {count, 120, Tone::Secondary}};
    };
    const auto countText = [](int count) { return QString("%1 %2").arg(count).arg(count == 1 ? "transaction" : "transactions"); };
    const Settings& settings = Settings::instance();
    auto* heading = new ReportRow(ReportRow::Kind::Heading, "Payee", columns("Spent", "Share", "Transactions"), false, widget);
    heading->setObjectName("payeeHeading");
    layout->addWidget(heading);

    // The largest ones, then everyone else together
    constexpr int Shown = 20;
    const double largest = double(qMax<Cents>(1, payees.payees.first().spentCents));
    const ReportRange range = payees.range;
    const int account = shownAccountId;
    for (const PayeeSpent& spent : payees.payees.mid(0, Shown)) {
        const QString amount = settings.formatSpending(spent.spentCents);
        const QString share = shareText(spent.spentCents, payees.totalCents);
        auto* row = new ReportRow(ReportRow::Kind::Item, payeeName(spent.payee), columns(amount, share, countText(spent.count)), true, widget);
        row->setObjectName("payeeRow");
        row->setAccessibleDescription(QString("%1, %2 of all spending, %3").arg(amount, share, countText(spent.count)));
        row->ratio = double(spent.spentCents) / largest;
        if (!spent.payee.isEmpty()) {
            row->makeClickable(QString("Show the transactions with %1").arg(spent.payee));
            const QString payee = spent.payee;
            connect(row, &QAbstractButton::clicked, this, [this, payee, account, range]() {
                emit payeeTransactionsRequested(payee, account, range.first, range.last);
            });
        }
        layout->addWidget(row);
    }
    if (payees.payees.size() > Shown) {
        Cents restCents = 0;
        int restCount = 0;
        for (const PayeeSpent& spent : payees.payees.mid(Shown)) {
            restCents += spent.spentCents;
            restCount += spent.count;
        }
        const int others = int(payees.payees.size()) - Shown;
        auto* rest = new ReportRow(ReportRow::Kind::Item, QString("%1 other %2").arg(others).arg(others == 1 ? "payee" : "payees"),
                                   columns(settings.formatSpending(restCents), shareText(restCents, payees.totalCents), countText(restCount)),
                                   false, widget);
        rest->setObjectName("payeeRest");
        layout->addWidget(rest);
    }
    int totalCount = 0;
    for (const PayeeSpent& spent : payees.payees) totalCount += spent.count;
    auto* total = new ReportRow(ReportRow::Kind::Total, "Total",
                                columns(settings.formatSpending(payees.totalCents), payees.totalCents > 0 ? QString("100%") : QString("—"),
                                        countText(totalCount)),
                                false, widget);
    total->setObjectName("payeeTotal");
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::budgetDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("budgetDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);

    // Budgets don't follow the range or account exactly; say so rather than look as if they did
    QStringList notes;
    if (budgets.range.isValid() && !(budgets.range == shownRange)) notes.append(QString("Budgets are per month, so this covers %1.").arg(budgets.range.label()));
    if (shownAccountId != ALL_ACCOUNTS) notes.append("Budgets are for all accounts together, so the chosen account doesn't apply here.");
    if (!notes.isEmpty()) {
        auto* note = new QLabel(notes.join(' '), widget);
        note->setObjectName("budgetNote");
        note->setWordWrap(true);
        note->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(note);
        layout->addSpacing(10);
    }
    if (budgets.categories.isEmpty()) {
        auto* empty = new QLabel("No budgets in this range. Set limits per category on the Budget page.", widget);
        empty->setObjectName("reportsEmptyLabel");
        empty->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(empty);
        return widget;
    }

    // How it went, then the chart: the pace in a single month, the months otherwise
    const QString summary = budgetSummary(budgets, today());
    if (!summary.isEmpty()) {
        auto* label = new QLabel(summary, widget);
        label->setObjectName("budgetSummary");
        layout->addWidget(label);
        layout->addSpacing(8);
    }
    // The chart in views, chosen like the range: all budgets together (the pace in a month, the
    // months otherwise), each budget against its own limit, and for several months a grid. The
    // view is remembered. All of them are as tall, so the list below stays put when switching.
    const bool singleMonth = budgets.months.size() == 1;
    auto* viewChips = new QHBoxLayout();
    viewChips->setSpacing(6);
    auto* viewGroup = new QButtonGroup(widget);
    const QList<std::pair<QString, QString>> viewNames = {{"together", "All together"}, {"each", "Each budget"}, {"grid", "Grid"}};
    for (int i = 0; i < (singleMonth ? 2 : 3); ++i) {
        auto* viewChip = new QToolButton(widget);
        viewChip->setObjectName("budgetView_" + viewNames[i].first);
        viewChip->setText(viewNames[i].second);
        viewChip->setCheckable(true);
        viewChip->setCursor(Qt::PointingHandCursor);
        viewChip->setProperty("chip", true);         // Theme::controlStyleSheet tints it while checked
        viewGroup->addButton(viewChip, i);
        viewChips->addWidget(viewChip);
    }
    viewChips->addStretch();
    layout->addLayout(viewChips);
    layout->addSpacing(10);

    QList<QWidget*> views;
    auto* chart = new BudgetChart(widget);
    chart->setReport(budgets);
    views.append(chart);

    const auto showMonth = [this](int categoryId, const ReportRange& month) {
        emit transactionsRequested(categoryId, ALL_ACCOUNTS, month.first, month.last);
    };
    QStringList colorNames;
    for (const BudgetActual& budget : budgets.categories) colorNames << budget.categoryColor;
    const QList<QColor> lineColors = budgetLineColors(colorNames);
    auto* each = new QWidget(widget);
    each->setObjectName("budgetEachView");
    auto* eachLayout = new QVBoxLayout(each);
    eachLayout->setContentsMargins(0, 0, 0, 0);
    QStringList legendEntries;
    const auto legendEntry = [](const QColor& color, const QString& mark, const QString& text) {
        return QString("<span style=\"color:%1\">%2</span>&nbsp;%3").arg(color.name(), mark, text.toHtmlEscaped());
    };
    for (int i = 0; i < budgets.categories.size(); ++i) legendEntries << legendEntry(lineColors[i], "■", budgets.categories[i].categoryName);
    if (singleMonth) {
        QList<BudgetLinesChart::Line> lines;
        for (int i = 0; i < budgets.categories.size(); ++i) {
            const BudgetActual& budget = budgets.categories[i];
            lines.append({budget.categoryId, budget.categoryName, lineColors[i], budget.limitCents, budget.spentByDay});
        }
        auto* linesChart = new BudgetLinesChart(each);
        const ReportRange month = budgets.months.first().month;
        linesChart->setLines(month, lines);
        connect(linesChart, &BudgetLinesChart::budgetClicked, this, [showMonth, month](int categoryId) { showMonth(categoryId, month); });
        eachLayout->addWidget(linesChart, 1);
        legendEntries << legendEntry(Theme::colors().textSecondary, "┅", "Even pace");
    } else {
        QList<ReportRange> months;
        for (const BudgetMonth& month : budgets.months) months.append(month.month);
        QList<BudgetMonthsChart::Line> lines;
        for (int i = 0; i < budgets.categories.size(); ++i) {
            const BudgetActual& budget = budgets.categories[i];
            lines.append({budget.categoryId, budget.categoryName, lineColors[i], budget.byMonth});
        }
        auto* monthsChart = new BudgetMonthsChart(each);
        monthsChart->setLines(months, lines);
        connect(monthsChart, &BudgetMonthsChart::monthClicked, this, showMonth);
        eachLayout->addWidget(monthsChart, 1);

        auto* grid = new BudgetGrid(widget);
        grid->setBudgets(months, budgets.categories);
        connect(grid, &BudgetGrid::monthClicked, this, showMonth);
        views.append(each);
        views.append(grid);
    }
    if (singleMonth) views.append(each);
    auto* legend = new QLabel(legendEntries.join("&nbsp;&nbsp;&nbsp; "), each);
    legend->setObjectName("budgetLegend");
    legend->setTextFormat(Qt::RichText);
    legend->setWordWrap(true);
    legend->setForegroundRole(QPalette::PlaceholderText);
    eachLayout->addWidget(legend);

    // The charts are as tall as each other, so the list stays put between them; the grid only as
    // tall as its rows. Only the shown view is visible, so the others take no room.
    auto* viewArea = new QWidget(widget);
    viewArea->setObjectName("budgetViews");
    auto* viewLayout = new QVBoxLayout(viewArea);
    viewLayout->setContentsMargins(0, 0, 0, 0);
    for (QWidget* view : std::as_const(views)) {
        if (view != views.value(2)) view->setMinimumHeight(ChartHeight);
        viewLayout->addWidget(view);
    }
    const auto showView = [views](int index) {
        for (int i = 0; i < views.size(); ++i) views[i]->setVisible(i == index);
    };
    const int shown = qBound(0, Settings::instance().getReportsBudgetView(), int(views.size()) - 1);
    viewGroup->button(shown)->setChecked(true);
    showView(shown);
    layout->addWidget(viewArea);
    connect(viewGroup, &QButtonGroup::idClicked, this, [showView](int index) {
        showView(index);
        Settings::instance().setReportsBudgetView(index);
    });
    layout->addSpacing(16);

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const auto columns = [](const QString& limit, const QString& spent, const QString& left, bool over, const QString& used) {
        return QList<Column>{{limit, 120, Tone::Normal}, {spent, 120, Tone::Normal},
                             {left, 120, over ? Tone::Bad : Tone::Normal}, {used, 64, Tone::Secondary}};
    };
    const auto usedText = [](Cents spent, Cents limit) { return QString("%1%").arg(qRound(double(spent) / double(limit) * 100.0)); };
    const Settings& settings = Settings::instance();
    auto* heading = new ReportRow(ReportRow::Kind::Heading, "Budget", columns("Limit", "Spent", "Left", false, "Used"), false, widget);
    heading->setObjectName("budgetHeading");
    layout->addWidget(heading);
    for (const BudgetActual& budget : budgets.categories) {
        const QString limit = settings.formatCurrency(budget.limitCents);
        const QString spent = settings.formatSpending(budget.spentCents);
        const QString left = settings.formatCurrency(budget.leftCents());
        const QString used = usedText(budget.spentCents, budget.limitCents);
        auto* row = new ReportRow(ReportRow::Kind::Item, budget.categoryName, columns(limit, spent, left, !budget.kept(), used), true, widget);
        row->setObjectName("budgetRow");
        row->setAccessibleDescription(QString("Limit %1, spent %2, left %3, %4 used").arg(limit, spent, left, used));
        row->showsUsage = true;
        row->ratio = double(budget.spentCents) / double(budget.limitCents);
        const QString monthsText = budget.months == 1 ? QString("1 month with a budget") : QString("%1 months with a budget").arg(budget.months);
        const QString overText = budget.monthsOver > 0 ? QString(", over in %1").arg(budget.monthsOver) : QString();
        row->makeClickable(QString("%1%2\nClick for its trend").arg(monthsText, overText));
        const int categoryId = budget.categoryId;
        connect(row, &QAbstractButton::clicked, this, [this, categoryId]() { openCategoryTrend(categoryId); });
        layout->addWidget(row);
    }
    auto* total = new ReportRow(ReportRow::Kind::Total, "Total",
                                columns(settings.formatCurrency(budgets.limitCents), settings.formatSpending(budgets.spentCents),
                                        settings.formatCurrency(budgets.limitCents - budgets.spentCents),
                                        budgets.spentCents > budgets.limitCents, usedText(budgets.spentCents, budgets.limitCents)),
                                false, widget);
    total->setObjectName("budgetTotal");
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::moneyFlowDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("moneyFlowDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);

    // A category's band or row leads to its transactions
    const ReportRange range = moneyFlow.range;
    const int account = shownAccountId;
    const auto showCategory = [this, account, range](int categoryId) {
        emit transactionsRequested(categoryId, account, range.first, range.last);
    };
    auto* chart = new SankeyChart(false, widget);
    chart->setFlow(moneyFlow);
    chart->partClicked = showCategory;
    layout->addWidget(chart);
    if (moneyFlow.totalCents() <= 0) return widget;
    layout->addSpacing(16);

    // Where it went, as numbers
    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const Settings& settings = Settings::instance();
    const auto columns = [](const QString& amount, const QString& share) {
        return QList<Column>{{amount, 130, Tone::Normal}, {share, 120, Tone::Secondary}};
    };
    const auto share = [this](Cents cents) { return shareText(cents, moneyFlow.incomeCents); };
    auto* heading = new ReportRow(ReportRow::Kind::Heading, "Where it went", columns("Amount", "Of the income"), false, widget);
    heading->setObjectName("moneyFlowHeading");
    layout->addWidget(heading);
    for (const FlowPart& part : moneyFlow.spending) {
        auto* row = new ReportRow(ReportRow::Kind::Item, part.name, columns(settings.formatSpending(part.cents), share(part.cents)), false, widget);
        row->setObjectName("moneyFlowRow");
        if (!part.isRest) {
            row->makeClickable(QString("Show the transactions in %1").arg(part.name));
            const int categoryId = part.categoryId;
            connect(row, &QAbstractButton::clicked, this, [showCategory, categoryId]() { showCategory(categoryId); });
        }
        layout->addWidget(row);
    }
    if (moneyFlow.keptCents() > 0) {
        auto* kept = new ReportRow(ReportRow::Kind::Item, "Kept", columns(settings.formatCurrency(moneyFlow.keptCents()), share(moneyFlow.keptCents())),
                                   false, widget);
        kept->setObjectName("moneyFlowKept");
        layout->addWidget(kept);
    }
    if (moneyFlow.fromSavingsCents() > 0) {
        auto* savings = new ReportRow(ReportRow::Kind::Item, "From savings, more than came in",
                                      {{settings.formatCurrency(moneyFlow.fromSavingsCents()), 130, Tone::Bad}, {QString(), 120, Tone::Secondary}},
                                      false, widget);
        savings->setObjectName("moneyFlowFromSavings");
        layout->addWidget(savings);
    }
    auto* total = new ReportRow(ReportRow::Kind::Total, "Income", columns(settings.formatCurrency(moneyFlow.incomeCents),
                                                                          moneyFlow.incomeCents > 0 ? QString("100%") : QString("—")),
                                false, widget);
    total->setObjectName("moneyFlowTotal");
    layout->addWidget(total);
    return widget;
}

QWidget* ReportsPage::trendDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("trendDetail");
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 4, 0, 0);
    layout->setSpacing(0);
    if (!trend) {
        auto* empty = new QLabel("No spending in this range.", widget);
        empty->setObjectName("reportsEmptyLabel");
        empty->setForegroundRole(QPalette::PlaceholderText);
        layout->addWidget(empty);
        return widget;
    }

    // Which category, and the way on to its transactions
    const int categoryId = trend->categoryId;
    const ReportRange range = trend->range;
    const int account = shownAccountId;
    auto* bar = new QHBoxLayout();
    bar->setSpacing(8);
    auto* categoryCombo = new QComboBox(widget);
    categoryCombo->setObjectName("trendCategoryCombo");
    categoryCombo->setAccessibleName("Category");
    categoryCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    for (const Category& category : CategoryDAO(userId).getAllCategories()) {
        const QString name = category.getName();
        if (category.getType() != "expense" || name == TransferCategory || name == InitialBalanceCategory
            || name == BalanceCorrectionCategory) {
            continue;
        }
        categoryCombo->addItem(name, category.getCategoryId());
    }
    categoryCombo->addItem("No category", 0);
    if (categoryCombo->findData(categoryId) < 0) categoryCombo->addItem(trend->categoryName, categoryId);   // An income category
    categoryCombo->setCurrentIndex(categoryCombo->findData(categoryId));
    connect(categoryCombo, &QComboBox::activated, this, [this, categoryCombo]() {
        openCategoryTrend(categoryCombo->currentData().toInt());
    });
    bar->addWidget(categoryCombo);
    bar->addStretch();
    auto* transactionsButton = new QPushButton("Show transactions", widget);
    transactionsButton->setObjectName("trendTransactionsButton");
    transactionsButton->setToolTip(QString("%1 in %2").arg(trend->categoryName, range.label()));
    connect(transactionsButton, &QPushButton::clicked, this, [this, categoryId, account, range]() {
        emit transactionsRequested(categoryId, account, range.first, range.last);
    });
    bar->addWidget(transactionsButton);
    layout->addLayout(bar);
    layout->addSpacing(12);

    // A week or month leads to its transactions, from the chart or the list
    const QList<PeriodSpent> periods = trend->periods;
    const auto showPeriod = [this, categoryId, account, periods](int index) {
        emit transactionsRequested(categoryId, account, periods[index].period.first, periods[index].period.last);
    };
    auto* chart = new TrendChart(false, widget);
    chart->setTrend(*trend);
    chart->periodClicked = showPeriod;
    layout->addWidget(chart);
    layout->addSpacing(16);

    using Column = ReportRow::Column;
    using Tone = ReportRow::Tone;
    const Settings& settings = Settings::instance();
    const auto columns = [](const QString& spent, const QString& count) {
        return QList<Column>{{spent, 140, Tone::Normal}, {count, 120, Tone::Secondary}};
    };
    const auto countText = [](int count) { return QString("%1 %2").arg(count).arg(count == 1 ? "transaction" : "transactions"); };
    const bool byWeek = range.flowUnit() == ReportRange::Unit::Week;
    auto* heading = new ReportRow(ReportRow::Kind::Heading, byWeek ? "Week" : "Month", columns("Spent", "Transactions"), false, widget);
    heading->setObjectName("trendHeading");
    layout->addWidget(heading);
    for (int i = 0; i < periods.size(); ++i) {
        const PeriodSpent& spent = periods[i];
        auto* row = new ReportRow(ReportRow::Kind::Item, periodTitle(spent.period, range),
                                  columns(settings.formatSpending(spent.spentCents), countText(spent.count)), false, widget);
        row->setObjectName("trendRow");
        row->setAccessibleDescription(QString("%1, %2").arg(settings.formatSpending(spent.spentCents), countText(spent.count)));
        row->makeClickable(QString("Show these %1 transactions").arg(trend->categoryName));
        connect(row, &QAbstractButton::clicked, this, [showPeriod, i]() { showPeriod(i); });
        layout->addWidget(row);
    }
    auto* total = new ReportRow(ReportRow::Kind::Total, "Total",
                                columns(settings.formatSpending(trend->totalCents), countText(trend->count)), false, widget);
    total->setObjectName("trendTotal");
    total->setToolTip(QString("%1 a %2 on average").arg(settings.formatSpending(trend->averageCents), byWeek ? "week" : "month"));
    layout->addWidget(total);
    return widget;
}

std::optional<ReportsPage::Report> ReportsPage::openedReport() const {
    return opened;
}

void ReportsPage::updateHeader() {
    backLink->setVisible(opened.has_value());
    titleLabel->setText(opened ? title(*opened) : QString("Reports"));
    QString rangeText = describe(shownRange);
    if (opened == Report::SpendingByCategory) rangeText += " · vs. " + shownRange.comparisonText(today());
    rangeLabel->setText(rangeText);
}

// How long the range is, as the stepper already shows its days
QString ReportsPage::describe(const ReportRange& range) const {
    QString text;
    if (range.isWholeMonths()) {
        text = range.months() == 1 ? QString("1 month") : QString("%1 months").arg(range.months());
    } else {
        text = range.days() == 1 ? QString("1 day") : QString("%1 days").arg(range.days());
    }
    const QDate now = today();
    if (range.first <= now && now <= range.last) text += ", to today";
    return text;
}

void ReportsPage::updateIcons() {
    backLink->setIcon(Icons::icon("chevron-left", Theme::colors().accent));
    setTextColor(backLink, Theme::colors().accent);
    const bool centred = Settings::instance().isReportsCentred();
    widthButton->setIcon(Icons::icon(centred ? "maximize" : "minimize", Theme::colors().textPrimary));
    widthButton->setToolTip(centred ? "Use the full width" : "Back to a centred column");
    widthButton->setAccessibleName(widthButton->toolTip());
}

void ReportsPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && backLink) {
        updateIcons();
        for (ReportCard* card : std::as_const(cards)) card->update();
    }
    QWidget::changeEvent(event);
}
