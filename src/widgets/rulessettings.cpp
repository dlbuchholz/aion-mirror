#include "rulessettings.h"
#include "dialog/applyrulesdialog.h"
#include "dialog/renamedpayeesdialog.h"
#include "account.h"
#include "categorytag.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

using Field = RuleCondition::Field;
using Operator = RuleCondition::Operator;

namespace {

QString fieldLabel(Field field) { return RuleCondition::fieldLabel(field); }
QString operatorLabel(Operator op) { return RuleCondition::operatorLabel(op); }

QList<Operator> operatorsFor(Field field) {
    switch (field) {
    case Field::Payee:
    case Field::Description:
    case Field::PayeeOrDescription: return {Operator::Contains, Operator::Is, Operator::StartsWith};
    case Field::Amount: return {Operator::Is, Operator::Between};
    case Field::Type:
    case Field::Account: return {Operator::Is};
    }
    return {};
}

// Text wrapped to at most maxLines lines, the last ending in "…" when the rest doesn't fit. Takes
// any width it's given, so a long rule never widens the list.
class ElidedText : public QWidget {
public:
    ElidedText(const QString& text, int maxLines, QWidget* parent) : QWidget(parent), text(text), maxLines(maxLines) {
        setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Maximum);
        setAttribute(Qt::WA_TransparentForMouseEvents);
    }

    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int width) const override {
        return qMax<qsizetype>(1, layoutLines(width).size()) * fontMetrics().lineSpacing();
    }
    QSize sizeHint() const override {
        return {fontMetrics().horizontalAdvance(text), heightForWidth(qMax(width(), 200))};
    }
    QSize minimumSizeHint() const override { return {40, fontMetrics().lineSpacing()}; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setPen(palette().color(foregroundRole()));
        int y = 0;
        for (const QString& line : layoutLines(width())) {
            painter.drawText(QRect(0, y, width(), fontMetrics().lineSpacing()), Qt::AlignLeft | Qt::AlignVCenter, line);
            y += fontMetrics().lineSpacing();
        }
    }

private:
    QStringList layoutLines(int width) const {
        QStringList lines;
        if (width <= 0) return lines;
        QTextLayout layout(text, font());
        layout.beginLayout();
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(width);
            const int start = line.textStart();
            if (lines.size() == maxLines - 1) {          // The last one gets everything left, cut short
                lines.append(fontMetrics().elidedText(text.mid(start), Qt::ElideRight, width));
                break;
            }
            lines.append(text.mid(start, line.textLength()).trimmed());
        }
        layout.endLayout();
        return lines;
    }

    QString text;
    int maxLines;
};

// One rule: its name and conditions (or only the conditions), its category and how many
// transactions it matches; ▲▼ move it
class RuleRow : public QAbstractButton {
public:
    RuleRow(const QString& name, const QString& description, const Category* category, const QString& newPayee,
            int matches, QWidget* parent)
        : QAbstractButton(parent) {
        setObjectName("ruleRow");
        setAccessibleName(name.isEmpty() ? description : name);
        setAccessibleDescription(description);
        setToolTip(description);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover);
        setMinimumHeight(38);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(4, 2, 10, 2);
        layout->setSpacing(10);
        up = new QToolButton(this);
        up->setObjectName("moveRuleUpButton");
        up->setArrowType(Qt::UpArrow);
        up->setAutoRaise(true);
        up->setToolTip("Try this rule earlier");
        down = new QToolButton(this);
        down->setObjectName("moveRuleDownButton");
        down->setArrowType(Qt::DownArrow);
        down->setAutoRaise(true);
        down->setToolTip("Try this rule later");
        layout->addWidget(up);
        layout->addWidget(down);
        // Named: the name, then the conditions on one line. Unnamed: the conditions on up to two.
        auto* texts = new QVBoxLayout();
        texts->setSpacing(0);
        texts->addStretch();                             // Centred in the row, also when one line is short
        if (!name.isEmpty()) {
            auto* nameText = new ElidedText(name, 1, this);
            nameText->setObjectName("ruleName");
            QFont bold = nameText->font();
            bold.setBold(true);
            nameText->setFont(bold);
            texts->addWidget(nameText);
        }
        auto* conditions = new ElidedText(description, name.isEmpty() ? 2 : 1, this);
        conditions->setObjectName("ruleConditions");
        if (!name.isEmpty()) conditions->setForegroundRole(QPalette::PlaceholderText);
        texts->addWidget(conditions);
        texts->addStretch();
        layout->addLayout(texts, 1);
        // What it does: renames (→ "Rewe") and/or sets a category
        if (!newPayee.isEmpty()) {
            auto* rename = new QLabel(QString("→ \"%1\"").arg(newPayee), this);
            rename->setObjectName("ruleRename");
            rename->setToolTip("Renames the payee");
            rename->setAttribute(Qt::WA_TransparentForMouseEvents);
            layout->addWidget(rename);
        }
        if (category) {
            auto* tag = new CategoryTag(this);
            tag->setCategory(category->getName(), category->getColor());
            tag->setAttribute(Qt::WA_TransparentForMouseEvents);
            layout->addWidget(tag);
        }
        auto* count = new QLabel(QString("%1 %2").arg(matches).arg(matches == 1 ? "match" : "matches"), this);
        count->setObjectName("ruleMatchCount");
        count->setForegroundRole(QPalette::PlaceholderText);
        count->setMinimumWidth(80);
        count->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        count->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(count);
    }

    QToolButton* up = nullptr;
    QToolButton* down = nullptr;
    bool open = false;

