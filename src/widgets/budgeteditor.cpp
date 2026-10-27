#include "budgeteditor.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QString monthName(const QDate& month) {
    return QLocale(QLocale::English).toString(month, "MMMM");
}
}

BudgetEditor::BudgetEditor(QWidget* parent)
    : QFrame(parent) {
    setFrameShape(QFrame::StyledPanel);
    setFixedWidth(300);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(12);

    // Title and close button
    auto* header = new QHBoxLayout();
    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    header->addWidget(titleLabel, 1);
    auto* closeButton = new QToolButton(this);
    closeButton->setText("Close");                   // For screen readers; the button shows only the icon
    Icons::setThemedIcon(closeButton, "x");
    closeButton->setAutoRaise(true);
    closeButton->setToolTip("Close (Esc)");
    connect(closeButton, &QToolButton::clicked, this, &BudgetEditor::closed);
    header->addWidget(closeButton);
    layout->addLayout(header);

    auto* form = new QFormLayout();
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setVerticalSpacing(8);

    auto* categoryField = new QWidget(this);
    auto* categoryLayout = new QVBoxLayout(categoryField);
    categoryLayout->setContentsMargins(0, 0, 0, 0);
    categoryCombo = new QComboBox(categoryField);
    categoryCombo->setObjectName("budgetCategoryCombo");
    categoryLabel = new QLabel(categoryField);
    categoryLabel->setObjectName("budgetCategoryLabel");
    QFont categoryFont = categoryLabel->font();
    categoryFont.setPointSize(11);
    categoryLabel->setFont(categoryFont);
    categoryLayout->addWidget(categoryCombo);
    categoryLayout->addWidget(categoryLabel);
    form->addRow("Category", categoryField);

    limitEdit = new QDoubleSpinBox(this);
    limitEdit->setObjectName("budgetLimitEdit");
    limitEdit->setRange(0.0, 999999999.99);
    limitEdit->setDecimals(2);
    limitEdit->setButtonSymbols(QAbstractSpinBox::NoButtons);
    limitEdit->setAlignment(Qt::AlignRight);
    QFont limitFont = limitEdit->font();
    limitFont.setPointSize(13);
    limitEdit->setFont(limitFont);
    form->addRow("Limit", limitEdit);

    // Every month from now on, or this month only
    auto* scopeField = new QWidget(this);
    auto* scopeLayout = new QVBoxLayout(scopeField);
    scopeLayout->setContentsMargins(0, 2, 0, 0);
    scopeLayout->setSpacing(6);
    everyMonthButton = new QRadioButton(scopeField);
    everyMonthButton->setObjectName("everyMonthButton");
    onlyThisMonthButton = new QRadioButton(scopeField);
    onlyThisMonthButton->setObjectName("onlyThisMonthButton");
    auto* scopeGroup = new QButtonGroup(this);
    scopeGroup->addButton(everyMonthButton);
    scopeGroup->addButton(onlyThisMonthButton);
    scopeLayout->addWidget(everyMonthButton);
    scopeLayout->addWidget(onlyThisMonthButton);
    hintLabel = new QLabel(scopeField);
    hintLabel->setWordWrap(true);
    QFont hintFont = hintLabel->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() * 0.9);
    hintLabel->setFont(hintFont);
    hintLabel->setForegroundRole(QPalette::PlaceholderText);
    scopeLayout->addWidget(hintLabel);
    form->addRow("Applies to", scopeField);
    connect(scopeGroup, &QButtonGroup::buttonToggled, this, [this]() {
        const QString name = monthName(month);
        hintLabel->setText(everyMonthButton->isChecked()
            ? QString("Earlier months keep their limit. Change it again whenever your plans change.")
            : QString("After %1 the regular limit applies again, if there is one.").arg(name));
    });
    layout->addLayout(form);

    errorLabel = new QLabel(this);
    errorLabel->setWordWrap(true);
    QPalette errorPalette = errorLabel->palette();
    errorPalette.setColor(QPalette::WindowText, Theme::colors().danger);
    errorLabel->setPalette(errorPalette);
    errorLabel->hide();
    layout->addWidget(errorLabel);

    // Save / cancel, remove for existing budgets
    auto* buttons = new QHBoxLayout();
    auto* saveButton = new QPushButton("Save", this);
    saveButton->setObjectName("budgetSaveButton");
    saveButton->setDefault(true);
    saveButton->setToolTip("Save (Enter)");
    connect(saveButton, &QPushButton::clicked, this, &BudgetEditor::requestSave);
    auto* cancelButton = new QPushButton("Cancel", this);
    connect(cancelButton, &QPushButton::clicked, this, &BudgetEditor::closed);
    removeButton = new QPushButton("Remove", this);
    removeButton->setObjectName("budgetRemoveButton");
    removeMenu = new QMenu(removeButton);
    connect(removeButton, &QPushButton::clicked, this, [this]() {
        if (!removeButton->menu()) emit removeRequested(categoryId, false);   // Single month: nothing to choose
    });
    buttons->addWidget(saveButton, 1);
    buttons->addWidget(cancelButton);
    buttons->addStretch();
    buttons->addWidget(removeButton);
    layout->addLayout(buttons);
    layout->addStretch();

    applySettings();
}

