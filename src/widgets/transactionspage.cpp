#include "transactionspage.h"
#include "account.h"
#include "categorycolors.h"
#include "dialog/balancecorrectiondialog.h"
#include "dialog/importmanagerdialog.h"
#include "icons.h"
#include "settings.h"
#include "theme.h"
#include "transactioneditor.h"
#include "transactionlistmodel.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QAction>
#include <QApplication>
#include <QEvent>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <functional>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QVBoxLayout>

// Draws day headers and transaction rows: payee and description, account, category pill, amount
class TransactionDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    bool showAccountColumn = true;
    bool showCheckboxes = false;
    int openTransactionId = -1;                      // Row shown in the editor (highlighted, but not selected)

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        Q_UNUSED(option);
        return QSize(0, isHeader(index) ? 36 : 48);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        const Theme::Colors colors = Theme::colors();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);

        if (isHeader(index)) {
            paintHeader(painter, option, index, colors);
            painter->restore();
            return;
        }

        // Row background
        const QRect row = rowRect(option.rect);
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;
        const bool open = index.data(TransactionListModel::TransactionIdRole).toInt() == openTransactionId;
        if (selected || open || hovered) {
            QPainterPath path;
            path.addRoundedRect(QRectF(row), 6, 6);
            painter->fillPath(path, (selected || open) ? colors.itemSelected : colors.itemHover);
        }

        // Selection checkbox (optional, see Settings → Transactions)
        if (showCheckboxes) {
            QStyleOptionButton checkbox;
            checkbox.rect = checkboxRect(option.rect);
            checkbox.state = QStyle::State_Enabled | (selected ? QStyle::State_On : QStyle::State_Off);
            if (hovered) checkbox.state |= QStyle::State_MouseOver;
            const QWidget* widget = option.widget;
            QStyle* style = widget ? widget->style() : QApplication::style();
            style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &checkbox, painter, widget);
        }

        const QFontMetrics metrics(option.font);
        QFont smallFont = option.font;
        smallFont.setPointSizeF(option.font.pointSizeF() * 0.9);
        const QFontMetrics smallMetrics(smallFont);

        // Amount column on the right
        const Cents amount = index.data(TransactionListModel::SignedAmountRole).toLongLong();
        const int amountWidth = 130;
        QRect amountRect(row.right() - 12 - amountWidth, row.top(), amountWidth, row.height());
        painter->setFont(option.font);
        painter->setPen(amount > 0 ? colors.positive : colors.textPrimary);
        painter->drawText(amountRect, Qt::AlignRight | Qt::AlignVCenter,
                          Settings::instance().formatCurrency(amount, amount > 0));
        int right = amountRect.left() - 16;

        // Category pill
        const int categoryWidth = 160;
        const QString category = index.data(TransactionListModel::CategoryRole).toString();
        if (!category.isEmpty()) {
            painter->setFont(smallFont);
            const QString text = smallMetrics.elidedText(category, Qt::ElideRight, categoryWidth - 16);
            const int pillWidth = smallMetrics.horizontalAdvance(text) + 16;
            const QRectF pill(right - categoryWidth, row.center().y() - 10, pillWidth, 20);
            QPainterPath pillPath;
            pillPath.addRoundedRect(pill, 10, 10);
            const CategoryColors::Swatch swatch = CategoryColors::swatch(index.data(TransactionListModel::CategoryColorRole).toString());
            painter->fillPath(pillPath, swatch.background);
            painter->setPen(swatch.text);
            painter->drawText(pill, Qt::AlignCenter, text);
        }
        right -= categoryWidth + 12;

        // Account column (only when several accounts are shown)
        if (showAccountColumn) {
            const int accountWidth = 130;
            painter->setFont(smallFont);
            painter->setPen(colors.textSecondary);
            painter->drawText(QRect(right - accountWidth, row.top(), accountWidth, row.height()),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              smallMetrics.elidedText(index.data(TransactionListModel::AccountRole).toString(),
                                                      Qt::ElideRight, accountWidth));
            right -= accountWidth + 12;
        }

        // Payee with the description underneath
        const int left = showCheckboxes ? checkboxRect(option.rect).right() + 14 : row.left() + 12;
        const int textWidth = qMax(0, right - left);
        const QString payee = index.data(TransactionListModel::PayeeRole).toString();
        const QString description = index.data(TransactionListModel::DescriptionRole).toString().simplified();
        const QRect payeeRect(left, row.top(), textWidth, description.isEmpty() ? row.height() : row.height() / 2 + 2);

        painter->setFont(option.font);
        painter->setPen(payee.isEmpty() ? colors.textMuted : colors.textPrimary);
        painter->drawText(payeeRect, Qt::AlignLeft | (description.isEmpty() ? Qt::AlignVCenter : Qt::AlignBottom),
                          metrics.elidedText(payee.isEmpty() ? QString("No payee") : payee, Qt::ElideRight, textWidth));
        if (!description.isEmpty()) {
            painter->setFont(smallFont);
            painter->setPen(colors.textSecondary);
            painter->drawText(QRect(left, payeeRect.bottom() + 2, textWidth, row.height() / 2 - 4),
                              Qt::AlignLeft | Qt::AlignTop,
                              smallMetrics.elidedText(description, Qt::ElideRight, textWidth));
        }
        painter->restore();
    }

    static bool isHeader(const QModelIndex& index) {
        return index.data(TransactionListModel::KindRole).toInt() == static_cast<int>(TransactionListModel::Kind::DayHeader);
    }

    static QRect rowRect(const QRect& itemRect) {
        return itemRect.adjusted(6, 1, -6, -1);
    }

    // Clicks inside this square toggle the selection instead of opening the editor
    static QRect checkboxRect(const QRect& itemRect) {
        const QRect row = rowRect(itemRect);
        return QRect(row.left() + 10, row.center().y() - 8, 16, 16);
    }

