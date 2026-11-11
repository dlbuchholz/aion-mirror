#include "budgetcharts.h"
#include "applocale.h"
#include "categorycolors.h"
#include "settings.h"
#include "theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSet>
#include <QToolTip>
#include <cmath>

namespace {


QString percentText(Cents spent, Cents limit) {
    return QString("%1%").arg(limit > 0 ? qRound(double(spent) * 100.0 / double(limit)) : 0);
}

// The unused palette colour furthest in hue from those in use; muted ones last
QString furthestColor(const QStringList& palette, const QSet<QString>& used) {
    QString best;
    double bestDistance = -1;
    for (const QString& name : palette) {
        if (used.contains(name)) continue;
        const QColor color = CategoryColors::swatch(name).solid;
        double distance = 1.0;
        for (const QString& other : used) {
            const double apart = std::abs(color.hslHueF() - CategoryColors::swatch(other).solid.hslHueF());
            distance = qMin(distance, qMin(apart, 1.0 - apart) * 2.0);
        }
        distance *= color.hslSaturationF();
        if (distance > bestDistance) {
            best = name;
            bestDistance = distance;
        }
    }
    return best.isEmpty() ? palette.value(used.size() % qMax(1, int(palette.size()))) : best;   // All taken: repeat
}

double distanceToSegment(const QPointF& point, const QPointF& from, const QPointF& to) {
    const QPointF segment = to - from;
    const double lengthSquared = QPointF::dotProduct(segment, segment);
    const double t = lengthSquared > 0 ? qBound(0.0, QPointF::dotProduct(point - from, segment) / lengthSquared, 1.0) : 0.0;
    const QPointF nearest = from + t * segment;
    return std::hypot(point.x() - nearest.x(), point.y() - nearest.y());
}

QFont smallFont(const QWidget* widget) {
    QFont font = widget->font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    return font;
}

// The limit (100%) and the ground as plain lines, the limit's name on the left
void paintShareFrame(QPainter& painter, const QRectF& plot, double limitY, double groundY) {
    const Theme::Colors colors = Theme::colors();
    painter.setPen(colors.sidebarBorder);
    painter.drawLine(QPointF(plot.left(), limitY), QPointF(plot.right(), limitY));
    painter.drawLine(QPointF(plot.left(), groundY), QPointF(plot.right(), groundY));
    painter.setPen(colors.textSecondary);
    painter.drawText(QRectF(0, limitY - 8, plot.left() - 8, 16), Qt::AlignRight | Qt::AlignVCenter, "100%");
}

QColor faded(QColor color, bool fade) {
    if (fade) color.setAlphaF(0.3f);
    return color;
}

} // namespace

QList<QColor> budgetLineColors(const QStringList& categoryColors) {
    const bool own = Settings::instance().isCategoryColorsInOverviewsEnabled();
    const QStringList palette = CategoryColors::names();
    QSet<QString> used;
    if (own) {
        for (const QString& name : categoryColors) {
            if (!name.isEmpty()) used.insert(name);
        }
    }
    QList<QColor> result;
    for (const QString& categoryColor : categoryColors) {
        QString name = own ? categoryColor : QString();
        if (name.isEmpty()) {
            name = furthestColor(palette, used);
            used.insert(name);
        }
        result.append(CategoryColors::swatch(name).solid);
    }
    return result;
}

QString budgetShareText(const QString& name, const QString& when, Cents spentCents, Cents limitCents) {
    const Settings& settings = Settings::instance();
    return BudgetLinesChart::tr("%1, %2: %3 of %4 (%5)").arg(name, when, settings.formatSpending(spentCents),
                                                settings.formatCurrency(limitCents), percentText(spentCents, limitCents));
}

// ---- BudgetLinesChart ----

BudgetLinesChart::BudgetLinesChart(QWidget* parent)
    : QWidget(parent) {
    setObjectName("budgetLinesChart");
    setMouseTracking(true);
    setMinimumHeight(200);
}