protected:
    void paintEvent(QPaintEvent*) override {
        if (!(open || underMouse())) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 6, 6);
        painter.fillPath(path, open ? Theme::colors().itemSelected : Theme::colors().itemHover);
    }
};

} // namespace

// One condition in the editor: field, operator and the input that fits the field
class ConditionEditor : public QWidget {
public:
    ConditionEditor(const QHash<int, QString>& accounts, QWidget* parent) : QWidget(parent) {
        setObjectName("ruleCondition");
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);
        fieldCombo = new QComboBox(this);
        fieldCombo->setObjectName("conditionField");
        for (Field field : {Field::Payee, Field::Description, Field::PayeeOrDescription, Field::Amount, Field::Type,
                            Field::Account}) {
            fieldCombo->addItem(fieldLabel(field), static_cast<int>(field));
        }
        operatorCombo = new QComboBox(this);
        operatorCombo->setObjectName("conditionOperator");
        textEdit = new QLineEdit(this);
        textEdit->setObjectName("conditionText");
        textEdit->setPlaceholderText("...");
        amountFrom = amountSpin("conditionAmount");
        andLabel = new QLabel("and", this);
        amountTo = amountSpin("conditionAmountTo");
        typeCombo = new QComboBox(this);
        typeCombo->setObjectName("conditionType");
        typeCombo->addItem("Expense", "expense");
        typeCombo->addItem("Income", "income");
        accountCombo = new QComboBox(this);
        accountCombo->setObjectName("conditionAccount");
        for (auto it = accounts.cbegin(); it != accounts.cend(); ++it) {
            accountCombo->addItem(it.value(), it.key());
        }
        accountCombo->model()->sort(0);
        removeButton = new QToolButton(this);
        removeButton->setObjectName("removeConditionButton");
        removeButton->setText("Remove");             // For screen readers; the button shows only the icon
        Icons::setThemedIcon(removeButton, "x");
        removeButton->setAutoRaise(true);
        removeButton->setToolTip("Remove this condition");
        layout->addWidget(fieldCombo);
        layout->addWidget(operatorCombo);
        layout->addWidget(textEdit, 1);
        layout->addWidget(amountFrom, 1);
        layout->addWidget(andLabel);
        layout->addWidget(amountTo, 1);
        layout->addWidget(typeCombo, 1);
        layout->addWidget(accountCombo, 1);
        layout->addWidget(removeButton);

