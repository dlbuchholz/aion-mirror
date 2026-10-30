#include "reportspage.h"
#include "account.h"
#include "category.h"
#include "categorycolors.h"
#include "icons.h"
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

namespace {

const QLocale english(QLocale::English);

const QList<ReportsPage::Report> AllReports = {
    ReportsPage::Report::SpendingByCategory, ReportsPage::Report::IncomeVsSpending, ReportsPage::Report::Balance,
    ReportsPage::Report::CategoryTrend, ReportsPage::Report::TopPayees, ReportsPage::Report::BudgetVsActual,
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
        if (ratio > 0.0) {
            QPainterPath bar;
            bar.addRoundedRect(QRectF(0, 26, qMax(4.0, nameWidth * qMin(ratio, 1.0)), 10), 3, 3);
            painter.fillPath(bar, barColor(categoryColor));
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
                                                                          {flow.spendingCents, colors.textMuted}}) {
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
        for (const auto& [label, color] : QList<QPair<QString, QColor>>{{"Spending", colors.textMuted}, {"Income", colors.positive}}) {
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
        valueLabel = new QLabel(this);
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
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        if (underMouse()) painter.fillPath(path, colors.itemHover);
        painter.setPen(QPen(hasFocus() ? colors.accent : colors.sidebarBorder, hasFocus() ? 2 : 1));
        painter.drawPath(path);
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

private:
    QLabel* titleLabel = nullptr;
    QLabel* valueLabel = nullptr;
    QLabel* detailLabel = nullptr;
    QWidget* previewArea = nullptr;
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

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
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

    auto* choices = new QHBoxLayout();
    choices->setSpacing(6);
    rangeGroup = new QButtonGroup(this);
    rangeGroup->setExclusive(true);
    addChip("Month", ReportRange::Preset::Month, choices);
    addChip("3 months", ReportRange::Preset::ThreeMonths, choices);
    addChip("6 months", ReportRange::Preset::SixMonths, choices);
    addChip("Year", ReportRange::Preset::Year, choices);
    addChip("Custom…", ReportRange::Preset::Custom, choices)->setToolTip("Choose the first and last day");
    connect(rangeGroup, &QButtonGroup::idClicked, this, &ReportsPage::onRangeClicked);
    choices->addSpacing(8);
    accountCombo = new QComboBox(this);
    accountCombo->setObjectName("reportsAccountCombo");
    accountCombo->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    connect(accountCombo, &QComboBox::activated, this, [this]() {
        shownAccountId = accountCombo->currentData().toInt();
        refresh();
    });
    choices->addWidget(accountCombo);
    header->addLayout(choices);
    header->setAlignment(choices, Qt::AlignTop);
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
    payeePreview = new QLabel(payeeCard->preview());
    payeePreview->setObjectName("payeePreview");
    payeePreview->setForegroundRole(QPalette::PlaceholderText);
    payeeCard->preview()->layout()->addWidget(payeePreview);
    payeeCard->preview()->layout()->setAlignment(payeePreview, Qt::AlignTop);
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
        setRange(start, ReportRange::forPreset(start, QDate::currentDate()));
    }
    showOverview();
    updateIcons();
}

QString ReportsPage::title(Report report) {
    switch (report) {
    case Report::SpendingByCategory: return "Spending by category";
    case Report::IncomeVsSpending: return "Income vs. spending";
    case Report::Balance: return "Balance over time";
    case Report::CategoryTrend: return "Category trend";
    case Report::TopPayees: return "Top payees";
    case Report::BudgetVsActual: return "Budget vs. actual";
    }
    return QString();
}

QToolButton* ReportsPage::addChip(const QString& text, ReportRange::Preset chipPreset, QLayout* layout) {
    auto* chip = new QToolButton(this);
    chip->setObjectName("reportsRange_" + ReportRange::key(chipPreset));
    chip->setText(text);
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    Theme::highlightWhenChecked(chip);
    rangeGroup->addButton(chip, static_cast<int>(chipPreset));
    layout->addWidget(chip);
    return chip;
}

void ReportsPage::onRangeClicked(int presetId) {
    const auto clicked = static_cast<ReportRange::Preset>(presetId);
    if (clicked != ReportRange::Preset::Custom) {
        setRange(clicked, ReportRange::forPreset(clicked, QDate::currentDate()));
        return;
    }
    // Starts from what's shown; cancelling keeps it
    QDate from = shownRange.first;
    QDate to = shownRange.last;
    if (!askCustomRange(from, to)) {
        rangeGroup->button(static_cast<int>(preset))->setChecked(true);
        return;
    }
    setRange(ReportRange::Preset::Custom, ReportRange::custom(from, to));
}

void ReportsPage::setRange(ReportRange::Preset newPreset, const ReportRange& range) {
    preset = newPreset;
    shownRange = range;
    rangeGroup->button(static_cast<int>(preset))->setChecked(true);
    Settings& settings = Settings::instance();
    settings.setReportsRange(ReportRange::key(preset));
    if (preset == ReportRange::Preset::Custom) settings.setReportsCustomDays(range.first, range.last);
    refresh();
}

void ReportsPage::setUserId(int newUserId) {
    userId = newUserId;
    shownAccountId = ALL_ACCOUNTS;
    reload();
}

void ReportsPage::reload() {
    // The month ranges move on with the date
    if (preset != ReportRange::Preset::Custom) {
        shownRange = ReportRange::forPreset(preset, QDate::currentDate());
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
    refresh();
}

void ReportsPage::refresh() {
    updateHeader();
    if (userId <= 0) return;
    spending = Reports::spendingByCategory(userId, shownAccountId, shownRange, QDate::currentDate());
    flow = Reports::incomeVsSpending(userId, shownAccountId, shownRange);
    balance = Reports::balanceOverTime(userId, shownAccountId, shownRange, QDate::currentDate());
    payees = Reports::topPayees(userId, shownAccountId, shownRange);
    if (const std::optional<int> category = trendCategory()) {
        trend = Reports::categoryTrend(userId, shownAccountId, *category, shownRange, QDate::currentDate());
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
    if (userId > 0) trend = Reports::categoryTrend(userId, shownAccountId, categoryId, shownRange, QDate::currentDate());
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
                               QString("%1 vs. %2").arg(change, shownRange.comparisonText(QDate::currentDate())),
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
                                                     english.toString(since, since.year() == QDate::currentDate().year() ? "d MMMM" : "d MMMM yyyy")),
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
    } else {
        auto* placeholder = new QLabel("This report isn't built yet.", parent);
        placeholder->setObjectName("reportsDetailPlaceholder");
        placeholder->setForegroundRole(QPalette::PlaceholderText);
        detailContent = placeholder;
    }
    detailLayout->insertWidget(0, detailContent);
}

QWidget* ReportsPage::spendingDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("spendingDetail");
    widget->setMaximumWidth(960);
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
    widget->setMaximumWidth(960);
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
    widget->setMaximumWidth(960);
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
    widget->setMaximumWidth(960);
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

QWidget* ReportsPage::trendDetail(QWidget* parent) {
    auto* widget = new QWidget(parent);
    widget->setObjectName("trendDetail");
    widget->setMaximumWidth(960);
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
    QString rangeText = shownRange.label();
    if (opened == Report::SpendingByCategory) rangeText += " · vs. " + shownRange.comparisonText(QDate::currentDate());
    rangeLabel->setText(rangeText);
}

void ReportsPage::updateIcons() {
    backLink->setIcon(Icons::icon("chevron-left", Theme::colors().accent));
    setTextColor(backLink, Theme::colors().accent);
}

void ReportsPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && backLink) {
        updateIcons();
        for (ReportCard* card : std::as_const(cards)) card->update();
    }
    QWidget::changeEvent(event);
}
