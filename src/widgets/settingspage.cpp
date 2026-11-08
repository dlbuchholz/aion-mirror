#include "settingspage.h"
#include "dialog/rawsettingsdialog.h"
#include "aboutwidget.h"
#include "accounttypessettings.h"
#include "backupsettings.h"
#include "categoriessettings.h"
#include "reportspage.h"
#include "rulessettings.h"
#include "icons.h"
#include "language.h"
#include "settings.h"
#include "theme.h"

#include <QCheckBox>
#include <QEvent>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QStyledItemDelegate>
#include <QToolButton>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {

constexpr int ICON_ROLE = Qt::UserRole;

// Selects the combo box entry whose item data equals value
void selectByData(QComboBox* combo, const QVariant& value) {
    const int index = combo->findData(value);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    }
}

// Paints the section list like the sidebar: icon, label and a rounded highlight
class SectionDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        const Theme::Colors colors = Theme::colors();
        const bool selected = option.state & QStyle::State_Selected;
        const bool hovered = option.state & QStyle::State_MouseOver;

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        if (selected || hovered) {
            QPainterPath path;
            path.addRoundedRect(QRectF(option.rect).adjusted(0, 1, 0, -1), 6, 6);
            painter->fillPath(path, selected ? colors.itemSelected : colors.itemHover);
        }

        const QColor textColor = selected ? colors.textPrimary : colors.textSecondary;
        const int iconSize = 18;
        const QString icon = index.data(ICON_ROLE).toString();
        if (!icon.isEmpty()) {
            const qreal dpr = painter->device()->devicePixelRatioF();
            painter->drawPixmap(option.rect.left() + 10, option.rect.center().y() - iconSize / 2 + 1,
                                Icons::tinted(icon, textColor, iconSize, dpr));
        }

        const QRect textRect = option.rect.adjusted(38, 0, -8, 0);
        painter->setPen(textColor);
        painter->setFont(option.font);
        painter->drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft,
                          option.fontMetrics.elidedText(index.data().toString(), Qt::ElideRight, textRect.width()));
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override {
        return QSize(QStyledItemDelegate::sizeHint(option, index).width(), 36);
    }
};

} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 16);
    root->setSpacing(16);

    auto *title = new QLabel(tr("Settings"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    title->setFont(titleFont);
    root->addWidget(title);

    // Section list on the left, section content on the right
    auto *body = new QHBoxLayout();
    body->setSpacing(24);
    sectionList = new QListWidget(this);
    sectionList->setFrameShape(QFrame::NoFrame);
    sectionList->setFixedWidth(230);
    sectionList->setMouseTracking(true);
    sectionList->setFocusPolicy(Qt::NoFocus);
    sectionList->setItemDelegate(new SectionDelegate(sectionList));
    sectionList->viewport()->setAutoFillBackground(false);
    sectionStack = new QStackedWidget(this);
    body->addWidget(sectionList);
    body->addWidget(sectionStack, 1);
    root->addLayout(body, 1);

    buildCurrencySection();
    buildTransactionsSection();
    buildAppearanceSection();
    buildImportSection();
    buildCategoriesSection();
    buildAboutSection();

    connect(sectionList, &QListWidget::currentRowChanged, sectionStack, &QStackedWidget::setCurrentIndex);
    sectionList->setCurrentRow(0);

    // Bottom bar: restore defaults and developer settings on the left, apply / revert on the right
    auto *buttonBar = new QHBoxLayout();
    restoreDefaultsButton = new QToolButton(this);
    restoreDefaultsButton->setText(tr("Restore defaults"));
    restoreDefaultsButton->setToolTip(tr("Reset currency, transaction, appearance and import settings"));
    connect(restoreDefaultsButton, &QToolButton::clicked, this, &SettingsPage::restoreDefaults);
    advancedButton = new QToolButton(this);
    advancedButton->setText(tr("Advanced settings"));
    advancedButton->setToolTip(tr("Edit every stored setting as raw text"));
    connect(advancedButton, &QToolButton::clicked, this, &SettingsPage::openAdvancedSettings);
    for (QToolButton *button : {restoreDefaultsButton, advancedButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        buttonBar->addWidget(button);
    }
    updateIcons();
    buttonBar->addStretch();

    unsavedLabel = new QLabel(tr("Unsaved changes"), this);
    unsavedLabel->setObjectName("unsavedLabel");
    buttonBar->addWidget(unsavedLabel);
    revertButton = new QPushButton(tr("Revert"), this);
    revertButton->setObjectName("revertButton");
    connect(revertButton, &QPushButton::clicked, this, &SettingsPage::revert);
    buttonBar->addWidget(revertButton);
    applyButton = new QPushButton(tr("Apply"), this);
    applyButton->setObjectName("applyButton");
    applyButton->setDefault(true);
    connect(applyButton, &QPushButton::clicked, this, &SettingsPage::apply);
    buttonBar->addWidget(applyButton);
    root->addLayout(buttonBar);

    loadSettings();
}

void SettingsPage::addSection(const QString &title, const QString &icon, QWidget *content)
{
    auto *item = new QListWidgetItem(title, sectionList);
    item->setData(ICON_ROLE, icon);
    sectionStack->addWidget(content);
}

QFormLayout* SettingsPage::addFormSection(const QString &title, const QString &icon)
{
    auto *page = new QWidget(this);
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    auto *heading = new QLabel(title, page);
    QFont headingFont = heading->font();
    headingFont.setPointSize(11);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);

    auto *form = new QFormLayout();
    form->setFieldGrowthPolicy(QFormLayout::FieldsStayAtSizeHint);
    form->setVerticalSpacing(10);
    layout->addLayout(form);
    layout->addStretch();

    addSection(title, icon, page);
    return form;
}

void SettingsPage::buildCurrencySection()
{
    QFormLayout *form = addFormSection(tr("Currency"), "currency");

    currencyCombo = new QComboBox(this);
    currencyCombo->setObjectName("currencyCombo");
    currencyCombo->addItem(tr("US dollar ($)"), static_cast<int>(Settings::CurrencySymbol::USD));
    currencyCombo->addItem(tr("Euro (€)"), static_cast<int>(Settings::CurrencySymbol::EUR));
    currencyCombo->addItem(tr("Pound sterling (£)"), static_cast<int>(Settings::CurrencySymbol::GBP));
    currencyCombo->addItem(tr("Yen (¥)"), static_cast<int>(Settings::CurrencySymbol::JPY));
    currencyCombo->addItem(tr("Swiss franc (CHF)"), static_cast<int>(Settings::CurrencySymbol::CHF));
    currencyCombo->addItem(tr("Custom"), static_cast<int>(Settings::CurrencySymbol::Custom));
    form->addRow(tr("Currency"), currencyCombo);
    watch(currencyCombo);

    customSymbolEdit = new QLineEdit(this);
    customSymbolEdit->setObjectName("customSymbolEdit");
    customSymbolEdit->setMaxLength(5);
    form->addRow(tr("Custom symbol"), customSymbolEdit);
    watch(customSymbolEdit);
    connect(currencyCombo, &QComboBox::currentIndexChanged, this, [this]() {
        customSymbolEdit->setEnabled(currencyCombo->currentData().toInt() == static_cast<int>(Settings::CurrencySymbol::Custom));
    });

    symbolPositionCombo = new QComboBox(this);
    symbolPositionCombo->setObjectName("symbolPositionCombo");
    symbolPositionCombo->addItem(tr("Before the amount ($100)"), true);
    symbolPositionCombo->addItem(tr("After the amount (100 $)"), false);
    form->addRow(tr("Symbol position"), symbolPositionCombo);
    watch(symbolPositionCombo);

    decimalSeparatorCombo = new QComboBox(this);
    decimalSeparatorCombo->setObjectName("decimalSeparatorCombo");
    decimalSeparatorCombo->addItem(tr("Point (1234.56)"), static_cast<int>(Settings::DecimalSeparator::Point));
    decimalSeparatorCombo->addItem(tr("Comma (1234,56)"), static_cast<int>(Settings::DecimalSeparator::Comma));
    form->addRow(tr("Decimal separator"), decimalSeparatorCombo);
    watch(decimalSeparatorCombo);

    thousandsSeparatorCombo = new QComboBox(this);
    thousandsSeparatorCombo->setObjectName("thousandsSeparatorCombo");
    thousandsSeparatorCombo->addItem(tr("None (1234)"), static_cast<int>(Settings::ThousandsSeparator::None));
    thousandsSeparatorCombo->addItem(tr("Comma (1,234)"), static_cast<int>(Settings::ThousandsSeparator::Comma));
    thousandsSeparatorCombo->addItem(tr("Point (1.234)"), static_cast<int>(Settings::ThousandsSeparator::Point));
    thousandsSeparatorCombo->addItem(tr("Space (1 234)"), static_cast<int>(Settings::ThousandsSeparator::Space));
    form->addRow(tr("Thousands separator"), thousandsSeparatorCombo);
    watch(thousandsSeparatorCombo);

    auto *note = new QLabel(tr("Saved in your database, it looks the same on every computer you open it."), this);
    note->setObjectName("currencyNote");
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(note);
}

void SettingsPage::buildTransactionsSection()
{
    QFormLayout *form = addFormSection(tr("Transactions"), "transactions");

    fastEntryCheck = new QCheckBox(tr("Fast entry: after saving a new transaction, start the next one"), this);
    fastEntryCheck->setObjectName("fastEntryCheck");
    fastEntryCheck->setToolTip(tr("Keeps the editor open with the same date and account, so you can type many transactions in a row"));
    form->addRow(fastEntryCheck);
    watch(fastEntryCheck);

    selectionCheckboxesCheck = new QCheckBox(tr("Show checkboxes for selecting several transactions"), this);
    selectionCheckboxesCheck->setObjectName("selectionCheckboxesCheck");
    selectionCheckboxesCheck->setToolTip(tr("Without checkboxes, select several transactions with Ctrl+click or Shift+click"));
    form->addRow(selectionCheckboxesCheck);
    watch(selectionCheckboxesCheck);

    suggestCategoriesCheck = new QCheckBox(tr("Suggest categories and payee names from my rules when adding a transaction"), this);
    suggestCategoriesCheck->setObjectName("suggestCategoriesCheck");
    suggestCategoriesCheck->setToolTip(tr("Imports have their own choice; see the Review step of the importer"));
    form->addRow(suggestCategoriesCheck);
    watch(suggestCategoriesCheck);
}

void SettingsPage::buildAppearanceSection()
{
    QFormLayout *form = addFormSection(tr("Appearance"), "appearance");

    themeCombo = new QComboBox(this);
    themeCombo->setObjectName("themeCombo");
    themeCombo->addItem(tr("Dark"), "Dark");
    themeCombo->addItem(tr("Light"), "Light");
    themeCombo->addItem(tr("Match system"), "System");
    form->addRow(tr("Theme"), themeCombo);
    watch(themeCombo);

    // Each language in its own words. "Match system" names the language it comes to.
    languageCombo = new QComboBox(this);
    languageCombo->setObjectName("languageCombo");
    const QString systemCode = Language::resolve(Language::System);
    for (const Language::Option& option : Language::available()) {
        if (option.code == systemCode) {
            languageCombo->addItem(tr("Match system (%1)").arg(option.nativeName), Language::System);
        }
    }
    for (const Language::Option& option : Language::available()) {
        languageCombo->addItem(option.nativeName, option.code);
    }
    form->addRow(tr("Language"), languageCombo);
    watch(languageCombo);

    // Shown while the saved language isn't the one Aion started with
    restartNote = new QWidget(this);
    restartNote->setObjectName("restartNote");
    auto *restartLayout = new QHBoxLayout(restartNote);
    restartLayout->setContentsMargins(0, 0, 0, 0);
    auto *restartLabel = new QLabel(tr("Takes effect when Aion restarts."), restartNote);
    restartLabel->setForegroundRole(QPalette::PlaceholderText);
    auto *restartButton = new QToolButton(restartNote);
    restartButton->setObjectName("restartButton");
    restartButton->setText(tr("Restart now"));
    Theme::makeLink(restartButton);
    connect(restartButton, &QToolButton::clicked, this, &SettingsPage::restartRequested);
    restartLayout->addWidget(restartLabel);
    restartLayout->addWidget(restartButton);
    restartLayout->addStretch();
    form->addRow(restartNote);

    categoryColorsCheck = new QCheckBox(tr("Use category colours on the Dashboard, the Budget page and in the sidebar"), this);
    categoryColorsCheck->setObjectName("categoryColorsCheck");
    categoryColorsCheck->setToolTip(tr("Coloured spending bars and dots before budget names. "
                                       "Without it they stay neutral; category tags are always coloured."));
    form->addRow(categoryColorsCheck);
    watch(categoryColorsCheck);

    expensesRedCheck = new QCheckBox(tr("Show expenses in red"), this);
    expensesRedCheck->setObjectName("expensesRedCheck");
    expensesRedCheck->setToolTip(tr("In the transaction lists, the Expenses totals and the income and spending charts. "
                                     "Transfers and balance corrections stay as they are."));
    form->addRow(expensesRedCheck);
    watch(expensesRedCheck);

    reportsCentredCheck = new QCheckBox(tr("Show reports in a centred column instead of the full width"), this);
    reportsCentredCheck->setObjectName("reportsCentredCheck");
    reportsCentredCheck->setToolTip(tr("At most %1 pixels wide, so elements stay close on a large screen")
                                        .arg(ReportsPage::CentredWidth));
    form->addRow(reportsCentredCheck);
    watch(reportsCentredCheck);

    allBudgetsCheck = new QCheckBox(tr("Show all budgets in the sidebar"), this);
    allBudgetsCheck->setObjectName("allBudgetsCheck");
    allBudgetsCheck->setToolTip(tr("Otherwise the sidebar shows as many as fit without scrolling, in the order below, and links to the Budget page for the rest"));
    form->addRow(allBudgetsCheck);
    watch(allBudgetsCheck);

    budgetSortCombo = new QComboBox(this);
    budgetSortCombo->setObjectName("budgetSortCombo");
    budgetSortCombo->addItem(tr("Percentage used"), int(Settings::BudgetSort::Percentage));
    budgetSortCombo->addItem(tr("Amount spent"), int(Settings::BudgetSort::Spent));
    budgetSortCombo->addItem(tr("Budgeted amount"), int(Settings::BudgetSort::Budgeted));
    budgetSortCombo->addItem(tr("Name"), int(Settings::BudgetSort::Name));
    budgetSortCombo->setToolTip(tr("Also in the right-click menu of the sidebar's Budget header"));
    form->addRow(tr("Sort sidebar budgets by"), budgetSortCombo);
    watch(budgetSortCombo);
}

void SettingsPage::reloadSidebarBudgetSort()
{
    loading = true;
    budgetSortCombo->setCurrentIndex(budgetSortCombo->findData(int(Settings::instance().getSidebarBudgetSort())));
    loading = false;
}

void SettingsPage::reloadReportsWidth()
{
    loading = true;
    reportsCentredCheck->setChecked(Settings::instance().isReportsCentred());
    loading = false;
}

void SettingsPage::buildImportSection()
{
    QFormLayout *form = addFormSection(tr("Import"), "import");

    autoDetectDelimiterCheck = new QCheckBox(tr("Detect the CSV delimiter automatically"), this);
    autoDetectDelimiterCheck->setObjectName("autoDetectDelimiterCheck");
    form->addRow(autoDetectDelimiterCheck);
    watch(autoDetectDelimiterCheck);

    defaultDelimiterCombo = new QComboBox(this);
    defaultDelimiterCombo->setObjectName("defaultDelimiterCombo");
    defaultDelimiterCombo->addItem(tr("Comma ( , )"), QString(","));
    defaultDelimiterCombo->addItem(tr("Semicolon ( ; )"), QString(";"));
    defaultDelimiterCombo->addItem(tr("Tab"), QString("\t"));
    defaultDelimiterCombo->addItem(tr("Pipe ( | )"), QString("|"));
    form->addRow(tr("Default delimiter"), defaultDelimiterCombo);
    watch(defaultDelimiterCombo);

    autoDetectDecimalCheck = new QCheckBox(tr("Detect the decimal separator automatically"), this);
    autoDetectDecimalCheck->setObjectName("autoDetectDecimalCheck");
    form->addRow(autoDetectDecimalCheck);
    watch(autoDetectDecimalCheck);
}

void SettingsPage::setUserId(int userId)
{
    categoriesSettings->setUserId(userId);
    rulesSettings->setUserId(userId);
    accountTypesSettings->setUserId(userId);
}

void SettingsPage::openRuleDraft(const Rule &draft, const QList<Transaction> &examples)
{
    const QList<QListWidgetItem *> items = sectionList->findItems(tr("Rules"), Qt::MatchExactly);
    if (!items.isEmpty()) {
        sectionList->setCurrentItem(items.first());
    }
    rulesSettings->openDraft(draft, examples);
}

void SettingsPage::reloadData()
{
    categoriesSettings->reload();
    rulesSettings->reload();
    accountTypesSettings->reload();
}

void SettingsPage::buildAboutSection()
{
    addSection(tr("About"), "info", new AboutWidget(this));
}

void SettingsPage::buildCategoriesSection()
{
    // Saved right away, so they don't take part in Apply / Revert
    categoriesSettings = new CategoriesSettings(this);
    connect(categoriesSettings, &CategoriesSettings::categoriesChanged, this, &SettingsPage::dataChanged);
    addSection(tr("Categories"), "tag", categoriesSettings);

    // Rules point to categories, so they reload when categories change (a merge moves rules along)
    rulesSettings = new RulesSettings(this);
    connect(rulesSettings, &RulesSettings::rulesChanged, this, &SettingsPage::dataChanged);
    connect(rulesSettings, &RulesSettings::transactionsChanged, this, &SettingsPage::dataChanged);
    connect(categoriesSettings, &CategoriesSettings::categoriesChanged, rulesSettings, &RulesSettings::reload);
    addSection(tr("Rules"), "search", rulesSettings);

    accountTypesSettings = new AccountTypesSettings(this);
    connect(accountTypesSettings, &AccountTypesSettings::accountTypesChanged, this, &SettingsPage::dataChanged);
    addSection(tr("Account types"), "bank", accountTypesSettings);

    auto *backupSettings = new BackupSettings(this);
    connect(backupSettings, &BackupSettings::restored, this, &SettingsPage::databaseRestored);
    addSection(tr("Backup"), "database", backupSettings);
}

void SettingsPage::watch(QComboBox *combo)
{
    connect(combo, &QComboBox::currentIndexChanged, this, &SettingsPage::markFormChanged);
}

void SettingsPage::watch(QCheckBox *checkBox)
{
    connect(checkBox, &QCheckBox::toggled, this, &SettingsPage::markFormChanged);
}

void SettingsPage::watch(QLineEdit *lineEdit)
{
    connect(lineEdit, &QLineEdit::textEdited, this, &SettingsPage::markFormChanged);
}

void SettingsPage::markFormChanged()
{
    if (loading) {
        return;
    }
    formChanged = true;
    updateChangeState();
}

void SettingsPage::updateChangeState()
{
    const bool hasChanges = formChanged;
    unsavedLabel->setVisible(hasChanges);
    revertButton->setVisible(hasChanges);
    applyButton->setVisible(hasChanges);
}

void SettingsPage::loadSettings()
{
    loading = true;
    const Settings &settings = Settings::instance();

    selectByData(currencyCombo, static_cast<int>(settings.getCurrencySymbolType()));
    customSymbolEdit->setText(settings.getCustomCurrencySymbol());
    customSymbolEdit->setEnabled(settings.getCurrencySymbolType() == Settings::CurrencySymbol::Custom);
    selectByData(symbolPositionCombo, settings.isCurrencySymbolPrefix());
    selectByData(decimalSeparatorCombo, static_cast<int>(settings.getDecimalSeparator()));
    selectByData(thousandsSeparatorCombo, static_cast<int>(settings.getThousandsSeparator()));

    fastEntryCheck->setChecked(settings.isFastEntryEnabled());
    selectionCheckboxesCheck->setChecked(settings.isSelectionCheckboxesEnabled());
    suggestCategoriesCheck->setChecked(settings.isCategorySuggestionEnabled());
    categoryColorsCheck->setChecked(settings.isCategoryColorsInOverviewsEnabled());
    expensesRedCheck->setChecked(settings.isExpensesInRed());
    reportsCentredCheck->setChecked(settings.isReportsCentred());
    allBudgetsCheck->setChecked(settings.isSidebarShowingAllBudgets());
    budgetSortCombo->setCurrentIndex(budgetSortCombo->findData(int(settings.getSidebarBudgetSort())));

    // Stored values are case-insensitive, matching Theme::fromName()
    const QString theme = settings.getTheme().toLower();
    for (int i = 0; i < themeCombo->count(); ++i) {
        if (themeCombo->itemData(i).toString().toLower() == theme) {
            themeCombo->setCurrentIndex(i);
        }
    }
    selectByData(languageCombo, settings.getLanguage());
    if (languageCombo->currentIndex() < 0) {
        selectByData(languageCombo, Language::System);
    }
    updateRestartNote();

    autoDetectDelimiterCheck->setChecked(settings.isAutoDetectCsvDelimiter());
    selectByData(defaultDelimiterCombo, QString(settings.getDefaultCsvDelimiter()));
    autoDetectDecimalCheck->setChecked(settings.isAutoDetectDecimalSeparator());

    loading = false;
    formChanged = false;
    updateChangeState();
}

void SettingsPage::saveSettings()
{
    Settings &settings = Settings::instance();

    const auto currency = static_cast<Settings::CurrencySymbol>(currencyCombo->currentData().toInt());
    const QString customSymbol = customSymbolEdit->text().trimmed();
    if (currency == Settings::CurrencySymbol::Custom && !customSymbol.isEmpty()) {
        settings.setCustomCurrencySymbol(customSymbol);
    } else if (currency != Settings::CurrencySymbol::Custom) {
        settings.setCurrencySymbol(currency);
    }
    settings.setCurrencySymbolPrefix(symbolPositionCombo->currentData().toBool());
    settings.setDecimalSeparator(static_cast<Settings::DecimalSeparator>(decimalSeparatorCombo->currentData().toInt()));
    settings.setThousandsSeparator(static_cast<Settings::ThousandsSeparator>(thousandsSeparatorCombo->currentData().toInt()));

    settings.setFastEntryEnabled(fastEntryCheck->isChecked());
    settings.setSelectionCheckboxesEnabled(selectionCheckboxesCheck->isChecked());
    settings.setCategorySuggestionEnabled(suggestCategoriesCheck->isChecked());

    settings.setTheme(themeCombo->currentData().toString());
    settings.setLanguage(languageCombo->currentData().toString());
    settings.setCategoryColorsInOverviewsEnabled(categoryColorsCheck->isChecked());
    settings.setExpensesInRed(expensesRedCheck->isChecked());
    settings.setReportsCentred(reportsCentredCheck->isChecked());
    settings.setSidebarShowingAllBudgets(allBudgetsCheck->isChecked());
    settings.setSidebarBudgetSort(static_cast<Settings::BudgetSort>(budgetSortCombo->currentData().toInt()));

    settings.setAutoDetectCsvDelimiter(autoDetectDelimiterCheck->isChecked());
    settings.setDefaultCsvDelimiter(defaultDelimiterCombo->currentData().toString().at(0));
    settings.setAutoDetectDecimalSeparator(autoDetectDecimalCheck->isChecked());

    settings.getQSettings().sync();
}

void SettingsPage::apply()
{
    if (formChanged) {
        saveSettings();
        formChanged = false;
    }
    updateChangeState();
    updateRestartNote();
    emit settingsChanged();
}

void SettingsPage::updateRestartNote()
{
    restartNote->setHidden(Language::resolve(Settings::instance().getLanguage()) == Language::current());
}

void SettingsPage::revert()
{
    loadSettings();
}

void SettingsPage::restoreDefaults()
{
    const auto answer = QMessageBox::question(
        this, tr("Restore default settings?"),
        tr("This resets currency, transaction, appearance and import settings to their defaults. "
           "Your accounts, transactions, categories and account types are not affected."),
        QMessageBox::RestoreDefaults | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer != QMessageBox::RestoreDefaults) {
        return;
    }

    Settings::instance().restoreDefaults();
    loadSettings();
    emit settingsChanged();
}

void SettingsPage::updateIcons()
{
    const QColor color = palette().color(QPalette::ButtonText);
    restoreDefaultsButton->setIcon(Icons::icon("restore", color));
    advancedButton->setIcon(Icons::icon("code", color));
}

void SettingsPage::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange && restoreDefaultsButton) {
        updateIcons();
    }
    QWidget::changeEvent(event);
}

void SettingsPage::openAdvancedSettings()
{
    RawSettingsDialog dialog(this);
    connect(&dialog, &RawSettingsDialog::settingsChanged, this, [this]() {
        loadSettings();
        emit settingsChanged();
    });
    dialog.exec();
}