        connect(fieldCombo, &QComboBox::currentIndexChanged, this, [this]() { fieldChanged(); });
        connect(operatorCombo, &QComboBox::currentIndexChanged, this, [this]() { updateInputs(); });
        fieldChanged();
    }

    void setCondition(const RuleCondition& condition) {
        fieldCombo->setCurrentIndex(fieldCombo->findData(static_cast<int>(condition.field)));
        operatorCombo->setCurrentIndex(qMax(0, operatorCombo->findData(static_cast<int>(condition.op))));
        switch (condition.field) {
        case Field::Amount:
            amountFrom->setValue(Money::toDouble(condition.value.toLongLong()));
            amountTo->setValue(Money::toDouble(condition.value2.toLongLong()));
            break;
        case Field::Type:
            typeCombo->setCurrentIndex(qMax(0, typeCombo->findData(condition.value)));
            break;
        case Field::Account:
            accountCombo->setCurrentIndex(qMax(0, accountCombo->findData(condition.value.toInt())));
            break;
        default:
            textEdit->setText(condition.value);
        }
    }

    // The condition, or nothing (with error set) if it's incomplete
    std::optional<RuleCondition> condition(QString* error) const {
        RuleCondition condition;
        condition.field = static_cast<Field>(fieldCombo->currentData().toInt());
        condition.op = static_cast<Operator>(operatorCombo->currentData().toInt());
        switch (condition.field) {
        case Field::Amount:
            condition.value = QString::number(Money::fromDouble(amountFrom->value()));
            if (condition.op == Operator::Between) {
                condition.value2 = QString::number(Money::fromDouble(amountTo->value()));
            }
            break;
        case Field::Type:
            condition.value = typeCombo->currentData().toString();
            break;
        case Field::Account:
            if (accountCombo->currentIndex() < 0) {
                *error = "Choose an account, or remove that condition.";
                return std::nullopt;
            }
            condition.value = QString::number(accountCombo->currentData().toInt());
            break;
        default:
            condition.value = textEdit->text().simplified();
            if (condition.value.isEmpty()) {
                *error = QString("Enter the text the %1 should match.").arg(fieldLabel(condition.field).toLower());
                return std::nullopt;
            }
        }
        return condition;
    }

    QToolButton* removeButton = nullptr;
    QComboBox* fieldCombo = nullptr;
    QComboBox* operatorCombo = nullptr;
    QLineEdit* textEdit = nullptr;
    QDoubleSpinBox* amountFrom = nullptr;
    QDoubleSpinBox* amountTo = nullptr;
    QComboBox* typeCombo = nullptr;
    QComboBox* accountCombo = nullptr;

private:
    QDoubleSpinBox* amountSpin(const QString& name) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setObjectName(name);
        spin->setDecimals(2);
        spin->setRange(0, 1e9);
        spin->setSuffix(" " + Settings::instance().getCurrencySymbol());
        return spin;
    }

    void fieldChanged() {
        const Field field = static_cast<Field>(fieldCombo->currentData().toInt());
        const QVariant previous = operatorCombo->currentData();
        operatorCombo->blockSignals(true);
        operatorCombo->clear();
        for (Operator op : operatorsFor(field)) {
            operatorCombo->addItem(operatorLabel(op), static_cast<int>(op));
        }
        operatorCombo->setCurrentIndex(qMax(0, operatorCombo->findData(previous)));
        operatorCombo->blockSignals(false);
        updateInputs();
    }

    void updateInputs() {
        const Field field = static_cast<Field>(fieldCombo->currentData().toInt());
        const bool text = field == Field::Payee || field == Field::Description || field == Field::PayeeOrDescription;
        const bool between = operatorCombo->currentData().toInt() == static_cast<int>(Operator::Between);
        textEdit->setVisible(text);
        amountFrom->setVisible(field == Field::Amount);
        andLabel->setVisible(field == Field::Amount && between);
        amountTo->setVisible(field == Field::Amount && between);
        typeCombo->setVisible(field == Field::Type);
        accountCombo->setVisible(field == Field::Account);
    }

    QLabel* andLabel = nullptr;
};

