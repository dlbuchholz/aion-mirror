#include "reportspage.h"
#include "account.h"
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
    detailPlaceholder = new QLabel("This report isn't built yet.", detail);
    detailPlaceholder->setObjectName("reportsDetailPlaceholder");
    detailPlaceholder->setForegroundRole(QPalette::PlaceholderText);
    detailLayout->addWidget(detailPlaceholder);
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
}

void ReportsPage::openReport(Report report) {
    opened = report;
    stack->setCurrentIndex(1);
    updateHeader();
}

void ReportsPage::showOverview() {
    opened.reset();
    stack->setCurrentIndex(0);
    updateHeader();
}

std::optional<ReportsPage::Report> ReportsPage::openedReport() const {
    return opened;
}

void ReportsPage::updateHeader() {
    backLink->setVisible(opened.has_value());
    titleLabel->setText(opened ? title(*opened) : QString("Reports"));
    rangeLabel->setText(shownRange.label());
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