private:
    void paintHeader(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index,
                     const Theme::Colors& colors) const {
        const bool collapsed = index.data(TransactionListModel::CollapsedRole).toBool();
        const QRect area = option.rect.adjusted(6, 4, -6, 0);
        if (option.state & QStyle::State_MouseOver) {
            QPainterPath path;
            path.addRoundedRect(QRectF(area), 6, 6);
            painter->fillPath(path, colors.itemHover);
        }

        const qreal dpr = painter->device()->devicePixelRatioF();
        const int iconSize = 14;
        painter->drawPixmap(area.left() + 11, area.center().y() - iconSize / 2,
                            Icons::tinted(collapsed ? "chevron-right" : "chevron-down", colors.textSecondary, iconSize, dpr));

        QFont headerFont = option.font;
        headerFont.setPointSizeF(option.font.pointSizeF() * 0.9);
        headerFont.setWeight(QFont::DemiBold);
        painter->setFont(headerFont);
        painter->setPen(colors.textSecondary);
        const QRect textRect = area.adjusted(36, 0, -18, 0);
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter,
                          dayLabel(index.data(TransactionListModel::DateRole).toDate()));

        // Folded days summarise what they hide
        if (collapsed) {
            const int count = index.data(TransactionListModel::DayCountRole).toInt();
            const Cents net = index.data(TransactionListModel::DayNetRole).toLongLong();
            painter->setFont(option.font);
            painter->setPen(colors.textSecondary);
            painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter,
                              QString("%1 %2 · %3").arg(count).arg(count == 1 ? "transaction" : "transactions",
                                                        Settings::instance().formatCurrency(net, net > 0)));
        }
    }

    static QString dayLabel(const QDate& date) {
        const QDate today = QDate::currentDate();
        const QLocale english(QLocale::English);
        if (date == today) return "Today";
        if (date == today.addDays(-1)) return "Yesterday";
        return english.toString(date, date.year() == today.year() ? "dddd, d MMMM" : "dddd, d MMMM yyyy");
    }
};

// List view with checkbox clicks (toggle selection, no editor) and foldable day headers
class TransactionListView : public QListView {
public:
    using QListView::QListView;