RulesSettings::RulesSettings(QWidget* parent)
    : QWidget(parent) {
    confirm = [this](const QString& title, const QString& text) {
        return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
               == QMessageBox::Yes;
    };
    reviewRenamedPayees = [](RenamedPayeesDialog& dialog) { dialog.exec(); };

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    auto* header = new QHBoxLayout();
    auto* heading = new QLabel("Rules", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    header->addWidget(heading, 1);
    addButton = new QPushButton("New rule", this);
    addButton->setObjectName("addRuleButton");
    addButton->setIcon(Icons::icon("plus", palette().color(QPalette::ButtonText)));
    connect(addButton, &QPushButton::clicked, this, [this]() { openEditor(0); });
    header->addWidget(addButton);
    root->addLayout(header);

    auto* hint = new QLabel("Rules give transactions a category. They're tried from top to bottom, and the first "
                            "one that matches wins. Changes are saved right away.", this);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 6, 6, 6);
    listLayout = new QVBoxLayout();
    listLayout->setSpacing(0);
    contentLayout->addLayout(listLayout);
    emptyLabel = new QLabel("No rules yet. A rule could say: payee contains Landlord → Category: Rent.", content);
    emptyLabel->setObjectName("noRulesLabel");
    emptyLabel->setForegroundRole(QPalette::PlaceholderText);
    emptyLabel->setWordWrap(true);
    emptyLabel->setContentsMargins(10, 12, 10, 12);
    contentLayout->addWidget(emptyLabel);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    auto* applyRow = new QHBoxLayout();
    applyRow->addStretch();
    auto* applyButton = new QPushButton("Apply to existing transactions…", this);
    applyButton->setObjectName("applyRulesToExistingButton");
    applyButton->setToolTip("Shows which transactions your rules would change, before anything changes");
    connect(applyButton, &QPushButton::clicked, this, [this]() {
        ApplyRulesDialog dialog(userId, this);
        if (dialog.exec() == QDialog::Accepted) {
            reload();                                // Match counts don't change, but categories did
            emit transactionsChanged();
        }
    });
    applyRow->addWidget(applyButton);
    root->addLayout(applyRow);

    // The inline editor
    editor = new QFrame(content);
    editor->setObjectName("ruleEditor");
    editor->setFrameShape(QFrame::StyledPanel);
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(12, 10, 12, 10);
    editorLayout->setSpacing(8);

    auto* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Name", editor));
    nameEdit = new QLineEdit(editor);
    nameEdit->setObjectName("ruleNameEdit");
    nameEdit->setPlaceholderText("(Optional name. Without one, the list shows the conditions.)");
    nameEdit->setMaxLength(100);
    connect(nameEdit, &QLineEdit::returnPressed, this, &RulesSettings::save);
    nameRow->addWidget(nameEdit, 1);
    editorLayout->addLayout(nameRow);

    auto* matchRow = new QHBoxLayout();
    matchRow->addWidget(new QLabel("When", editor));
    matchCombo = new QComboBox(editor);
    matchCombo->setObjectName("ruleMatchCombo");
    matchCombo->addItem("all", true);
    matchCombo->addItem("any", false);
    connect(matchCombo, &QComboBox::currentIndexChanged, this, &RulesSettings::updateMatchCount);
    matchRow->addWidget(matchCombo);
    matchRow->addWidget(new QLabel("of these match:", editor));
    matchRow->addStretch();
    editorLayout->addLayout(matchRow);

    conditionsLayout = new QVBoxLayout();
    conditionsLayout->setSpacing(6);
    editorLayout->addLayout(conditionsLayout);
    auto* addConditionButton = new QPushButton("Add condition", editor);
    addConditionButton->setObjectName("addConditionButton");
    connect(addConditionButton, &QPushButton::clicked, this, [this]() { addConditionRow(RuleCondition()); });
    auto* addConditionRowLayout = new QHBoxLayout();
    addConditionRowLayout->addWidget(addConditionButton);
    addConditionRowLayout->addStretch();
    editorLayout->addLayout(addConditionRowLayout);

    // What the rule does: set the category, rename the payee, or both
    auto* categoryRow = new QHBoxLayout();
    setsCategoryCheck = new QCheckBox("Set category to", editor);
    setsCategoryCheck->setObjectName("ruleSetsCategoryCheck");
    categoryRow->addWidget(setsCategoryCheck);
    categoryCombo = new QComboBox(editor);
    categoryCombo->setObjectName("ruleCategoryCombo");
    categoryRow->addWidget(categoryCombo);
    categoryRow->addStretch();
    matchCountLabel = new QLabel(editor);
    matchCountLabel->setObjectName("ruleEditorMatchCount");
    matchCountLabel->setForegroundRole(QPalette::PlaceholderText);
    categoryRow->addWidget(matchCountLabel);
    editorLayout->addLayout(categoryRow);
    auto* renameRow = new QHBoxLayout();
    renamesCheck = new QCheckBox("Rename payee to", editor);
    renamesCheck->setObjectName("ruleRenamesCheck");
    renameRow->addWidget(renamesCheck);
    newPayeeEdit = new QLineEdit(editor);
    newPayeeEdit->setObjectName("ruleNewPayeeEdit");
    newPayeeEdit->setPlaceholderText("...");
    newPayeeEdit->setToolTip("The bank's name is kept, so imports still recognise the transaction");
    renameRow->addWidget(newPayeeEdit, 1);
    editorLayout->addLayout(renameRow);
    connect(setsCategoryCheck, &QCheckBox::toggled, categoryCombo, &QWidget::setEnabled);
    connect(renamesCheck, &QCheckBox::toggled, newPayeeEdit, &QWidget::setEnabled);
    connect(newPayeeEdit, &QLineEdit::textEdited, this, [this]() { renamesCheck->setChecked(true); });
    connect(newPayeeEdit, &QLineEdit::returnPressed, this, &RulesSettings::save);

    errorLabel = new QLabel(editor);
    errorLabel->setObjectName("ruleError");
    errorLabel->setWordWrap(true);
    QPalette errorPalette = errorLabel->palette();
    errorPalette.setColor(QPalette::WindowText, Theme::colors().danger);
    errorLabel->setPalette(errorPalette);
    errorLabel->hide();
    editorLayout->addWidget(errorLabel);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    deleteButton = new QPushButton("Delete", editor);
    deleteButton->setObjectName("deleteRuleButton");
    connect(deleteButton, &QPushButton::clicked, this, &RulesSettings::remove);
    buttons->addWidget(deleteButton);
    auto* cancelButton = new QPushButton("Cancel", editor);
    connect(cancelButton, &QPushButton::clicked, this, &RulesSettings::closeEditor);
    buttons->addWidget(cancelButton);
    auto* saveButton = new QPushButton("Save", editor);
    saveButton->setObjectName("saveRuleButton");
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &RulesSettings::save);
    buttons->addWidget(saveButton);
    editorLayout->addLayout(buttons);
    editor->hide();

    reload();
}

