#include "periodpicker.h"
#include "applocale.h"
#include "icons.h"
#include "theme.h"
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {


ReportRange wholeMonth(const QDate& day) {
    const QDate first(day.year(), day.month(), 1);
    return ReportRange{first, first.addMonths(1).addDays(-1)};
}

QToolButton* arrowButton(const QString& name, const QString& toolTip, QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setObjectName(name);
    button->setToolTip(toolTip);
    button->setAutoRaise(true);
    button->setCursor(Qt::PointingHandCursor);
    return button;
}

}

PeriodPicker::PeriodPicker(QWidget* parent) : QFrame(parent) {
    setProperty("stepper", true);                    // Theme::controlStyleSheet draws it as one box
    setFixedHeight(Theme::controlHeight);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(1, 1, 1, 1);          // Inside its border
    layout->setSpacing(0);
    const int inner = Theme::controlHeight - 2;

    previousButton = arrowButton("previousPeriod", tr("The period before"), this);
    previousButton->setFixedSize(inner, inner);
    connect(previousButton, &QToolButton::clicked, this, [this]() { choose(stepped(shown, -1)); });
    nameButton = new QToolButton(this);
    nameButton->setObjectName("periodName");
    nameButton->setProperty("stepperName", true);
    nameButton->setFixedHeight(inner);
    nameButton->setCursor(Qt::PointingHandCursor);
    nameButton->setToolTip(tr("Choose a period"));
    updateNameWidth();
    menu = new PeriodMenu(this);
    connect(nameButton, &QToolButton::clicked, this, [this]() {
        if (chooser) {
            if (const std::optional<ReportRange> chosen = chooser(shown)) choose(*chosen);
            return;
        }
        menu->today = today;
        menu->futureAllowed = futureAllowed;
        menu->showBelow(nameButton, shown);
    });
    connect(menu, &PeriodMenu::chosen, this, &PeriodPicker::choose);
    nextButton = arrowButton("nextPeriod", tr("The period after"), this);
    nextButton->setFixedSize(inner, inner);
    connect(nextButton, &QToolButton::clicked, this, [this]() { choose(stepped(shown, 1)); });

    layout->addWidget(previousButton);
    layout->addWidget(nameButton);
    layout->addWidget(nextButton);
    updateIcons();
    refresh();
}

void PeriodPicker::setPeriod(const ReportRange& range) {
    shown = range;
    refresh();
}

void PeriodPicker::choose(const ReportRange& range) {
    setPeriod(range);
    emit periodChanged(range);
}

void PeriodPicker::setMonthsOnly(bool monthsOnly) {
    menu->setMonthsOnly(monthsOnly);
}

void PeriodPicker::setWidestName(const QString& name) {
    widestName = name;
    updateNameWidth();
}

ReportRange PeriodPicker::stepped(const ReportRange& range, int direction) {
    if (!range.isValid()) return range;
    if (range.isWholeMonths()) {
        const QDate first = range.first.addMonths(range.months() * direction);
        return ReportRange{first, first.addMonths(range.months()).addDays(-1)};
    }
    return ReportRange{range.first.addDays(range.days() * direction), range.last.addDays(range.days() * direction)};
}

QString PeriodPicker::nameOf(const ReportRange& range) {
    if (!range.isValid()) return tr("All time");
    if (range.first == QDate(range.first.year(), 1, 1) && range.last == QDate(range.first.year(), 12, 31)) {
        return QString::number(range.first.year());
    }
    return range.label();
}

void PeriodPicker::refresh() {
    nameButton->setText(nameOf(shown));
    previousButton->setEnabled(shown.isValid());
    // Not into the future, unless that's allowed
    nextButton->setEnabled(shown.isValid() && (futureAllowed || stepped(shown, 1).first <= today()));
}

// Room for the longest month, so the arrows stay put while stepping through months. A report's
// days can take more, up to the longest name a range can have.
void PeriodPicker::updateNameWidth() {
    const auto widthFor = [this](const QString& name) {
        nameButton->setText(name);
        return nameButton->sizeHint().width() + 8;
    };
    int widest = widthFor(tr("All time"));
    for (int month = 1; month <= 12; ++month) {
        widest = qMax(widest, widthFor(AppLocale::monthName(month) + " 0000"));
    }
    if (!widestName.isEmpty()) widest = qMax(widest, widthFor(widestName));
    nameButton->setMinimumWidth(widest);
    nameButton->setMaximumWidth(widthFor("September 0000 – December 0000"));
    nameButton->setText(nameOf(shown));
}

void PeriodPicker::updateIcons() {
    previousButton->setIcon(Icons::icon("chevron-left", Theme::colors().textSecondary));
    nextButton->setIcon(Icons::icon("chevron-right", Theme::colors().textSecondary));
}

void PeriodPicker::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange) {
        updateIcons();
    }
    if (event->type() == QEvent::StyleChange && nameButton) {
        updateNameWidth();                           // A style sheet's padding changes what fits
    }
    QFrame::changeEvent(event);
}

