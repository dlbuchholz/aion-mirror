#include "reportspage.h"
#include "account.h"
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
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

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

// The colour of a category's bar, or a neutral one when category colours are off in overviews
QColor barColor(const QString& categoryColor) {
    if (!Settings::instance().isCategoryColorsInOverviewsEnabled()) return Theme::colors().textMuted;
    return CategoryColors::swatch(categoryColor).solid;
}

// A category in the spending report: its name above a bar, then spent, share and change in
// columns. The same columns make the heading and the total.
class SpendingRow : public QWidget {
public:
    enum class Kind { Heading, Category, Total };
    static constexpr int SpentWidth = 130;
    static constexpr int ShareWidth = 64;
    static constexpr int ChangeWidth = 80;

    SpendingRow(Kind rowKind, const QString& name, const QString& spent, const QString& share, const QString& change, QWidget* parent)
        : QWidget(parent), kind(rowKind), columns({name, spent, share, change}) {
        setObjectName(kind == Kind::Heading ? "spendingHeading" : kind == Kind::Total ? "spendingTotal" : "spendingRow");
        setFixedHeight(kind == Kind::Heading ? 24 : kind == Kind::Total ? 36 : 44);
        setAccessibleName(name);
        if (kind != Kind::Heading) {
            setAccessibleDescription(QString("%1, %2 of all spending, %3").arg(spent, share, change));
        }
    }

    double ratio = 0.0;                              // Bar length against the largest; none at 0 or below
    QString categoryColor;
    int direction = 0;                               // Of the change: 1 more spent, -1 less

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        if (kind != Kind::Heading) {
            painter.setPen(colors.sidebarBorder);
            painter.drawLine(QPointF(0, 0.5), QPointF(width(), 0.5));
        }
        QFont textFont = font();
        if (kind == Kind::Total) textFont.setWeight(QFont::DemiBold);
        if (kind == Kind::Heading) textFont.setPointSizeF(font().pointSizeF() * 0.9);
        painter.setFont(textFont);
        const QFontMetrics metrics(textFont);
        const QColor normal = kind == Kind::Heading ? colors.textSecondary : colors.textPrimary;

        // The numbers, right-aligned in their columns
        int right = width();
        const QList<QPair<int, int>> numberColumns = {{3, ChangeWidth}, {2, ShareWidth}, {1, SpentWidth}};
        for (const auto& [index, columnWidth] : numberColumns) {
            const bool isChange = index == 3 && kind != Kind::Heading;
            painter.setPen(isChange ? changeColor(direction) : index == 2 && kind != Kind::Heading ? colors.textSecondary : normal);
            painter.drawText(QRect(right - columnWidth, 0, columnWidth, height()), Qt::AlignRight | Qt::AlignVCenter, columns[index]);
            right -= columnWidth;
        }

        // The name, above its bar for categories
        const int nameWidth = qMax(0, right - 24);
        const QString name = metrics.elidedText(columns[0], Qt::ElideRight, nameWidth);
        painter.setPen(normal);
        if (kind != Kind::Category) {
            painter.drawText(QRect(0, 0, nameWidth, height()), Qt::AlignLeft | Qt::AlignVCenter, name);
            return;
        }
        painter.drawText(QRect(0, 4, nameWidth, 20), Qt::AlignLeft | Qt::AlignVCenter, name);
        if (ratio > 0.0) {
            QPainterPath bar;
            bar.addRoundedRect(QRectF(0, 26, qMax(4.0, nameWidth * qMin(ratio, 1.0)), 10), 3, 3);
            painter.fillPath(bar, barColor(categoryColor));
        }
    }

private:
    Kind kind;
    QStringList columns;                             // Name, spent, share, change
};

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
    updateCards();
    updateDetail();
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
}

void ReportsPage::updateDetail() {
    delete detailContent;
    detailContent = nullptr;
    if (!opened) return;
    QWidget* parent = detailLayout->parentWidget();
    if (*opened == Report::SpendingByCategory) {
        detailContent = spendingDetail(parent);
    } else {
        auto* placeholder = new QLabel("This report isn't built yet.", parent);
        placeholder->setObjectName("reportsDetailPlaceholder");
        placeholder->setForegroundRole(QPalette::PlaceholderText);
        detailContent = placeholder;
    }
    detailLayout->insertWidget(0, detailContent);
}

QWidget* ReportsPage::spendingDetail(QWidget* parent) const {
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

    const Settings& settings = Settings::instance();
    layout->addWidget(new SpendingRow(SpendingRow::Kind::Heading, "Category", "Spent", "Share", "Change", widget));
    // Refunded categories (below zero) come last and get no bar
    const double largest = double(qMax<Cents>(1, spending.categories.first().spentCents));
    for (const CategorySpent& spent : spending.categories) {
        int direction = 0;
        const QString change = changeText(spent.spentCents, spent.beforeCents, direction);
        auto* row = new SpendingRow(SpendingRow::Kind::Category, spent.categoryName, settings.formatSpending(spent.spentCents),
                                    shareText(spent.spentCents, spending.totalCents), change, widget);
        row->ratio = double(spent.spentCents) / largest;
        row->categoryColor = spent.categoryColor;
        row->direction = direction;
        row->setToolTip(spent.beforeCents != 0 ? QString("%1 in %2").arg(settings.formatSpending(spent.beforeCents),
                                                                            spending.before.label())
                                               : QString("Nothing in %1").arg(spending.before.label()));
        layout->addWidget(row);
    }
    int direction = 0;
    const QString change = changeText(spending.totalCents, spending.beforeTotalCents, direction);
    auto* total = new SpendingRow(SpendingRow::Kind::Total, "Total", settings.formatSpending(spending.totalCents),
                                  spending.totalCents > 0 ? QString("100%") : QString("—"), change, widget);
    total->direction = direction;
    total->setToolTip(QString("%1 in %2").arg(settings.formatSpending(spending.beforeTotalCents), spending.before.label()));
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
