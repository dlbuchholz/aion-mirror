#include "categoriessettings.h"
#include "categorycolors.h"
#include "categorytag.h"
#include "icons.h"
#include "theme.h"

#include <QAbstractButton>
#include <QApplication>
#include <QButtonGroup>
#include <QComboBox>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

namespace {

QString usageText(const CategoryUsage& usage) {
    QStringList parts;
    parts << QString("%1 %2").arg(usage.transactions).arg(usage.transactions == 1 ? "transaction" : "transactions");
    if (usage.budgets > 0) parts << (usage.budgets == 1 ? QString("budget") : QString("%1 budgets").arg(usage.budgets));
    return parts.join(" · ");
}

// Round colour choice; the neutral one shows a slash
class SwatchButton : public QAbstractButton {
public:
    SwatchButton(const QString& color, QWidget* parent) : QAbstractButton(parent), color(color) {
        setCheckable(true);
        setFixedSize(26, 26);
        setCursor(Qt::PointingHandCursor);
        setToolTip(CategoryColors::label(color));
        setAccessibleName(CategoryColors::label(color));
        setObjectName("swatch_" + (color.isEmpty() ? QString("none") : color));
    }

    const QString color;

protected:
    void paintEvent(QPaintEvent*) override {
        const Theme::Colors colors = Theme::colors();
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF dot = QRectF(rect()).adjusted(5, 5, -5, -5);
        if (isChecked() || underMouse()) {
            painter.setPen(QPen(isChecked() ? colors.textPrimary : colors.textMuted, 2));
            painter.drawEllipse(QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5));
        }
        painter.setPen(Qt::NoPen);
        painter.setBrush(color.isEmpty() ? colors.track : CategoryColors::swatch(color).solid);
        painter.drawEllipse(dot);
        if (color.isEmpty()) {
            painter.setPen(QPen(colors.textMuted, 1.5));
            painter.drawLine(dot.topRight() + QPointF(-3, 3), dot.bottomLeft() + QPointF(3, -3));
        }
    }
    void enterEvent(QEnterEvent* event) override { update(); QAbstractButton::enterEvent(event); }
    void leaveEvent(QEvent* event) override { update(); QAbstractButton::leaveEvent(event); }
};

// One category: its tag, how much it is used, and a lock for built-in ones
class CategoryRow : public QAbstractButton {
public:
    CategoryRow(const Category& category, const CategoryUsage& usage, QWidget* parent)
        : QAbstractButton(parent), categoryId(category.getCategoryId()) {
        setObjectName("categoryRow");
        setAccessibleName(category.getName());
        setCursor(category.isBuiltIn() ? Qt::ArrowCursor : Qt::PointingHandCursor);
        setEnabled(true);
        setAttribute(Qt::WA_Hover);
        setFixedHeight(38);
        auto* layout = new QHBoxLayout(this);
        layout->setContentsMargins(10, 0, 10, 0);
        layout->setSpacing(12);
        auto* tag = new CategoryTag(this);
        tag->setCategory(category.getName(), category.getColor());
        tag->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(tag);
        auto* detail = new QLabel(category.isBuiltIn() ? QString("Built in · not counted as income or spending")
                                                       : usageText(usage), this);
        detail->setForegroundRole(QPalette::PlaceholderText);
        detail->setAttribute(Qt::WA_TransparentForMouseEvents);
        layout->addWidget(detail, 1);
        if (category.isBuiltIn()) {
            setToolTip("Used for starting balances and balance corrections, so it can't be changed");
        }
        builtIn = category.isBuiltIn();
    }

    const int categoryId;
    bool builtIn = false;
    bool open = false;

protected:
    void paintEvent(QPaintEvent*) override {
        if (!(open || (underMouse() && !builtIn))) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        QPainterPath path;
        path.addRoundedRect(QRectF(rect()), 6, 6);
        painter.fillPath(path, open ? Theme::colors().itemSelected : Theme::colors().itemHover);
    }
};

QLabel* groupLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("categoryGroupLabel");
    QFont font = label->font();
    font.setPointSizeF(font.pointSizeF() * 0.9);
    font.setWeight(QFont::DemiBold);
    label->setFont(font);
    label->setForegroundRole(QPalette::PlaceholderText);
    label->setContentsMargins(10, 12, 0, 4);
    return label;
}

} // namespace