void BudgetLinesChart::setLines(const ReportRange& newMonth, const QList<Line>& newLines) {
    month = newMonth;
    lines = newLines;
    hovered = -1;
    lowest = 0;
    highest = 100;
    for (int line = 0; line < lines.size(); ++line) {
        for (int day = 0; day <= lastDay(); ++day) {
            lowest = qMin(lowest, percentOn(line, day));
            highest = qMax(highest, percentOn(line, day));
        }
    }
    highest += (highest - lowest) / 10;              // Room above the top
    update();
}

int BudgetLinesChart::lastDay() const {
    int last = 0;
    for (const Line& line : lines) last = qMax(last, int(line.spentByDay.size()));
    return last;
}

double BudgetLinesChart::percentOn(int line, int day) const {
    const Line& budget = lines[line];
    if (budget.limitCents <= 0) return 0.0;
    return double(budget.spentByDay.value(day - 1)) * 100.0 / double(budget.limitCents);   // Day 0: value() gives 0
}

QRectF BudgetLinesChart::plot() const {
    const QFontMetrics metrics(smallFont(this));
    return QRectF(rect()).adjusted(metrics.horizontalAdvance("100%") + 8, 8, -8, -24);
}

double BudgetLinesChart::xFor(double day) const {
    const QRectF area = plot();
    return area.left() + day / qMax<qint64>(1, month.days()) * area.width();
}

double BudgetLinesChart::yFor(double percent) const {
    const QRectF area = plot();
    return area.bottom() - (percent - lowest) / (highest - lowest) * area.height();
}

QPointF BudgetLinesChart::pointOn(int line, int day) const {
    return QPointF(xFor(day), yFor(percentOn(line, day)));
}

int BudgetLinesChart::dayAt(const QPointF& position) const {
    const QRectF area = plot();
    const int day = qRound((position.x() - area.left()) / area.width() * double(month.days()));
    return qBound(1, day, qMax(1, lastDay()));
}

int BudgetLinesChart::lineAt(const QPointF& position) const {
    int nearest = -1;
    double nearestDistance = 6.0;                    // Pixels from the line
    for (int line = 0; line < lines.size(); ++line) {
        for (int day = 1; day <= lastDay(); ++day) {
            const double distance = distanceToSegment(position, pointOn(line, day - 1), pointOn(line, day));
            if (distance < nearestDistance) {
                nearest = line;
                nearestDistance = distance;
            }
        }
    }
    return nearest;
}

QString BudgetLinesChart::hoverText(const QPointF& position) const {
    const int line = lineAt(position);
    if (line < 0) return QString();
    const int day = dayAt(position);
    const Line& budget = lines[line];
    return budgetShareText(budget.name, AppLocale::date(month.first.addDays(day - 1), AppLocale::DateFormat::ShortDayMonth),
                           budget.spentByDay.value(day - 1), budget.limitCents);
}

void BudgetLinesChart::paintEvent(QPaintEvent*) {
    const Theme::Colors colors = Theme::colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(smallFont(this));
    const QRectF area = plot();
    const int days = int(month.days());
    if (days < 1) return;

    paintShareFrame(painter, area, yFor(100), yFor(0));
    painter.setPen(QPen(colors.textSecondary, 1, Qt::DashLine));            // Even pace
    painter.drawLine(QPointF(xFor(0), yFor(0)), QPointF(xFor(days), yFor(100)));
    painter.setPen(colors.textSecondary);
    const QList<std::pair<int, QString>> labels = {{0, AppLocale::date(month.first, AppLocale::DateFormat::ShortDayMonth)},
                                                   {days / 2, QString::number(days / 2)},
                                                   {days, QString::number(days)}};
    for (const auto& [day, label] : labels) {
        painter.drawText(QRectF(xFor(day) - 40, area.bottom() + 4, 80, 18), Qt::AlignCenter, label);
    }

    // The hovered budget on top, the others faded
    const auto pathOf = [this](int line) {
        QPainterPath path(pointOn(line, 0));
        for (int day = 1; day <= lastDay(); ++day) path.lineTo(pointOn(line, day));
        return path;
    };
    painter.setBrush(Qt::NoBrush);
    for (int line = 0; line < lines.size(); ++line) {
        if (line == hovered) continue;
        painter.setPen(QPen(faded(lines[line].color, hovered >= 0), 2));
        painter.drawPath(pathOf(line));
    }
    if (hovered >= 0) {
        painter.setPen(QPen(lines[hovered].color, 3));
        painter.drawPath(pathOf(hovered));
    }
}

