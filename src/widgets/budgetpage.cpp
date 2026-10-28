#include "budgetpage.h"
#include "budgeteditor.h"
#include "categorycolors.h"
#include "category.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace {

QString monthName(const QDate& month) {
    return QLocale(QLocale::English).toString(month, "MMMM");
}

// Gives a push button the accent colour used for the main action of a view
void makePrimary(QPushButton* button) {
    QPalette palette = button->palette();
    palette.setColor(QPalette::Button, Theme::colors().accent);
    palette.setColor(QPalette::ButtonText, Theme::colors().onAccent);
    button->setPalette(palette);
}

// "88,00 € left" or "12,00 € over"
QString leftText(Cents left) {
    const Settings& settings = Settings::instance();
    return left < 0 ? QString("%1 over").arg(settings.formatCurrency(-left))
                    : QString("%1 left").arg(settings.formatCurrency(left));
}

// One budget: category with a "monthly" / "only <Month>" tag, spent of limit, progress bar
// and what is left. Clicking opens it in the editor.
class BudgetRow : public QAbstractButton {
public:
    BudgetRow(const MonthBudget& budget, const QDate& month, QWidget* parent)
        : QAbstractButton(parent), budget(budget),
          tag(budget.repeats ? QString("monthly") : QString("only %1").arg(monthName(month))) {
        setObjectName("budgetRow");
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::TabFocus);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(62);
        setAccessibleName(budget.categoryName);
        setAccessibleDescription(QString("%1 of %2 · %3").arg(Settings::instance().formatSpending(budget.spentCents),
                                                              Settings::instance().formatCurrency(budget.limitCents),
                                                              leftText(budget.leftCents())));
        setToolTip(budget.repeats ? QString("Regular limit since %1").arg(QLocale(QLocale::English).toString(budget.validFrom, "MMMM yyyy"))
                                  : QString("Set for %1 only").arg(monthName(month)));
    }

    int categoryId() const { return budget.categoryId; }
    void setOpen(bool isOpen) { open = isOpen; update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        const Settings& settings = Settings::instance();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        const QRect area = rect().adjusted(0, 1, 0, -1);
        if (open || underMouse() || hasFocus()) {
            QPainterPath path;
            path.addRoundedRect(QRectF(area), 6, 6);
            painter.fillPath(path, open ? colors.itemSelected : colors.itemHover);
        }
        const QRect content = area.adjusted(12, 8, -12, -10);
        const int leftWidth = 150;                    // "left" / "over" column

        // Top line: category and tag on the left, spent of limit on the right
        QFont font = this->font();
        QFont smallFont = font;
        smallFont.setPointSizeF(font.pointSizeF() * 0.9);
        const QFontMetrics metrics(font);
        const QFontMetrics smallMetrics(smallFont);
        const QRect topLine(content.left(), content.top(), content.width(), 22);

        const QString amounts = QString("%1 of %2").arg(settings.formatSpending(budget.spentCents),
                                                        settings.formatCurrency(budget.limitCents));
        painter.setFont(smallFont);
        painter.setPen(colors.textSecondary);
        painter.drawText(topLine.adjusted(0, 0, -leftWidth - 24, 0), Qt::AlignRight | Qt::AlignVCenter, amounts);
        const int amountsWidth = smallMetrics.horizontalAdvance(amounts);

        const int tagWidth = smallMetrics.horizontalAdvance(tag) + 16;
        // The category's colour as a dot before the name
        int nameLeft = topLine.left();
        if (settings.isCategoryColorsInOverviewsEnabled() && !budget.categoryColor.isEmpty()) {
            painter.setPen(Qt::NoPen);
            painter.setBrush(CategoryColors::swatch(budget.categoryColor).solid);
            painter.drawEllipse(QRectF(nameLeft, topLine.center().y() - 4, 9, 9));
            nameLeft += 17;
        }
        const int nameWidth = qMax(0, topLine.right() - nameLeft - leftWidth - amountsWidth - tagWidth - 32);
        const QString name = metrics.elidedText(budget.categoryName, Qt::ElideRight, nameWidth);
        painter.setFont(font);
        painter.setPen(colors.textPrimary);
        painter.drawText(QRect(nameLeft, topLine.top(), topLine.right() - nameLeft, topLine.height()), Qt::AlignLeft | Qt::AlignVCenter, name);

        const QRectF pill(nameLeft + metrics.horizontalAdvance(name) + 10, topLine.center().y() - 9, tagWidth, 18);
        QPainterPath pillPath;
        pillPath.addRoundedRect(pill, 9, 9);
        painter.fillPath(pillPath, colors.track);
        painter.setFont(smallFont);
        painter.setPen(colors.textSecondary);
        painter.drawText(pill, Qt::AlignCenter, tag);

        // Bottom line: progress bar and what is left
        const Cents left = budget.leftCents();
        const QRect bottomLine(content.left(), content.bottom() - 16, content.width(), 18);
        painter.setFont(font);
        painter.setPen(left < 0 ? colors.danger : colors.textPrimary);
        painter.drawText(QRect(content.right() - leftWidth, area.top(), leftWidth, area.height()),
                         Qt::AlignRight | Qt::AlignVCenter, leftText(left));

        const QRectF track(bottomLine.left(), bottomLine.center().y() - 3, bottomLine.width() - leftWidth - 24, 6);
        QPainterPath trackPath;
        trackPath.addRoundedRect(track, 3, 3);
        painter.fillPath(trackPath, colors.track);
        const double ratio = qBound(0.0, double(budget.spentCents) / double(budget.limitCents), 1.0);
        if (ratio > 0.0) {
            QRectF bar = track;
            bar.setWidth(qMax(6.0, track.width() * ratio));
            QPainterPath barPath;
            barPath.addRoundedRect(bar, 3, 3);
            // Over the limit: red; exactly used up: grey, nothing more to spend; otherwise accent
            const QColor color = left < 0 ? colors.danger : left == 0 ? colors.textMuted : colors.accent;
            painter.fillPath(barPath, color);
        }
    }

    void keyPressEvent(QKeyEvent* event) override {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            return;
        }
        QAbstractButton::keyPressEvent(event);
    }