CategoriesSettings::CategoriesSettings(QWidget* parent)
    : QWidget(parent) {
    confirm = [this](const QString& title, const QString& text) {
        return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
               == QMessageBox::Yes;
    };

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    // Heading, search and the add button
    auto* header = new QHBoxLayout();
    auto* heading = new QLabel("Categories", this);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    header->addWidget(heading, 1);
    searchEdit = new QLineEdit(this);
    searchEdit->setObjectName("categorySearch");
    searchEdit->setPlaceholderText("Search");
    searchEdit->setClearButtonEnabled(true);
    searchEdit->setMaximumWidth(200);
    connect(searchEdit, &QLineEdit::textChanged, this, &CategoriesSettings::rebuildList);
    header->addWidget(searchEdit);
    addButton = new QPushButton("Add category", this);
    addButton->setObjectName("addCategoryButton");
    connect(addButton, &QPushButton::clicked, this, [this]() { openEditor(0); });
    header->addWidget(addButton);
    root->addLayout(header);

    auto* hint = new QLabel("Every user has their own categories. Changes are saved right away.", this);
    hint->setForegroundRole(QPalette::PlaceholderText);
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::StyledPanel);
    auto* content = new QWidget(scroll);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(6, 0, 6, 6);
    listLayout = new QVBoxLayout();
    listLayout->setSpacing(0);
    contentLayout->addLayout(listLayout);
    emptyLabel = new QLabel("No categories match.", content);
    emptyLabel->setForegroundRole(QPalette::PlaceholderText);
    emptyLabel->setContentsMargins(10, 12, 10, 12);
    contentLayout->addWidget(emptyLabel);
    contentLayout->addStretch();
    scroll->setWidget(content);
    root->addWidget(scroll, 1);

    // The inline editor
    editor = new QFrame(content);
    editor->setObjectName("categoryEditor");
    editor->setFrameShape(QFrame::StyledPanel);
    auto* editorLayout = new QVBoxLayout(editor);
    editorLayout->setContentsMargins(12, 10, 12, 10);
    editorLayout->setSpacing(8);
    auto* nameRow = new QHBoxLayout();
    nameEdit = new QLineEdit(editor);
    nameEdit->setObjectName("categoryNameEdit");
    nameEdit->setPlaceholderText("Name");
    connect(nameEdit, &QLineEdit::returnPressed, this, &CategoriesSettings::save);
    typeCombo = new QComboBox(editor);
    typeCombo->setObjectName("categoryTypeCombo");
    typeCombo->addItem("Expense", "expense");
    typeCombo->addItem("Income", "income");
    nameRow->addWidget(nameEdit, 1);
    nameRow->addWidget(typeCombo);
    editorLayout->addLayout(nameRow);

    // No colour, then the palette in hue order: two rows of ten
    auto* colorRow = new QHBoxLayout();
    auto* colorCaption = new QLabel("Colour", editor);
    colorCaption->setMinimumWidth(56);
    colorRow->addWidget(colorCaption, 0, Qt::AlignTop);
    auto* swatches = new QGridLayout();
    swatches->setSpacing(4);
    colorGroup = new QButtonGroup(this);
    colorGroup->setExclusive(true);
    const QStringList colors = QStringList{QString()} + CategoryColors::names();
    constexpr int SWATCHES_PER_ROW = 10;
    for (int i = 0; i < colors.size(); ++i) {
        auto* swatch = new SwatchButton(colors[i], editor);
        colorGroup->addButton(swatch);
        swatches->addWidget(swatch, i / SWATCHES_PER_ROW, i % SWATCHES_PER_ROW);
    }
    colorRow->addLayout(swatches);
    colorRow->addStretch();
    editorLayout->addLayout(colorRow);

    errorLabel = new QLabel(editor);
    errorLabel->setObjectName("categoryError");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    editorLayout->addWidget(errorLabel);

    auto* buttons = new QHBoxLayout();
    usageLabel = new QLabel(editor);
    usageLabel->setForegroundRole(QPalette::PlaceholderText);
    buttons->addWidget(usageLabel, 1);
    mergeButton = new QToolButton(editor);
    mergeButton->setObjectName("mergeCategoryButton");
    mergeButton->setText("Merge into…");
    mergeButton->setPopupMode(QToolButton::InstantPopup);
    mergeButton->setMenu(new QMenu(mergeButton));
    mergeButton->setToolTip("Move this category's transactions to another category and delete it");
    buttons->addWidget(mergeButton);
    deleteButton = new QPushButton("Delete", editor);
    deleteButton->setObjectName("deleteCategoryButton");
    connect(deleteButton, &QPushButton::clicked, this, &CategoriesSettings::remove);
    buttons->addWidget(deleteButton);
    auto* cancelButton = new QPushButton("Cancel", editor);
    connect(cancelButton, &QPushButton::clicked, this, &CategoriesSettings::closeEditor);
    buttons->addWidget(cancelButton);
    auto* saveButton = new QPushButton("Save", editor);
    saveButton->setObjectName("saveCategoryButton");
    saveButton->setDefault(true);
    connect(saveButton, &QPushButton::clicked, this, &CategoriesSettings::save);
    buttons->addWidget(saveButton);
    editorLayout->addLayout(buttons);
    editor->hide();

    updateSwatches();
    reload();
}