void BudgetLinesChart::mouseMoveEvent(QMouseEvent* event) {
    const int line = lineAt(event->position());
    if (line != hovered) {
        hovered = line;
        update();
    }
    line >= 0 ? setCursor(Qt::PointingHandCursor) : unsetCursor();
    const QString text = hoverText(event->position());
    text.isEmpty() ? QToolTip::hideText() : QToolTip::showText(event->globalPosition().toPoint(), text, this);
    QWidget::mouseMoveEvent(event);
}

void BudgetLinesChart::mouseReleaseEvent(QMouseEvent* event) {
    const int line = event->button() == Qt::LeftButton ? lineAt(event->position()) : -1;
    if (line >= 0) {
        emit budgetClicked(lines[line].categoryId);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void BudgetLinesChart::leaveEvent(QEvent* event) {
    hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

// ---- BudgetMonthsChart ----

BudgetMonthsChart::BudgetMonthsChart(QWidget* parent)
    : QWidget(parent) {
    setObjectName("budgetMonthsChart");
    setMouseTracking(true);
    setMinimumHeight(200);
}

void BudgetMonthsChart::setLines(const QList<ReportRange>& newMonths, const QList<Line>& newLines) {
    months = newMonths;
    lines = newLines;
    hovered = -1;
    highest = 100;
    for (const Line& line : lines) {
        for (const BudgetMonth& budget : line.byMonth) {
            if (budget.limitCents > 0) highest = qMax(highest, double(budget.spentCents) * 100.0 / double(budget.limitCents));
        }
    }
    highest *= 1.1;                                  // Room above the top
    update();
}

const BudgetMonth* BudgetMonthsChart::budgetIn(int line, int month) const {
    for (const BudgetMonth& budget : lines[line].byMonth) {
        if (budget.month.first == months[month].first) return &budget;
    }
    return nullptr;
}

QRectF BudgetMonthsChart::plot() const {
    const QFontMetrics metrics(smallFont(this));
    return QRectF(rect()).adjusted(metrics.horizontalAdvance("100%") + 8, 8, -8, -24);
}

double BudgetMonthsChart::xFor(int month) const {
    const QRectF area = plot();
    return area.left() + (month + 0.5) * area.width() / qMax(1, int(months.size()));
}

double BudgetMonthsChart::yFor(double percent) const {
    const QRectF area = plot();
    return area.bottom() - qMax(0.0, percent) / highest * area.height();   // Money back stays on the ground
}

std::optional<QPointF> BudgetMonthsChart::pointOn(int line, int month) const {
    const BudgetMonth* budget = budgetIn(line, month);
    if (!budget || budget->limitCents <= 0) return std::nullopt;
    return QPointF(xFor(month), yFor(double(budget->spentCents) * 100.0 / double(budget->limitCents)));
}

std::pair<int, int> BudgetMonthsChart::pointAt(const QPointF& position) const {
    std::pair<int, int> nearest{-1, -1};
    double nearestDistance = 8.0;                    // Pixels from the point
    for (int line = 0; line < lines.size(); ++line) {
        for (int month = 0; month < months.size(); ++month) {
            const std::optional<QPointF> point = pointOn(line, month);
            if (!point) continue;
            const double distance = std::hypot(point->x() - position.x(), point->y() - position.y());
            if (distance < nearestDistance) {
                nearest = {line, month};
                nearestDistance = distance;
            }
        }
    }
    return nearest;
}

QString BudgetMonthsChart::hoverText(const QPointF& position) const {
    const auto [line, month] = pointAt(position);
    if (line < 0) return QString();
    const BudgetMonth* budget = budgetIn(line, month);
    return budgetShareText(lines[line].name, AppLocale::date(months[month].first, AppLocale::DateFormat::ShortMonthYear), budget->spentCents,
                           budget->limitCents);
}

void BudgetMonthsChart::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setFont(smallFont(this));
    const QRectF area = plot();
    paintShareFrame(painter, area, yFor(100), yFor(0));
    painter.setPen(Theme::colors().textSecondary);
    for (int month = 0; month < months.size(); ++month) {
        painter.drawText(QRectF(xFor(month) - 40, area.bottom() + 4, 80, 18), Qt::AlignCenter,
                         AppLocale::date(months[month].first, AppLocale::DateFormat::ShortMonth));
    }

    // Lines between months in a row that both had the budget; the hovered one on top
    const auto paintLine = [&](int line, bool fade, bool strong) {
        const QColor color = faded(lines[line].color, fade);
        painter.setPen(QPen(color, strong ? 3 : 2));
        for (int month = 1; month < months.size(); ++month) {
            const std::optional<QPointF> from = pointOn(line, month - 1);
            const std::optional<QPointF> to = pointOn(line, month);
            if (from && to) painter.drawLine(*from, *to);
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        for (int month = 0; month < months.size(); ++month) {
            if (const std::optional<QPointF> point = pointOn(line, month)) painter.drawEllipse(*point, strong ? 4.5 : 3.5, strong ? 4.5 : 3.5);
        }
        painter.setBrush(Qt::NoBrush);
    };
    for (int line = 0; line < lines.size(); ++line) {
        if (line != hovered) paintLine(line, hovered >= 0, false);
    }
    if (hovered >= 0) paintLine(hovered, false, true);
}

void BudgetMonthsChart::mouseMoveEvent(QMouseEvent* event) {
    const int line = pointAt(event->position()).first;
    if (line != hovered) {
        hovered = line;
        update();
    }
    line >= 0 ? setCursor(Qt::PointingHandCursor) : unsetCursor();
    const QString text = hoverText(event->position());
    text.isEmpty() ? QToolTip::hideText() : QToolTip::showText(event->globalPosition().toPoint(), text, this);
    QWidget::mouseMoveEvent(event);
}

void BudgetMonthsChart::mouseReleaseEvent(QMouseEvent* event) {
    const auto [line, month] = event->button() == Qt::LeftButton ? pointAt(event->position()) : std::pair{-1, -1};
    if (line >= 0) {
        emit monthClicked(lines[line].categoryId, months[month]);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void BudgetMonthsChart::leaveEvent(QEvent* event) {
    hovered = -1;
    update();
    QWidget::leaveEvent(event);
}

// ---- BudgetGrid ----

BudgetGrid::BudgetGrid(QWidget* parent)
    : QWidget(parent) {
    setObjectName("budgetGrid");
    setMouseTracking(true);
}

void BudgetGrid::setBudgets(const QList<ReportRange>& newMonths, const QList<BudgetActual>& newBudgets) {
    months = newMonths;
    budgets = newBudgets;
    setMinimumHeight(minimumSizeHint().height());
    updateGeometry();
    update();
}

QSize BudgetGrid::sizeHint() const {
    return QSize(600, RowHeight * (int(budgets.size()) + 1) + 2);
}

QSize BudgetGrid::minimumSizeHint() const {
    return QSize(nameWidth() + 48 * int(qMax<qsizetype>(1, months.size())), sizeHint().height());
}

int BudgetGrid::nameWidth() const {
    int widest = 0;
    for (const BudgetActual& budget : budgets) widest = qMax(widest, fontMetrics().horizontalAdvance(budget.categoryName));
    return qMin(180, widest + 16);
}

const BudgetMonth* BudgetGrid::budgetIn(int row, int month) const {
    for (const BudgetMonth& budget : budgets[row].byMonth) {
        if (budget.month.first == months[month].first) return &budget;
    }
    return nullptr;
}

QRect BudgetGrid::cellRect(int row, int month) const {
    const int cellWidth = (width() - nameWidth()) / qMax(1, int(months.size()));
    return QRect(nameWidth() + month * cellWidth, RowHeight * (row + 1), cellWidth, RowHeight).adjusted(2, 2, -2, -2);
}

std::pair<int, int> BudgetGrid::cellAt(const QPoint& position) const {
    for (int row = 0; row < budgets.size(); ++row) {
        for (int month = 0; month < months.size(); ++month) {
            if (cellRect(row, month).contains(position)) return {row, month};
        }
    }
    return {-1, -1};
}

QString BudgetGrid::cellText(int row, int month) const {
    const BudgetMonth* budget = budgetIn(row, month);
    return budget ? percentText(budget->spentCents, budget->limitCents) : QString("–");
}

void BudgetGrid::paintEvent(QPaintEvent*) {
    const Theme::Colors colors = Theme::colors();
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(colors.textSecondary);
    for (int month = 0; month < months.size(); ++month) {
        const QRect header = cellRect(-1, month);
        painter.drawText(header, Qt::AlignCenter, AppLocale::date(months[month].first, AppLocale::DateFormat::ShortMonth));
    }
    for (int row = 0; row < budgets.size(); ++row) {
        const QRect name(0, RowHeight * (row + 1), nameWidth() - 8, RowHeight);
        painter.setPen(colors.textPrimary);
        painter.drawText(name, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(budgets[row].categoryName, Qt::ElideRight, name.width()));
        for (int month = 0; month < months.size(); ++month) {
            const QRect cell = cellRect(row, month);
            const BudgetMonth* budget = budgetIn(row, month);
            QColor background = colors.track;
            QColor text = budget ? colors.textPrimary : colors.textMuted;
            if (budget && budget->limitCents > 0) {
                const double share = double(budget->spentCents) / double(budget->limitCents);
                if (share >= 0.9) {
                    const CategoryColors::Swatch swatch = CategoryColors::swatch(share > 1.0 ? "red" : "amber");
                    background = swatch.background;
                    text = swatch.text;
                }
            }
            QPainterPath path;
            path.addRoundedRect(QRectF(cell), 4, 4);
            painter.fillPath(path, budget ? background : Qt::transparent);
            painter.setPen(text);
            painter.drawText(cell, Qt::AlignCenter, cellText(row, month));
        }
    }
}

void BudgetGrid::mouseMoveEvent(QMouseEvent* event) {
    const auto [row, month] = cellAt(event->position().toPoint());
    const BudgetMonth* budget = row >= 0 ? budgetIn(row, month) : nullptr;
    if (budget) {
        setCursor(Qt::PointingHandCursor);
        QToolTip::showText(event->globalPosition().toPoint(),
                           budgetShareText(budgets[row].categoryName, AppLocale::date(months[month].first, AppLocale::DateFormat::ShortMonthYear),
                                           budget->spentCents, budget->limitCents),
                           this);
    } else {
        unsetCursor();
        QToolTip::hideText();
    }
    QWidget::mouseMoveEvent(event);
}

void BudgetGrid::mouseReleaseEvent(QMouseEvent* event) {
    const auto [row, month] = event->button() == Qt::LeftButton ? cellAt(event->position().toPoint()) : std::pair{-1, -1};
    if (row >= 0 && budgetIn(row, month)) {
        emit monthClicked(budgets[row].categoryId, months[month]);
        return;
    }
    QWidget::mouseReleaseEvent(event);
}