private:
    MonthBudget budget;
    QString tag;
    bool open = false;
};

} // namespace

BudgetPage::BudgetPage(QWidget* parent)
    : QWidget(parent) {
    const QDate today = QDate::currentDate();
    shownMonth = QDate(today.year(), today.month(), 1);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 12);
    root->setSpacing(12);

    // Header: title, month navigation and the main action
    auto* header = new QHBoxLayout();
    header->setSpacing(6);
    auto* titleLabel = new QLabel("Budget", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel);
    header->addSpacing(18);

    previousButton = new QToolButton(this);
    previousButton->setObjectName("previousMonthButton");
    previousButton->setAutoRaise(true);
    previousButton->setToolTip("Previous month");
    connect(previousButton, &QToolButton::clicked, this, [this]() { showMonth(shownMonth.addMonths(-1)); });
    header->addWidget(previousButton);
    monthLabel = new QLabel(this);
    monthLabel->setObjectName("monthLabel");
    monthLabel->setAlignment(Qt::AlignCenter);
    QFont monthFont = monthLabel->font();
    monthFont.setPointSize(11);
    monthFont.setWeight(QFont::DemiBold);
    monthLabel->setFont(monthFont);
    monthLabel->setMinimumWidth(QFontMetrics(monthFont).horizontalAdvance("September 2026") + 16);
    header->addWidget(monthLabel);
    nextButton = new QToolButton(this);
    nextButton->setObjectName("nextMonthButton");
    nextButton->setAutoRaise(true);
    nextButton->setToolTip("Next month");
    connect(nextButton, &QToolButton::clicked, this, [this]() { showMonth(shownMonth.addMonths(1)); });
    header->addWidget(nextButton);
    thisMonthButton = new QPushButton("This month", this);
    thisMonthButton->setObjectName("thisMonthButton");
    connect(thisMonthButton, &QPushButton::clicked, this, [this]() { showMonth(QDate::currentDate()); });
    header->addWidget(thisMonthButton);
    header->addStretch();

    addButton = new QPushButton("Add budget", this);
    addButton->setObjectName("addBudgetButton");
    addButton->setToolTip("Add budget (Ctrl+N)");
    makePrimary(addButton);
    connect(addButton, &QPushButton::clicked, this, [this]() { addBudget(); });
    header->addWidget(addButton);
    root->addLayout(header);

    // Totals of the budgeted categories
    auto* tiles = new QHBoxLayout();
    tiles->setSpacing(12);
    tiles->addWidget(createTile("Budgeted", &budgetedLabel));
    tiles->addWidget(createTile("Spent", &spentLabel));
    QFrame* leftTile = createTile("Left", &leftLabel);
    leftCaption = leftTile->findChild<QLabel*>("tileCaption");
    tiles->addWidget(leftTile);
    root->addLayout(tiles);

    // Budgets and unbudgeted spending, with the editor on the right
    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("budgetScroll");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* content = new QWidget(scroll);
    content->setObjectName("budgetContent");
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 6, 6, 6);
    contentLayout->setSpacing(0);

    rowsLayout = new QVBoxLayout();
    rowsLayout->setSpacing(0);
    contentLayout->addLayout(rowsLayout);

    emptyLabel = new QLabel(content);
    emptyLabel->setObjectName("budgetEmptyLabel");
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    emptyLabel->setContentsMargins(12, 28, 12, 28);
    emptyLabel->setForegroundRole(QPalette::PlaceholderText);
    contentLayout->addWidget(emptyLabel);

    unbudgetedTitle = new QLabel("Spending without a budget", content);
    unbudgetedTitle->setObjectName("unbudgetedTitle");
    QFont sectionFont = unbudgetedTitle->font();
    sectionFont.setPointSizeF(sectionFont.pointSizeF() * 0.9);
    sectionFont.setWeight(QFont::DemiBold);
    unbudgetedTitle->setFont(sectionFont);
    unbudgetedTitle->setContentsMargins(12, 18, 12, 6);
    contentLayout->addWidget(unbudgetedTitle);
    unbudgetedLayout = new QVBoxLayout();
    unbudgetedLayout->setSpacing(0);
    contentLayout->addLayout(unbudgetedLayout);
    contentLayout->addStretch();
    scroll->setWidget(content);
    body->addWidget(scroll, 1);

    editor = new BudgetEditor(this);
    editor->hide();
    connect(editor, &BudgetEditor::saveRequested, this, &BudgetPage::onSaveRequested);
    connect(editor, &BudgetEditor::removeRequested, this, &BudgetPage::onRemoveRequested);
    connect(editor, &BudgetEditor::closed, this, &BudgetPage::closeEditor);
    body->addWidget(editor);
    root->addLayout(body, 1);

    auto* addShortcut = new QShortcut(QKeySequence::New, this);
    addShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(addShortcut, &QShortcut::activated, this, [this]() { addBudget(); });

    updateIcons();
    updateHeader();
}