void CategoriesSettings::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && editor) {
        updateSwatches();
    }
    QWidget::changeEvent(event);
}

void CategoriesSettings::updateSwatches() {
    QPalette palette = errorLabel->palette();
    palette.setColor(QPalette::WindowText, Theme::colors().danger);
    errorLabel->setPalette(palette);
    addButton->setIcon(Icons::icon("plus", this->palette().color(QPalette::ButtonText)));
}

void CategoriesSettings::setUserId(int newUserId) {
    userId = newUserId;
    reload();
}

void CategoriesSettings::reload() {
    categories = CategoryDAO(userId).getAllCategories();
    usage = CategoryDAO(userId).usage();
    if (editedId > 0 && !category(editedId)) {
        editedId = -1;                               // Deleted meanwhile
    }
    rebuildList();
}

const Category* CategoriesSettings::category(int categoryId) const {
    for (const Category& entry : categories) {
        if (entry.getCategoryId() == categoryId) return &entry;
    }
    return nullptr;
}

void CategoriesSettings::rebuildList() {
    // A clicked row has the focus. Hiding it would pass the focus on, and the scroll area would
    // jump to wherever it lands.
    if (QWidget* focused = QApplication::focusWidget(); focused && listLayout->parentWidget()->isAncestorOf(focused)
                                                        && focused != editor && !editor->isAncestorOf(focused)) {
        focused->clearFocus();
    }
    // Replace the rows but keep the editor. deleteLater: this can run from a row's own click.
    while (QLayoutItem* item = listLayout->takeAt(0)) {
        if (QWidget* widget = item->widget(); widget && widget != editor) {
            widget->hide();
            widget->deleteLater();
        }
        delete item;
    }

    if (editedId == 0) {
        listLayout->addWidget(editor);               // New category: editor on top
    }
    const QString search = searchEdit->text().trimmed();
    int shown = 0;
    const QList<QPair<QString, std::function<bool(const Category&)>>> groups = {
        {"Expenses", [](const Category& c) { return !c.isBuiltIn() && c.getType() == "expense"; }},
        {"Income", [](const Category& c) { return !c.isBuiltIn() && c.getType() == "income"; }},
        {"Built in", [](const Category& c) { return c.isBuiltIn(); }},
    };
    for (const auto& group : groups) {
        bool labelAdded = false;
        for (const Category& entry : categories) {
            if (!group.second(entry) || (!search.isEmpty() && !entry.getName().contains(search, Qt::CaseInsensitive))) {
                continue;
            }
            if (!labelAdded) {
                listLayout->addWidget(groupLabel(group.first, listLayout->parentWidget()));
                labelAdded = true;
            }
            auto* row = new CategoryRow(entry, usage.value(entry.getCategoryId()), listLayout->parentWidget());
            row->open = entry.getCategoryId() == editedId;
            const int categoryId = entry.getCategoryId();
            if (!entry.isBuiltIn()) {
                connect(row, &QAbstractButton::clicked, this, [this, categoryId]() {
                    if (editedId == categoryId) closeEditor(); else openEditor(categoryId);
                });
            }
            listLayout->addWidget(row);
            if (entry.getCategoryId() == editedId) {
                listLayout->addWidget(editor);
            }
            ++shown;
        }
    }
    emptyLabel->setVisible(shown == 0);
    editor->setVisible(editedId >= 0);
}