PeriodMenu::PeriodMenu(QWidget* parent) : QFrame(parent, Qt::Popup | Qt::FramelessWindowHint) {
    setObjectName("periodMenu");
    setAttribute(Qt::WA_TranslucentBackground);      // Or the window shows square behind the round corners
    // The rounded box is a child: style sheets don't paint the background of a translucent window
    auto* box = new QFrame(this);
    box->setProperty("popup", true);                 // Theme::controlStyleSheet rounds it
    auto* window = new QVBoxLayout(this);
    window->setContentsMargins(0, 0, 0, 0);
    window->addWidget(box);
    auto* root = new QVBoxLayout(box);               // Widgets added below move into the box
    root->setContentsMargins(7, 7, 7, 9);
    root->setSpacing(1);

    const QList<QPair<QString, std::function<ReportRange(const QDate&)>>> shortcuts = {
        {tr("All time"), [](const QDate&) { return ReportRange(); }},
        {tr("This month"), [](const QDate& day) { return wholeMonth(day); }},
        {tr("Last month"), [](const QDate& day) { return wholeMonth(day.addMonths(-1)); }},
        {tr("This year"), [](const QDate& day) { return ReportRange{QDate(day.year(), 1, 1), QDate(day.year(), 12, 31)}; }},
    };
    for (int i = 0; i < shortcuts.size(); ++i) {
        const auto& shortcut = shortcuts.at(i);
        auto* button = new QPushButton(shortcut.first, this);   // Left-aligned, which a QToolButton can't be
        button->setFlat(true);
        button->setCursor(Qt::PointingHandCursor);
        const auto range = shortcut.second;
        connect(button, &QPushButton::clicked, this, [this, range]() { choose(range(today())); });
        root->addWidget(button);
        if (i == 0 || i == 3) longerPeriods.append(button);     // All time and this year
    }

    separator = new QFrame(this);
    separator->setObjectName("divider");
    separator->setFixedHeight(1);
    root->addSpacing(5);
    root->addWidget(separator);
    root->addSpacing(5);

    auto* header = new QHBoxLayout();
    previousButton = arrowButton("previousYear", tr("Previous year"), this);
    connect(previousButton, &QToolButton::clicked, this, [this]() { showYear(year - 1); });
    yearLabel = new QLabel(this);
    yearLabel->setObjectName("yearLabel");
    yearLabel->setAlignment(Qt::AlignCenter);
    QFont bold = yearLabel->font();
    bold.setBold(true);
    yearLabel->setFont(bold);
    nextButton = arrowButton("nextYear", tr("Next year"), this);
    connect(nextButton, &QToolButton::clicked, this, [this]() { showYear(year + 1); });
    header->addWidget(previousButton);
    header->addWidget(yearLabel, 1);
    header->addWidget(nextButton);
    root->addLayout(header);

    auto* grid = new QGridLayout();
    grid->setContentsMargins(4, 0, 4, 0);
    grid->setSpacing(4);
    for (int month = 1; month <= 12; ++month) {
        auto* button = new QToolButton(this);
        button->setObjectName(QString("month%1").arg(month));
        button->setText(AppLocale::monthName(month, QLocale::ShortFormat));
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumWidth(52);
        button->setFixedHeight(30);
        connect(button, &QToolButton::clicked, this, [this, month]() { choose(wholeMonth(QDate(year, month, 1))); });
        grid->addWidget(button, (month - 1) / 4, (month - 1) % 4);
        monthButtons.append(button);
    }
    root->addLayout(grid);
}

void PeriodMenu::showBelow(QWidget* anchor, const ReportRange& shownPeriod) {
    current = shownPeriod;
    // Tinted when opened, so they follow a theme change
    previousButton->setIcon(Icons::icon("chevron-left", Theme::colors().textSecondary));
    nextButton->setIcon(Icons::icon("chevron-right", Theme::colors().textSecondary));
    showYear(current.isValid() ? current.last.year() : today().year());
    adjustSize();
    move(anchor->mapToGlobal(QPoint(0, anchor->height() + 4)));
    show();
}

void PeriodMenu::showYear(int newYear) {
    year = newYear;
    yearLabel->setText(QString::number(year));
    const QDate now = today();
    nextButton->setEnabled(futureAllowed || year < now.year());   // Nothing to choose in later years, unless allowed
    for (int month = 1; month <= 12; ++month) {
        QToolButton* button = monthButtons[month - 1];
        button->setEnabled(futureAllowed || QDate(year, month, 1) <= now);
        button->setChecked(current == wholeMonth(QDate(year, month, 1)));
        const bool thisMonth = year == now.year() && month == now.month();
        if (button->property("thisMonth").toBool() != thisMonth) {
            button->setProperty("thisMonth", thisMonth);       // Outlined by the style sheet
            button->style()->unpolish(button);
            button->style()->polish(button);
        }
    }
}

void PeriodMenu::setMonthsOnly(bool monthsOnly) {
    for (QAbstractButton* button : std::as_const(longerPeriods)) button->setVisible(!monthsOnly);
}

void PeriodMenu::choose(const ReportRange& range) {
    hide();
    emit chosen(range);
}