void RulesSettings::setUserId(int newUserId) {
    userId = newUserId;
    reload();
}

void RulesSettings::reload() {
    rules = userId > 0 ? RuleDAO(userId).rules() : QList<Rule>();
    categories.clear();
    accountNames.clear();
    matchCounts.clear();
    QList<Transaction> transactions;
    if (userId > 0) {
        for (const Category& category : CategoryDAO(userId).getAllCategories()) {
            if (!category.isBalanceAdjustment()) categories.append(category);   // Rules may set Transfer
        }
        TransactionDAO transactionDao;
        for (const Account& account : AccountDAO().getAllAccountsForUser(userId)) {
            accountNames.insert(account.getAccountId(), account.getName());
            transactions += transactionDao.getTransactionsForAccount(account.getAccountId());
        }
    }
    matchIndex = RuleMatchIndex(transactions);
    closeEditor();
}

QString RulesSettings::describe(const Rule& rule) const {
    return Rules::describe(rule, accountNames);
}

int RulesSettings::matchCount(const Rule& rule) const {
    return matchIndex.count(rule);
}

void RulesSettings::rebuildList() {
    // A clicked row (or its ▲▼) has the focus. Hiding it would pass the focus on, and the scroll
    // area would jump to wherever it lands.
    if (QWidget* focused = QApplication::focusWidget(); focused && listLayout->parentWidget()->isAncestorOf(focused)
                                                        && focused != editor && !editor->isAncestorOf(focused)) {
        focused->clearFocus();
    }
    // The editor isn't a row; take it out before the rows go
    listLayout->removeWidget(editor);
    while (QLayoutItem* item = listLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
            widget->deleteLater();                   // Can run from a row's own button
        }
        delete item;
    }
    emptyLabel->setVisible(rules.isEmpty() && editedId != 0);

    for (int i = 0; i < rules.size(); ++i) {
        const Rule& rule = rules[i];
        const Category* category = nullptr;
        for (const Category& candidate : categories) {
            if (candidate.getCategoryId() == rule.categoryId) category = &candidate;
        }
        if (!matchCounts.contains(rule.id)) matchCounts.insert(rule.id, matchCount(rule));
        auto* row = new RuleRow(rule.name.simplified(), describe(rule), category, rule.newPayee, matchCounts.value(rule.id),
                                listLayout->parentWidget());
        row->open = rule.id == editedId;
        row->up->setEnabled(i > 0);
        row->down->setEnabled(i < rules.size() - 1);
        const int ruleId = rule.id;
        connect(row, &QAbstractButton::clicked, this, [this, ruleId]() { openEditor(ruleId); });
        connect(row->up, &QToolButton::clicked, this, [this, ruleId]() { move(ruleId, -1); });
        connect(row->down, &QToolButton::clicked, this, [this, ruleId]() { move(ruleId, +1); });
        listLayout->addWidget(row);
        if (rule.id == editedId) {
            listLayout->addWidget(editor);
        }
    }
    if (editedId == 0) {
        listLayout->addWidget(editor);
    }
    editor->setVisible(editedId >= 0);
}