void CategoriesSettings::openEditor(int categoryId) {
    const Category* edited = category(categoryId);
    editedId = edited ? categoryId : 0;
    errorLabel->hide();
    if (edited) {
        nameEdit->setText(edited->getName());
        typeCombo->setCurrentIndex(qMax(0, typeCombo->findData(edited->getType())));
        selectColor(edited->getColor());
        usageLabel->setText(usageText(usage.value(categoryId)));

        // Merge into another category (built-in ones stay out of it)
        QMenu* menu = mergeButton->menu();
        menu->clear();
        for (const QString& type : {QString("expense"), QString("income")}) {
            for (const Category& target : categories) {
                if (target.getCategoryId() == categoryId || target.isBuiltIn() || target.getType() != type) continue;
                const int targetId = target.getCategoryId();
                menu->addAction(target.getName(), this, [this, targetId]() { mergeInto(targetId); });
            }
            menu->addSeparator();
        }
        mergeButton->setEnabled(!menu->isEmpty());
    } else {
        nameEdit->clear();
        typeCombo->setCurrentIndex(0);
        selectColor(QString());
        usageLabel->setText("New category");
    }
    mergeButton->setVisible(edited);
    deleteButton->setVisible(edited);
    rebuildList();
    nameEdit->setFocus();
    nameEdit->selectAll();
}

void CategoriesSettings::closeEditor() {
    editedId = -1;
    rebuildList();
}

void CategoriesSettings::selectColor(const QString& color) {
    for (QAbstractButton* button : colorGroup->buttons()) {
        if (static_cast<SwatchButton*>(button)->color == color) button->setChecked(true);
    }
}

QString CategoriesSettings::selectedColor() const {
    auto* checked = static_cast<SwatchButton*>(colorGroup->checkedButton());
    return checked ? checked->color : QString();
}

void CategoriesSettings::save() {
    const QString name = nameEdit->text().simplified();
    auto fail = [this](const QString& message) {
        errorLabel->setText(message);
        errorLabel->show();
        nameEdit->setFocus();
    };
    if (name.isEmpty()) {
        fail("Enter a name.");
        return;
    }
    for (const Category& other : categories) {
        if (other.getCategoryId() != editedId && other.getName().compare(name, Qt::CaseInsensitive) == 0) {
            fail(QString("\"%1\" already exists. Use \"Merge into…\" to combine two categories.").arg(other.getName()));
            return;
        }
    }
    CategoryDAO dao(userId);
    const QString type = typeCombo->currentData().toString();
    const bool saved = editedId > 0 ? dao.editCategory(editedId, name, type, selectedColor())
                                    : dao.addCategory(Category(-1, name, type, selectedColor())) > 0;
    if (!saved) {
        fail("The category couldn't be saved.");
        return;
    }
    editedId = -1;
    reload();
    emit categoriesChanged();
}

void CategoriesSettings::mergeInto(int intoId) {
    const Category* from = category(editedId);
    const Category* into = category(intoId);
    if (!from || !into) return;
    const CategoryUsage fromUsage = usage.value(from->getCategoryId());
    QString details = QString("Its %1 %2 move to \"%3\".").arg(fromUsage.transactions)
                          .arg(fromUsage.transactions == 1 ? "transaction" : "transactions", into->getName());
    if (fromUsage.budgets > 0) {
        details += usage.value(intoId).budgets > 0
                       ? QString(" Its budgets are removed, \"%1\" keeps its own.").arg(into->getName())
                       : QString(" Its budgets move along.");
    }
    if (!confirm("Merge categories", QString("Merge \"%1\" into \"%2\"?\n\n%3 \"%1\" is then deleted. This can't be undone.")
                                         .arg(from->getName(), into->getName(), details))) {
        return;
    }
    if (!CategoryDAO(userId).mergeCategory(from->getCategoryId(), intoId)) {
        errorLabel->setText("The categories couldn't be merged.");
        errorLabel->show();
        return;
    }
    editedId = -1;
    reload();
    emit categoriesChanged();
}

void CategoriesSettings::remove() {
    const Category* edited = category(editedId);
    if (!edited) return;
    const CategoryUsage used = usage.value(edited->getCategoryId());
    QString question = QString("Delete \"%1\"?").arg(edited->getName());
    if (used.transactions > 0 || used.budgets > 0) {
        question += QString("\n\nIt's used by %1. Those transactions keep no category, and its budgets are removed. "
                            "To move them to another category instead, use \"Merge into…\".").arg(usageText(used));
    }
    if (!confirm("Delete category", question)) {
        return;
    }
    if (!CategoryDAO(userId).deleteCategoryKeepingTransactions(edited->getCategoryId())) {
        errorLabel->setText("The category couldn't be deleted.");
        errorLabel->show();
        return;
    }
    editedId = -1;
    reload();
    emit categoriesChanged();
}