QFrame* BudgetPage::createTile(const QString& caption, QLabel** valueLabel) {
    auto* tile = new QFrame(this);
    tile->setObjectName("budgetTile");
    tile->setFrameShape(QFrame::StyledPanel);
    auto* layout = new QVBoxLayout(tile);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(2);
    auto* captionLabel = new QLabel(caption, tile);
    captionLabel->setObjectName("tileCaption");
    captionLabel->setForegroundRole(QPalette::PlaceholderText);
    *valueLabel = new QLabel(tile);
    (*valueLabel)->setObjectName(caption.toLower() + "Total");
    QFont valueFont = (*valueLabel)->font();
    valueFont.setPointSize(15);
    (*valueLabel)->setFont(valueFont);
    layout->addWidget(captionLabel);
    layout->addWidget(*valueLabel);
    return tile;
}

void BudgetPage::updateIcons() {
    const QColor color = palette().color(QPalette::ButtonText);
    previousButton->setIcon(Icons::icon("chevron-left", color));
    nextButton->setIcon(Icons::icon("chevron-right", color));
    addButton->setIcon(Icons::icon("plus", Theme::colors().onAccent));
}

void BudgetPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && previousButton && leftLabel) {
        updateIcons();
        updateHeader();                              // "Over" colour of the left total
    }
    QWidget::changeEvent(event);
}

void BudgetPage::setUserId(int newUserId) {
    userId = newUserId;
    closeEditor();
    reload();
}

void BudgetPage::showMonth(const QDate& month) {
    shownMonth = QDate(month.year(), month.month(), 1);
    closeEditor();
    reload();
}

QDate BudgetPage::month() const {
    return shownMonth;
}

void BudgetPage::applySettings() {
    editor->applySettings();
    reload();
}

void BudgetPage::highlightRow(int categoryId) {
    for (QAbstractButton* row : budgetRows) {
        auto* budgetRow = static_cast<BudgetRow*>(row);
        budgetRow->setOpen(categoryId > 0 && budgetRow->categoryId() == categoryId);
    }
}

void BudgetPage::clearRows() {
    budgetRows.clear();
    for (QVBoxLayout* layout : {rowsLayout, unbudgetedLayout}) {
        while (QLayoutItem* item = layout->takeAt(0)) {
            delete item->widget();
            delete item;
        }
    }
}