    std::function<void(const QModelIndex&, Qt::KeyboardModifiers)> onHeaderClicked;
    bool checkboxesEnabled = false;
    // Keys held during the click that emits clicked(), from the event itself (like
    // selectionCommand), not from the app-wide state, which can lag behind the event
    Qt::KeyboardModifiers clickModifiers;

protected:
    // Only Ctrl/Shift change the selection. A plain click opens the row (and clears an existing
    // selection); plain dragging and arrow keys never select.
    QItemSelectionModel::SelectionFlags selectionCommand(const QModelIndex& index, const QEvent* event) const override {
        Qt::KeyboardModifiers modifiers = QGuiApplication::keyboardModifiers();
        if (event && (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseMove
                      || event->type() == QEvent::MouseButtonRelease || event->type() == QEvent::KeyPress)) {
            modifiers = static_cast<const QInputEvent*>(event)->modifiers();
        }
        if (!(modifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
            return (event && event->type() == QEvent::MouseButtonPress) ? QItemSelectionModel::Clear
                                                                        : QItemSelectionModel::NoUpdate;
        }
        return QListView::selectionCommand(index, event);
    }

    bool isOnCheckbox(const QModelIndex& index, const QPoint& pos) const {
        return checkboxesEnabled && TransactionDelegate::checkboxRect(visualRect(index)).contains(pos);
    }

    void mousePressEvent(QMouseEvent* event) override {
        const QModelIndex index = indexAt(event->position().toPoint());
        if (event->button() == Qt::LeftButton && index.isValid()) {
            if (TransactionDelegate::isHeader(index)) {
                consumedPress = true;
                if (onHeaderClicked) onHeaderClicked(index, event->modifiers());
                return;
            }
            if (isOnCheckbox(index, event->position().toPoint())) {
                consumedPress = true;
                selectionModel()->select(index, QItemSelectionModel::Toggle);
                selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
                return;
            }
        }
        consumedPress = false;
        QListView::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override {
        if (consumedPress) {
            consumedPress = false;                   // Don't emit clicked() for checkbox or header presses
            return;
        }
        clickModifiers = event->modifiers();         // clicked() is emitted during the release
        QListView::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override {
        const QModelIndex index = indexAt(event->position().toPoint());
        if (index.isValid() && (TransactionDelegate::isHeader(index)
                                || isOnCheckbox(index, event->position().toPoint()))) {
            mousePressEvent(event);                  // Treat a fast second click like a normal one
            return;
        }
        QListView::mouseDoubleClickEvent(event);
    }

private:
    bool consumedPress = false;
};

namespace {

// Gives a push button the accent colour used for the main action of a view
void makePrimary(QPushButton* button) {
    QPalette palette = button->palette();
    palette.setColor(QPalette::Button, Theme::colors().accent);
    palette.setColor(QPalette::ButtonText, Theme::colors().onAccent);
    button->setPalette(palette);
}

} // namespace

TransactionsPage::TransactionsPage(QWidget* parent)
    : QWidget(parent) {
    confirm = [this](const QString& title, const QString& text) {
        return QMessageBox::question(this, title, text, QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
               == QMessageBox::Yes;
    };
    model = new TransactionListModel(this);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 12);
    root->setSpacing(12);

    // Header: shown account, balance and the main actions
    auto* header = new QHBoxLayout();
    auto* titles = new QVBoxLayout();
    titles->setSpacing(2);
    titleLabel = new QLabel(this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleLabel->setFont(titleFont);
    subtitleLabel = new QLabel(this);
    titles->addWidget(titleLabel);
    titles->addWidget(subtitleLabel);
    header->addLayout(titles, 1);

    correctBalanceButton = new QPushButton("Correct balance", this);
    correctBalanceButton->setObjectName("correctBalanceButton");
    correctBalanceButton->setToolTip("Match the balance your bank shows by adding a balance correction");
    connect(correctBalanceButton, &QPushButton::clicked, this, [this]() { correctBalance(accountId); });
    header->addWidget(correctBalanceButton, 0, Qt::AlignTop);
    importButton = new QPushButton("Import", this);
    connect(importButton, &QPushButton::clicked, this, &TransactionsPage::importTransactions);
    header->addWidget(importButton, 0, Qt::AlignTop);
    auto* addButton = new QPushButton("Add transaction", this);
    addButton->setIcon(Icons::icon("plus", Theme::colors().onAccent));
    addButton->setToolTip("Add transaction (Ctrl+N)");
    makePrimary(addButton);
    connect(addButton, &QPushButton::clicked, this, &TransactionsPage::addTransaction);
    header->addWidget(addButton, 0, Qt::AlignTop);
    root->addLayout(header);

    // Filters: search, period, category, unassigned transactions
    auto* filters = new QHBoxLayout();
    filters->setSpacing(6);
    searchEdit = new QLineEdit(this);
    searchEdit->setObjectName("searchEdit");
    searchEdit->setPlaceholderText("Search payee, description, category or account  (Ctrl+F)");
    searchEdit->setClearButtonEnabled(true);
    searchAction = searchEdit->addAction(QIcon(), QLineEdit::LeadingPosition);
    connect(searchEdit, &QLineEdit::textChanged, this, &TransactionsPage::applyFilter);
    filters->addWidget(searchEdit, 1);
    filters->addSpacing(6);

    periodGroup = new QButtonGroup(this);
    periodGroup->setExclusive(true);
    const QList<QPair<QString, Period>> periods = {
        {"All time", Period::AllTime}, {"This month", Period::ThisMonth},
        {"Last month", Period::LastMonth}, {"This year", Period::ThisYear}
    };
    for (const auto& period : periods) {
        QToolButton* chip = addChip(period.first, filters);
        periodGroup->addButton(chip, static_cast<int>(period.second));
    }
    periodGroup->button(static_cast<int>(Period::AllTime))->setChecked(true);
    connect(periodGroup, &QButtonGroup::idClicked, this, &TransactionsPage::applyFilter);

    categoryFilter = new QComboBox(this);
    categoryFilter->setObjectName("categoryFilter");
    categoryFilter->setMinimumWidth(150);
    connect(categoryFilter, &QComboBox::currentIndexChanged, this, &TransactionsPage::applyFilter);
    filters->addWidget(categoryFilter);

    unassignedChip = addChip(QString(), filters);
    unassignedChip->setToolTip("Transactions that don't belong to an account yet, for example from an import");
    connect(unassignedChip, &QToolButton::toggled, this, [this](bool checked) {
        showingUnassigned = checked;
        closeEditor();
        reload();
    });
    root->addLayout(filters);

    // Bulk actions for multiple selected transactions
    bulkBar = new QFrame(this);
    bulkBar->setObjectName("bulkBar");
    bulkBar->setFrameShape(QFrame::StyledPanel);
    auto* bulkLayout = new QHBoxLayout(bulkBar);
    bulkLayout->setContentsMargins(12, 4, 8, 4);
    bulkLabel = new QLabel(bulkBar);
    bulkLayout->addWidget(bulkLabel);
    bulkLayout->addStretch();
    bulkCategoryButton = new QToolButton(bulkBar);
    bulkCategoryButton->setObjectName("bulkCategoryButton");
    bulkCategoryButton->setText("Set category");
    bulkCategoryButton->setPopupMode(QToolButton::InstantPopup);
    bulkCategoryButton->setMenu(new QMenu(bulkCategoryButton));
    bulkLayout->addWidget(bulkCategoryButton);
    auto* bulkDeleteButton = new QToolButton(bulkBar);
    bulkDeleteButton->setText("Delete");
    connect(bulkDeleteButton, &QToolButton::clicked, this, &TransactionsPage::deleteSelected);
    bulkLayout->addWidget(bulkDeleteButton);
    auto* clearSelectionButton = new QToolButton(bulkBar);
    clearSelectionButton->setText("Clear selection");
    clearSelectionButton->setToolTip("Clear selection (Esc)");
    connect(clearSelectionButton, &QToolButton::clicked, this, [this]() { listView->clearSelection(); });
    bulkLayout->addWidget(clearSelectionButton);
    bulkBar->hide();
    root->addWidget(bulkBar);

    // List (or empty state) with the editor panel on the right
    auto* body = new QHBoxLayout();
    body->setSpacing(12);
    listStack = new QStackedWidget(this);

    auto* transactionList = new TransactionListView(this);
    transactionList->onHeaderClicked = [this](const QModelIndex& index, Qt::KeyboardModifiers modifiers) {
        // Plain click folds the day; Ctrl/Shift select its transactions like they do for rows
        if (modifiers & Qt::ShiftModifier) {
            selectDay(index, true);
        } else if (modifiers & Qt::ControlModifier) {
            selectDay(index, false);
        } else {
            model->toggleDay(index.data(TransactionListModel::DateRole).toDate());
        }
    };
    listView = transactionList;
    listView->setObjectName("transactionList");
    listView->setModel(model);
    delegate = new TransactionDelegate(listView);
    listView->setItemDelegate(delegate);
    delegate->showCheckboxes = Settings::instance().isSelectionCheckboxesEnabled();
    transactionList->checkboxesEnabled = delegate->showCheckboxes;
    listView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    listView->setMouseTracking(true);
    listView->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    listView->setFrameShape(QFrame::StyledPanel);
    connect(listView->selectionModel(), &QItemSelectionModel::selectionChanged, this, &TransactionsPage::onSelectionChanged);
    // A plain click (or Enter) on a row opens it; Ctrl/Shift-clicks only extend the selection
    connect(listView, &QListView::clicked, this, [this, transactionList](const QModelIndex& index) {
        if (!(transactionList->clickModifiers & (Qt::ControlModifier | Qt::ShiftModifier))) {
            openEditor(index);
        }
    });
    connect(listView, &QListView::activated, this, &TransactionsPage::openEditor);
    listStack->addWidget(listView);

    emptyLabel = new QLabel(this);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setWordWrap(true);
    listStack->addWidget(emptyLabel);
    body->addWidget(listStack, 1);

    editor = new TransactionEditor(this);
    editor->hide();
    connect(editor, &TransactionEditor::saveRequested, this, &TransactionsPage::onSaveRequested);
    connect(editor, &TransactionEditor::deleteRequested, this, [this](int transactionId) {
        deleteTransactions({transactionId});
    });
    connect(editor, &TransactionEditor::closed, this, &TransactionsPage::closeEditor);
    body->addWidget(editor);
    root->addLayout(body, 1);

    // Totals of what is shown
    auto* footer = new QHBoxLayout();
    footer->addStretch();
    incomeLabel = new QLabel(this);
    expensesLabel = new QLabel(this);
    netLabel = new QLabel(this);
    for (QLabel* label : {incomeLabel, expensesLabel, netLabel}) {
        footer->addSpacing(16);
        footer->addWidget(label);
    }
    root->addLayout(footer);

    // Keyboard: Ctrl+N adds, Delete removes the selected transactions
    auto* addShortcut = new QShortcut(QKeySequence::New, this);
    addShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(addShortcut, &QShortcut::activated, this, &TransactionsPage::addTransaction);
    auto* deleteShortcut = new QShortcut(QKeySequence::Delete, listView);
    deleteShortcut->setContext(Qt::WidgetShortcut);
    connect(deleteShortcut, &QShortcut::activated, this, &TransactionsPage::deleteSelected);
    auto* clearShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), listView);
    clearShortcut->setContext(Qt::WidgetShortcut);
    connect(clearShortcut, &QShortcut::activated, listView, &QListView::clearSelection);
    auto* findShortcut = new QShortcut(QKeySequence::Find, this);
    findShortcut->setContext(Qt::WidgetWithChildrenShortcut);
    connect(findShortcut, &QShortcut::activated, this, [this]() {
        searchEdit->setFocus();
        searchEdit->selectAll();
    });

    updateIcons();
}

QToolButton* TransactionsPage::addChip(const QString& text, QLayout* layout) {
    auto* chip = new QToolButton(this);
    chip->setText(text);
    chip->setCheckable(true);
    chip->setCursor(Qt::PointingHandCursor);
    Theme::highlightWhenChecked(chip);
    layout->addWidget(chip);
    return chip;
}

void TransactionsPage::updateIcons() {
    importButton->setIcon(Icons::icon("import", palette().color(QPalette::ButtonText)));
    searchAction->setIcon(Icons::icon("search", Theme::colors().textSecondary));
}

void TransactionsPage::changeEvent(QEvent* event) {
    if (event->type() == QEvent::PaletteChange && searchAction && incomeLabel) {
        updateIcons();
        updateTotals();                              // Income colour is part of the label text
    }
    QWidget::changeEvent(event);
}

void TransactionsPage::setUserId(int newUserId) {
    userId = newUserId;
    accountId = ALL_ACCOUNTS;
    closeEditor();
    reload();
}

void TransactionsPage::showAccount(int newAccountId) {
    accountId = newAccountId;
    if (showingUnassigned) {
        const QSignalBlocker blocker(unassignedChip);
        unassignedChip->setChecked(false);
        showingUnassigned = false;
    }
    closeEditor();
    reload();
}

void TransactionsPage::applySettings() {
    const bool checkboxes = Settings::instance().isSelectionCheckboxesEnabled();
    delegate->showCheckboxes = checkboxes;
    static_cast<TransactionListView*>(listView)->checkboxesEnabled = checkboxes;
    editor->applySettings();
    listView->viewport()->update();
    updateHeader();
    updateTotals();
}

void TransactionsPage::reload() {
    if (userId == -1) {
        return;
    }

    AccountDAO accountDao;
    CategoryDAO categoryDao(userId);
    TransactionDAO transactionDao;

    const QList<Account> userAccounts = accountDao.getAllAccountsForUser(userId);
    QHash<int, QString> accountNames;
    accounts.clear();
    for (const Account& account : userAccounts) {
        accountNames.insert(account.getAccountId(), account.getName());
        accounts.append({account.getAccountId(), account.getName()});
    }
    if (accountId != ALL_ACCOUNTS && !accountNames.contains(accountId)) {
        accountId = ALL_ACCOUNTS;                    // The account was deleted
    }

    categories = categoryDao.getAllCategories();
    QHash<int, QString> categoryNames;
    QHash<int, QString> categoryColors;
    for (const Category& category : categories) {
        categoryNames.insert(category.getCategoryId(), category.getName());
        categoryColors.insert(category.getCategoryId(), category.getColor());
    }

    const QList<Transaction> unassigned = transactionDao.getShadowedTransactions();
    unassignedCount = unassigned.size();

    QList<Transaction> transactions;
    if (showingUnassigned) {
        transactions = unassigned;
    } else {
        for (const Account& account : userAccounts) {
            if (accountId == ALL_ACCOUNTS || account.getAccountId() == accountId) {
                transactions.append(transactionDao.getTransactionsForAccount(account.getAccountId()));
            }
        }
    }

    // Keep the open editor and selection across the reload
    const QList<int> selectedIds = selectedTransactionIds();

    delegate->showAccountColumn = showingUnassigned || accountId == ALL_ACCOUNTS;
    model->setCategoryColors(categoryColors);
    model->setTransactions(transactions, accountNames, categoryNames);
    populateCategoryFilter();
    applyFilter();

    editor->setAccounts(accounts);
    editor->setCategories(categories);
    editor->setPayeeSuggestions(model->payees());
    editor->setRules(RuleDAO(userId).rules());

    unassignedChip->setText(QString("Unassigned (%1)").arg(unassignedCount));
    unassignedChip->setVisible(unassignedCount > 0 || showingUnassigned);

    const QSignalBlocker blocker(listView->selectionModel());
    for (int transactionId : selectedIds) {
        const QModelIndex index = model->indexForTransaction(transactionId);
        if (index.isValid()) {
            listView->selectionModel()->select(index, QItemSelectionModel::Select);
        }
    }
    updateBulkBar();
}

void TransactionsPage::populateCategoryFilter() {
    const int selected = categoryFilter->currentIndex() >= 0 ? categoryFilter->currentData().toInt()
                                                             : TransactionFilter::ANY_CATEGORY;
    const QSignalBlocker blocker(categoryFilter);
    categoryFilter->clear();
    categoryFilter->addItem("All categories", TransactionFilter::ANY_CATEGORY);
    for (const Category& category : categories) {
        categoryFilter->addItem(category.getName(), category.getCategoryId());
    }
    categoryFilter->addItem("No category", TransactionFilter::NO_CATEGORY);
    const int index = categoryFilter->findData(selected);
    categoryFilter->setCurrentIndex(index >= 0 ? index : 0);

    // Bulk "Set category" menu
    QMenu* menu = bulkCategoryButton->menu();
    menu->clear();
    for (const Category& category : categories) {
        const int categoryId = category.getCategoryId();
        menu->addAction(category.getName(), this, [this, categoryId]() { setCategoryForSelection(categoryId); });
    }
    menu->addSeparator();
    menu->addAction("No category", this, [this]() { setCategoryForSelection(TransactionFilter::NO_CATEGORY); });
}

void TransactionsPage::applyFilter() {
    TransactionFilter filter;
    filter.search = searchEdit->text();
    filter.categoryId = categoryFilter->currentIndex() >= 0 ? categoryFilter->currentData().toInt()
                                                            : TransactionFilter::ANY_CATEGORY;

    const QDate today = QDate::currentDate();
    const QDate monthStart(today.year(), today.month(), 1);
    switch (static_cast<Period>(periodGroup->checkedId())) {
    case Period::ThisMonth:
        filter.from = monthStart;
        filter.to = monthStart.addMonths(1).addDays(-1);
        break;
    case Period::LastMonth:
        filter.from = monthStart.addMonths(-1);
        filter.to = monthStart.addDays(-1);
        break;
    case Period::ThisYear:
        filter.from = QDate(today.year(), 1, 1);
        filter.to = QDate(today.year(), 12, 31);
        break;
    case Period::AllTime:
        break;
    }

    model->setFilter(filter);

    const bool empty = model->totals().count == 0;
    if (empty) {
        const bool filtered = !filter.search.trimmed().isEmpty() || filter.from.isValid()
                              || filter.categoryId != TransactionFilter::ANY_CATEGORY;
        emptyLabel->setText(filtered ? "No transactions match these filters."
                                     : "No transactions yet.\nAdd one with \"Add transaction\" or import a file.");
    }
    listStack->setCurrentWidget(empty ? static_cast<QWidget*>(emptyLabel) : listView);
    updateHeader();
    updateTotals();
}

void TransactionsPage::updateHeader() {
    if (showingUnassigned) {
        titleLabel->setText("Unassigned transactions");
        subtitleLabel->setText(QString("%1 transactions without an account. Open one to assign it.").arg(unassignedCount));
        return;
    }

    AccountDAO accountDao;
    Cents balance = 0;
    QString title = "All accounts";
    for (const Account& account : accountDao.getAllAccountsForUser(userId)) {
        if (accountId == ALL_ACCOUNTS || account.getAccountId() == accountId) {
            balance += account.getBalanceCents();
            if (account.getAccountId() == accountId) {
                title = account.getName();
            }
        }
    }
    const int count = model->totals().count;
    titleLabel->setText(title);
    subtitleLabel->setText(QString("Balance %1 · %2 %3")
                               .arg(Settings::instance().formatCurrency(balance))
                               .arg(count)
                               .arg(count == 1 ? "transaction" : "transactions"));
}

void TransactionsPage::updateTotals() {
    const TransactionListModel::Totals totals = model->totals();
    const Settings& settings = Settings::instance();
    const Theme::Colors colors = Theme::colors();
    incomeLabel->setText(QString("Income <span style='color:%1'>%2</span>")
                             .arg(colors.positive.name(), settings.formatCurrency(totals.income, true)));
    expensesLabel->setText(QString("Expenses <b>%1</b>").arg(settings.formatCurrency(-totals.expenses)));
    netLabel->setText(QString("Net <b>%1</b>").arg(settings.formatCurrency(totals.net(), true)));
}

QList<int> TransactionsPage::selectedTransactionIds() const {
    QList<int> ids;
    for (const QModelIndex& index : listView->selectionModel()->selectedIndexes()) {
        ids.append(index.data(TransactionListModel::TransactionIdRole).toInt());
    }
    return ids;
}

void TransactionsPage::updateBulkBar() {
    // Bulk actions whenever something is selected (opening a row in the editor doesn't select it)
    const int count = selectedTransactionIds().size();
    QString text = QString("%1 %2 selected").arg(count).arg(count == 1 ? "transaction" : "transactions");
    if (count == 1 && !delegate->showCheckboxes) {
        text += "  ·  Ctrl+click or Shift+click to select more";
    }
    bulkLabel->setText(text);
    bulkBar->setVisible(count > 0);
}

void TransactionsPage::onSelectionChanged() {
    // Selecting transactions switches from editing one to acting on several
    if (!selectedTransactionIds().isEmpty() && !editor->isHidden()) {
        closeEditor();
    }
    updateBulkBar();
}

void TransactionsPage::selectDay(const QModelIndex& header, bool extendRange) {
    const QDate date = header.data(TransactionListModel::DateRole).toDate();
    if (model->isDayCollapsed(date)) {
        model->toggleDay(date);                      // Show what gets selected
    }

    // The day's transaction rows directly follow its header
    const int firstRow = header.row() + 1;
    int lastRow = firstRow;
    while (lastRow + 1 < model->rowCount()
           && model->index(lastRow + 1).data(TransactionListModel::KindRole).toInt()
                  == static_cast<int>(TransactionListModel::Kind::Transaction)) {
        ++lastRow;
    }
    if (firstRow >= model->rowCount()) {
        return;
    }
    const QModelIndex first = model->index(firstRow);
    const QModelIndex last = model->index(lastRow);
    QItemSelectionModel* selection = listView->selectionModel();

    if (extendRange) {
        // Shift: from the last clicked transaction through the whole day (headers are skipped automatically)
        const QModelIndex anchor = selection->currentIndex().isValid() ? selection->currentIndex() : first;
        const QModelIndex end = anchor.row() <= header.row() ? last : first;
        selection->select(QItemSelection(anchor.row() <= end.row() ? anchor : end,
                                         anchor.row() <= end.row() ? end : anchor),
                          QItemSelectionModel::ClearAndSelect);
        return;
    }

    // Ctrl: add the day, or remove it if it is fully selected already
    bool allSelected = true;
    for (int row = firstRow; row <= lastRow; ++row) {
        allSelected = allSelected && selection->isSelected(model->index(row));
    }
    selection->select(QItemSelection(first, last),
                      allSelected ? QItemSelectionModel::Deselect : QItemSelectionModel::Select);
    selection->setCurrentIndex(last, QItemSelectionModel::NoUpdate);
}

void TransactionsPage::openEditor(const QModelIndex& index) {
    const int transactionId = index.data(TransactionListModel::TransactionIdRole).toInt();
    const auto transaction = model->transaction(transactionId);
    if (!transaction) {
        return;
    }
    listView->clearSelection();
    listView->selectionModel()->setCurrentIndex(index, QItemSelectionModel::NoUpdate);
    delegate->openTransactionId = transactionId;
    listView->viewport()->update();
    editor->editTransaction(*transaction);
    editor->show();
    updateBulkBar();
}

void TransactionsPage::showCategoryThisMonth(int categoryId) {
    showAccount(ALL_ACCOUNTS);
    {
        const QSignalBlocker searchBlocker(searchEdit);
        const QSignalBlocker categoryBlocker(categoryFilter);
        searchEdit->clear();
        const int index = categoryFilter->findData(categoryId);
        categoryFilter->setCurrentIndex(index >= 0 ? index : 0);
    }
    periodGroup->button(static_cast<int>(Period::ThisMonth))->setChecked(true);
    applyFilter();
}

void TransactionsPage::openTransaction(int transactionId) {
    // Clear filters that could hide it, and unfold its day
    showAccount(ALL_ACCOUNTS);
    {
        const QSignalBlocker searchBlocker(searchEdit);
        const QSignalBlocker categoryBlocker(categoryFilter);
        searchEdit->clear();
        categoryFilter->setCurrentIndex(0);
    }
    periodGroup->button(static_cast<int>(Period::AllTime))->setChecked(true);
    applyFilter();
    if (const auto transaction = model->transaction(transactionId)) {
        if (model->isDayCollapsed(transaction->getDate())) {
            model->toggleDay(transaction->getDate());
        }
    }
    const QModelIndex index = model->indexForTransaction(transactionId);
    if (index.isValid()) {
        listView->scrollTo(index, QAbstractItemView::PositionAtCenter);
        openEditor(index);
    }
}

void TransactionsPage::addTransaction() {
    {
        const QSignalBlocker blocker(listView->selectionModel());
        listView->clearSelection();
    }
    delegate->openTransactionId = -1;
    listView->viewport()->update();
    updateBulkBar();
    editor->newTransaction(QDate::currentDate(), defaultAccountForNew());
    editor->show();
}

int TransactionsPage::defaultAccountForNew() const {
    if (accountId != ALL_ACCOUNTS) {
        return accountId;
    }
    return accounts.isEmpty() ? -1 : accounts.first().first;
}

void TransactionsPage::onSaveRequested(const Transaction& transaction) {
    TransactionDAO transactionDao;
    const bool isNew = transaction.getTransactionId() < 0;
    int savedId = transaction.getTransactionId();

    if (isNew) {
        savedId = transactionDao.addTransaction(transaction);
        if (savedId < 0) {
            editor->showError("Couldn't save the transaction. See the log for details.");
            return;
        }
    } else if (!transactionDao.updateTransaction(transaction)) {
        editor->showError("Couldn't save the changes. See the log for details.");
        return;
    }

    if (isNew && Settings::instance().isFastEntryEnabled()) {
        // Fast entry: start the next transaction with the same date and account
        editor->newTransaction(transaction.getDate(), transaction.getAccountId());
    } else {
        closeEditor();
    }
    reload();
    emit transactionsChanged();

    const QModelIndex savedIndex = model->indexForTransaction(savedId);
    if (savedIndex.isValid()) {
        listView->scrollTo(savedIndex);
    }
}

void TransactionsPage::correctBalance(int forAccountId) {
    BalanceCorrectionDialog dialog(userId, forAccountId, this);
    if (dialog.exec() == QDialog::Accepted) {
        reload();
        emit transactionsChanged();
    }
}

void TransactionsPage::importTransactions() {
    ImportManagerDialog dialog(userId, this);
    connect(&dialog, &ImportManagerDialog::transactionsImported, this, [this]() {
        reload();                                    // Also sent for a balance correction without new rows
        emit transactionsChanged();
    });
    // A new account shows up in the sidebar and filters even if the import is cancelled afterwards
    connect(&dialog, &ImportManagerDialog::accountCreated, this, [this]() {
        reload();
        emit transactionsChanged();
    });
    dialog.exec();
}

void TransactionsPage::deleteSelected() {
    const QList<int> ids = selectedTransactionIds();
    if (!ids.isEmpty()) {
        deleteTransactions(ids);
    }
}

void TransactionsPage::deleteTransactions(const QList<int>& transactionIds) {
    const QString question = transactionIds.size() == 1
        ? QString("Delete this transaction? This can't be undone.")
        : QString("Delete %1 transactions? This can't be undone.").arg(transactionIds.size());
    if (!confirm("Delete transactions", question)) {
        return;
    }

    TransactionDAO transactionDao;
    for (int transactionId : transactionIds) {
        transactionDao.deleteTransaction(transactionId);
    }
    {
        const QSignalBlocker blocker(listView->selectionModel());
        listView->clearSelection();
    }
    closeEditor();
    reload();
    emit transactionsChanged();
}

void TransactionsPage::setCategoryForSelection(int categoryId) {
    TransactionDAO transactionDao;
    for (int transactionId : selectedTransactionIds()) {
        if (auto transaction = model->transaction(transactionId)) {
            transaction->setCategoryId(categoryId);
            transactionDao.updateTransaction(*transaction);
        }
    }
    reload();
    emit transactionsChanged();
}

void TransactionsPage::closeEditor() {
    editor->hide();
    delegate->openTransactionId = -1;
    listView->viewport()->update();
    updateBulkBar();
}