void BudgetEditor::applySettings() {
    const bool comma = Settings::instance().getDecimalSeparator() == Settings::DecimalSeparator::Comma;
    QLocale locale(comma ? QLocale::German : QLocale::English);
    locale.setNumberOptions(QLocale::OmitGroupSeparator);
    limitEdit->setLocale(locale);
    limitEdit->setValue(limitEdit->value());         // Re-render with the new separator
}

void BudgetEditor::setMonth(const QDate& newMonth) {
    month = newMonth;
    const QString name = monthName(month);
    everyMonthButton->setText(QString("Every month from %1").arg(name));
    onlyThisMonthButton->setText(QString("Only %1").arg(name));
    errorLabel->hide();
}

void BudgetEditor::newBudget(const QDate& newMonth, const QList<Category>& categories, int selectedCategoryId,
                             Cents suggestedLimit) {
    setMonth(newMonth);
    categoryId = 0;
    titleLabel->setText("New budget");
    categoryCombo->clear();
    for (const Category& category : categories) {
        categoryCombo->addItem(category.getName(), category.getCategoryId());
    }
    categoryCombo->setCurrentIndex(qMax(0, categoryCombo->findData(selectedCategoryId)));
    categoryCombo->show();
    categoryLabel->hide();
    limitEdit->setValue(Money::toDouble(suggestedLimit));
    everyMonthButton->setChecked(true);
    removeButton->hide();

    if (categories.isEmpty()) {
        showError("Every expense category already has a budget this month. Add categories under Settings → Categories.");
    }
    (categoryCombo->count() > 0 && selectedCategoryId <= 0 ? static_cast<QWidget*>(categoryCombo) : limitEdit)->setFocus();
    limitEdit->selectAll();
}

void BudgetEditor::editBudget(const QDate& newMonth, const MonthBudget& budget) {
    setMonth(newMonth);
    categoryId = budget.categoryId;
    titleLabel->setText("Edit budget");
    categoryCombo->hide();
    categoryLabel->setText(budget.categoryName);
    categoryLabel->show();
    limitEdit->setValue(Money::toDouble(budget.limitCents));
    (budget.repeats ? everyMonthButton : onlyThisMonthButton)->setChecked(true);

    // A regular limit can stop for good or skip this month; a single month just goes away
    const QString name = monthName(month);
    removeMenu->clear();
    removeMenu->addAction(QString("From %1 on").arg(name), this, [this]() { emit removeRequested(categoryId, true); });
    removeMenu->addAction(QString("Only in %1").arg(name), this, [this]() { emit removeRequested(categoryId, false); });
    removeButton->setMenu(budget.repeats ? removeMenu : nullptr);
    removeButton->show();

    limitEdit->setFocus();
    limitEdit->selectAll();
}

int BudgetEditor::editedCategoryId() const {
    return categoryId;
}

void BudgetEditor::showError(const QString& message) {
    errorLabel->setText(message);
    errorLabel->show();
}

void BudgetEditor::requestSave() {
    const int category = categoryId > 0 ? categoryId : categoryCombo->currentData().toInt();
    if (category <= 0) {
        showError("Choose a category for this budget.");
        return;
    }
    const Cents limit = Money::fromDouble(limitEdit->value());
    if (limit <= 0) {
        showError("Enter a limit greater than zero.");
        limitEdit->setFocus();
        limitEdit->selectAll();
        return;
    }
    errorLabel->hide();
    emit saveRequested(category, limit, everyMonthButton->isChecked());
}

void BudgetEditor::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        emit closed();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        requestSave();
        return;
    }
    QFrame::keyPressEvent(event);
}