void BudgetPage::reload() {
    if (userId == -1) {
        return;
    }
    BudgetDAO dao;
    budgets = dao.budgetsForMonth(userId, shownMonth);
    unbudgeted = dao.unbudgetedSpending(userId, shownMonth);

    clearRows();
    for (const MonthBudget& budget : budgets) {
        auto* row = new BudgetRow(budget, shownMonth, this);
        const int categoryId = budget.categoryId;
        connect(row, &QAbstractButton::clicked, this, [this, categoryId]() { editBudget(categoryId); });
        rowsLayout->addWidget(row);
        budgetRows.append(row);
    }
    highlightRow(editor->isVisible() ? editor->editedCategoryId() : 0);
    emptyLabel->setText(QString("No budgets for %1 yet.\nAdd one to keep an eye on what a category may cost each month.")
                            .arg(monthName(shownMonth)));
    emptyLabel->setVisible(budgets.isEmpty());

    // Spending without a budget, each with a quick way to add one
    const Settings& settings = Settings::instance();
    for (const UnbudgetedSpending& spending : unbudgeted) {
        auto* row = new QWidget(this);
        row->setObjectName("unbudgetedRow");
        row->setAccessibleName(spending.categoryName);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(12, 4, 6, 4);
        auto* name = new QLabel(spending.categoryName, row);
        auto* amount = new QLabel(settings.formatCurrency(spending.spentCents), row);
        amount->setObjectName("unbudgetedAmount");
        layout->addWidget(name, 1);
        layout->addWidget(amount);
        if (spending.categoryId > 0) {
            auto* add = new QToolButton(row);
            add->setObjectName("addBudgetForCategory");
            add->setText("Add budget");
            add->setAutoRaise(true);
            add->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
            add->setIcon(Icons::icon("plus", Theme::colors().accent));
            QPalette linkPalette = add->palette();
            linkPalette.setColor(QPalette::ButtonText, Theme::colors().accent);
            add->setPalette(linkPalette);
            add->setCursor(Qt::PointingHandCursor);
            // Suggest the spending so far, rounded up to a round number
            const Cents step = 1000;
            const Cents suggested = (spending.spentCents + step - 1) / step * step;
            const int categoryId = spending.categoryId;
            connect(add, &QToolButton::clicked, this, [this, categoryId, suggested]() { addBudget(categoryId, suggested); });
            layout->addSpacing(8);
            layout->addWidget(add);
        } else {
            layout->addSpacing(8);
            auto* hint = new QLabel("Give these transactions a category to budget them", row);
            hint->setForegroundRole(QPalette::PlaceholderText);
            layout->addWidget(hint);
        }
        unbudgetedLayout->addWidget(row);
    }
    unbudgetedTitle->setVisible(!unbudgeted.isEmpty());

    updateHeader();
}

void BudgetPage::updateHeader() {
    monthLabel->setText(QLocale(QLocale::English).toString(shownMonth, "MMMM yyyy"));
    const QDate today = QDate::currentDate();
    thisMonthButton->setVisible(shownMonth != QDate(today.year(), today.month(), 1));

    Cents limit = 0;
    Cents spent = 0;
    for (const MonthBudget& budget : budgets) {
        limit += budget.limitCents;
        spent += budget.spentCents;
    }
    const Settings& settings = Settings::instance();
    budgetedLabel->setText(settings.formatCurrency(limit));
    spentLabel->setText(settings.formatCurrency(spent));
    leftLabel->setText(settings.formatCurrency(qAbs(limit - spent)));
    leftCaption->setText(limit - spent < 0 ? "Over budget" : "Left");
    QPalette leftPalette = leftLabel->palette();
    leftPalette.setColor(QPalette::WindowText, limit - spent < 0 ? Theme::colors().danger : palette().color(QPalette::WindowText));
    leftLabel->setPalette(leftPalette);
}

void BudgetPage::addBudget(int categoryId, Cents suggestedLimit) {
    if (userId == -1) {
        return;
    }
    // Expense categories that don't have a budget in this month yet
    QList<Category> available;
    for (const Category& category : CategoryDAO(userId).getAllCategories()) {
        const bool budgeted = std::any_of(budgets.cbegin(), budgets.cend(), [&](const MonthBudget& budget) {
            return budget.categoryId == category.getCategoryId();
        });
        if (category.getType() == "expense" && !budgeted) {
            available.append(category);
        }
    }
    editor->newBudget(shownMonth, available, categoryId, suggestedLimit);
    editor->show();
    highlightRow(0);
}

void BudgetPage::editBudget(int categoryId) {
    for (const MonthBudget& budget : budgets) {
        if (budget.categoryId == categoryId) {
            editor->editBudget(shownMonth, budget);
            editor->show();
        }
    }
    highlightRow(categoryId);
}

void BudgetPage::onSaveRequested(int categoryId, Cents limitCents, bool everyMonth) {
    BudgetDAO dao;
    const bool saved = everyMonth ? dao.setMonthlyLimit(userId, categoryId, shownMonth, limitCents)
                                  : dao.setLimitForMonth(userId, categoryId, shownMonth, limitCents);
    if (!saved) {
        editor->showError("The budget could not be saved.");
        return;
    }
    closeEditor();
    reload();
    emit budgetsChanged();
}

void BudgetPage::onRemoveRequested(int categoryId, bool fromThisMonthOn) {
    if (!BudgetDAO().removeLimit(userId, categoryId, shownMonth, fromThisMonthOn)) {
        editor->showError("The budget could not be removed.");
        return;
    }
    closeEditor();
    reload();
    emit budgetsChanged();
}

void BudgetPage::closeEditor() {
    editor->hide();
    highlightRow(0);
}