void RulesSettings::createRule(const Rule& draft) {
    openEditor(0, &draft);
}

void RulesSettings::openEditor(int ruleId, const Rule* draft) {
    editedId = ruleId;
    errorLabel->hide();
    for (ConditionEditor* conditionEditor : std::as_const(conditionEditors)) {
        conditionEditor->deleteLater();
    }
    conditionEditors.clear();

    categoryCombo->clear();
    for (const Category& category : std::as_const(categories)) {
        categoryCombo->addItem(category.getName(), category.getCategoryId());
    }

    Rule rule = draft ? *draft : Rule();
    for (const Rule& candidate : std::as_const(rules)) {
        if (ruleId > 0 && candidate.id == ruleId) rule = candidate;
    }
    nameEdit->setText(rule.name);
    matchCombo->setCurrentIndex(rule.matchAll ? 0 : 1);
    categoryCombo->setCurrentIndex(qMax(0, categoryCombo->findData(rule.categoryId)));
    // A rule that only renames has the category unticked; anything else starts with it ticked
    setsCategoryCheck->setChecked(rule.setsCategory() || !rule.renames());
    renamesCheck->setChecked(rule.renames());
    newPayeeEdit->setText(rule.newPayee);
    categoryCombo->setEnabled(setsCategoryCheck->isChecked());
    newPayeeEdit->setEnabled(renamesCheck->isChecked());
    const QList<RuleCondition> conditions = rule.conditions.isEmpty() ? QList<RuleCondition>{RuleCondition()}
                                                                      : rule.conditions;
    for (const RuleCondition& condition : conditions) {
        addConditionRow(condition);
    }
    deleteButton->setVisible(ruleId > 0);
    rebuildList();
    updateMatchCount();
    if (!conditionEditors.isEmpty()) {
        conditionEditors.first()->textEdit->setFocus();
    }
}

void RulesSettings::closeEditor() {
    editedId = -1;
    rebuildList();
}

void RulesSettings::addConditionRow(const RuleCondition& condition) {
    auto* conditionEditor = new ConditionEditor(accountNames, editor);
    conditionEditor->setCondition(condition);
    conditionsLayout->addWidget(conditionEditor);
    conditionEditors.append(conditionEditor);
    connect(conditionEditor->removeButton, &QToolButton::clicked, this, [this, conditionEditor]() {
        conditionEditors.removeOne(conditionEditor);
        conditionEditor->deleteLater();
        updateMatchCount();
    });
    // Any change updates the live match count
    for (QComboBox* combo : conditionEditor->findChildren<QComboBox*>()) {
        connect(combo, &QComboBox::currentIndexChanged, this, &RulesSettings::updateMatchCount);
    }
    connect(conditionEditor->textEdit, &QLineEdit::textChanged, this, &RulesSettings::updateMatchCount);
    connect(conditionEditor->textEdit, &QLineEdit::returnPressed, this, &RulesSettings::save);
    for (QDoubleSpinBox* spin : {conditionEditor->amountFrom, conditionEditor->amountTo}) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &RulesSettings::updateMatchCount);
    }
}

std::optional<Rule> RulesSettings::editedRule(QString* error) const {
    Rule rule;
    rule.id = editedId;
    rule.name = nameEdit->text().simplified();
    rule.matchAll = matchCombo->currentData().toBool();
    rule.categoryId = setsCategoryCheck->isChecked() ? categoryCombo->currentData().toInt() : -1;
    rule.newPayee = renamesCheck->isChecked() ? newPayeeEdit->text().simplified() : QString();
    for (const ConditionEditor* conditionEditor : conditionEditors) {
        const std::optional<RuleCondition> condition = conditionEditor->condition(error);
        if (!condition) return std::nullopt;
        rule.conditions.append(*condition);
    }
    if (rule.conditions.isEmpty()) {
        *error = "Add at least one condition.";
        return std::nullopt;
    }
    if (setsCategoryCheck->isChecked() && categoryCombo->currentIndex() < 0) {
        *error = "Choose the category the rule sets.";
        return std::nullopt;
    }
    if (renamesCheck->isChecked() && rule.newPayee.isEmpty()) {
        *error = "Enter the new name for the payee.";
        return std::nullopt;
    }
    if (!rule.setsCategory() && !rule.renames()) {
        *error = "Tick what the rule does: set a category, rename the payee, or both.";
        return std::nullopt;
    }
    return rule;
}

void RulesSettings::updateMatchCount() {
    QString error;
    const std::optional<Rule> rule = editedRule(&error);
    const int count = rule ? matchCount(*rule) : 0;
    matchCountLabel->setText(rule ? QString("Matches %1 existing %2").arg(count).arg(count == 1 ? "transaction" : "transactions")
                                  : QString());
}

void RulesSettings::save() {
    QString error;
    const std::optional<Rule> rule = editedRule(&error);
    if (!rule) {
        errorLabel->setText(error);
        errorLabel->show();
        return;
    }
    Rule before;
    for (const Rule& candidate : std::as_const(rules)) {
        if (editedId > 0 && candidate.id == editedId) before = candidate;
    }
    RuleDAO dao(userId);
    const bool saved = editedId > 0 ? dao.updateRule(*rule) : dao.addRule(*rule) > 0;
    if (!saved) {
        errorLabel->setText("The rule couldn't be saved.");
        errorLabel->show();
        return;
    }
    rules = dao.rules();
    matchCounts.remove(editedId);                    // Its conditions may have changed; a new rule has no count yet
    closeEditor();
    emit rulesChanged();
    if (before.id > 0 && before.newPayee != rule->newPayee) {
        offerToUpdateNames(TransactionDAO().getTransactionsRenamedByRule(before.id), before, rule->newPayee);
    }
}

void RulesSettings::offerToUpdateNames(const QList<Transaction>& renamed, const Rule& before, const QString& newPayee) {
    if (renamed.isEmpty()) {
        return;
    }
    RenamedPayeesDialog dialog(renamed, before.newPayee, newPayee, before.id, this);
    reviewRenamedPayees(dialog);
    if (dialog.result() == QDialog::Accepted) {
        reload();                                    // Match counts look at the payees
        emit transactionsChanged();
    }
}

void RulesSettings::remove() {
    if (editedId <= 0) return;
    Rule rule;
    for (const Rule& candidate : std::as_const(rules)) {
        if (candidate.id == editedId) rule = candidate;
    }
    if (!confirm("Delete rule", QString("Delete the rule \"%1\"?\n\nTransactions keep their categories.")
                                    .arg(Rules::title(rule, accountNames)))) {
        return;
    }
    // Read before deleting: the database then forgets which rule renamed them
    const QList<Transaction> renamed = TransactionDAO().getTransactionsRenamedByRule(rule.id);
    RuleDAO dao(userId);
    if (!dao.deleteRule(editedId)) {
        errorLabel->setText("The rule couldn't be deleted.");
        errorLabel->show();
        return;
    }
    rules = dao.rules();
    closeEditor();
    emit rulesChanged();
    // Also when its rename was turned off earlier and the names kept: they still point to the rule
    offerToUpdateNames(renamed, rule, QString());
}

void RulesSettings::move(int ruleId, int offset) {
    QList<int> order;
    for (const Rule& rule : std::as_const(rules)) order.append(rule.id);
    const qsizetype from = order.indexOf(ruleId);
    const qsizetype to = from + offset;
    if (from < 0 || to < 0 || to >= order.size()) return;
    order.move(from, to);
    RuleDAO dao(userId);
    if (dao.setOrder(order)) {
        rules = dao.rules();
        rebuildList();
        emit rulesChanged();
    }
}
