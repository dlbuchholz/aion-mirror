#include "settingspage.h"
#include "dialog/rawsettingsdialog.h"
#include "aboutwidget.h"
#include "accounttypessettings.h"
#include "categoriessettings.h"
#include "rulessettings.h"
#include "icons.h"
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

    auto *title = new QLabel("Settings", this);
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
    restoreDefaultsButton->setText("Restore defaults");
    restoreDefaultsButton->setToolTip("Reset currency, transaction, appearance and import settings");
    connect(restoreDefaultsButton, &QToolButton::clicked, this, &SettingsPage::restoreDefaults);
    advancedButton = new QToolButton(this);
    advancedButton->setText("Advanced settings");
    advancedButton->setToolTip("Edit every stored setting as raw text");
    connect(advancedButton, &QToolButton::clicked, this, &SettingsPage::openAdvancedSettings);
    for (QToolButton *button : {restoreDefaultsButton, advancedButton}) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        buttonBar->addWidget(button);
    }
    updateIcons();
    buttonBar->addStretch();

    unsavedLabel = new QLabel("Unsaved changes", this);
    unsavedLabel->setObjectName("unsavedLabel");
    buttonBar->addWidget(unsavedLabel);
    revertButton = new QPushButton("Revert", this);
    revertButton->setObjectName("revertButton");
    connect(revertButton, &QPushButton::clicked, this, &SettingsPage::revert);
    buttonBar->addWidget(revertButton);
    applyButton = new QPushButton("Apply", this);
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
    QFormLayout *form = addFormSection("Currency", "currency");

    currencyCombo = new QComboBox(this);
    currencyCombo->setObjectName("currencyCombo");
    currencyCombo->addItem("US dollar ($)", static_cast<int>(Settings::CurrencySymbol::USD));
    currencyCombo->addItem("Euro (€)", static_cast<int>(Settings::CurrencySymbol::EUR));
    currencyCombo->addItem("Pound sterling (£)", static_cast<int>(Settings::CurrencySymbol::GBP));
    currencyCombo->addItem("Yen (¥)", static_cast<int>(Settings::CurrencySymbol::JPY));
    currencyCombo->addItem("Swiss franc (CHF)", static_cast<int>(Settings::CurrencySymbol::CHF));
    currencyCombo->addItem("Custom", static_cast<int>(Settings::CurrencySymbol::Custom));
    form->addRow("Currency", currencyCombo);
    watch(currencyCombo);

    customSymbolEdit = new QLineEdit(this);
    customSymbolEdit->setObjectName("customSymbolEdit");
    customSymbolEdit->setMaxLength(5);
    form->addRow("Custom symbol", customSymbolEdit);
    watch(customSymbolEdit);
    connect(currencyCombo, &QComboBox::currentIndexChanged, this, [this]() {
        customSymbolEdit->setEnabled(currencyCombo->currentData().toInt() == static_cast<int>(Settings::CurrencySymbol::Custom));
    });

    symbolPositionCombo = new QComboBox(this);
    symbolPositionCombo->setObjectName("symbolPositionCombo");
    symbolPositionCombo->addItem("Before the amount ($100)", true);
    symbolPositionCombo->addItem("After the amount (100 $)", false);
    form->addRow("Symbol position", symbolPositionCombo);
    watch(symbolPositionCombo);

    decimalSeparatorCombo = new QComboBox(this);
    decimalSeparatorCombo->setObjectName("decimalSeparatorCombo");
    decimalSeparatorCombo->addItem("Point (1234.56)", static_cast<int>(Settings::DecimalSeparator::Point));
    decimalSeparatorCombo->addItem("Comma (1234,56)", static_cast<int>(Settings::DecimalSeparator::Comma));
    form->addRow("Decimal separator", decimalSeparatorCombo);
    watch(decimalSeparatorCombo);

    thousandsSeparatorCombo = new QComboBox(this);
    thousandsSeparatorCombo->setObjectName("thousandsSeparatorCombo");
    thousandsSeparatorCombo->addItem("None (1234)", static_cast<int>(Settings::ThousandsSeparator::None));
    thousandsSeparatorCombo->addItem("Comma (1,234)", static_cast<int>(Settings::ThousandsSeparator::Comma));
    thousandsSeparatorCombo->addItem("Point (1.234)", static_cast<int>(Settings::ThousandsSeparator::Point));
    thousandsSeparatorCombo->addItem("Space (1 234)", static_cast<int>(Settings::ThousandsSeparator::Space));
    form->addRow("Thousands separator", thousandsSeparatorCombo);
    watch(thousandsSeparatorCombo);

    auto *note = new QLabel("Saved in this database, so it looks the same on every computer that opens it.", this);
    note->setObjectName("currencyNote");
    note->setWordWrap(true);
    note->setForegroundRole(QPalette::PlaceholderText);
    form->addRow(note);
}

void SettingsPage::buildTransactionsSection()
{
    QFormLayout *form = addFormSection("Transactions", "transactions");

    fastEntryCheck = new QCheckBox("Fast entry: after saving a new transaction, start the next one", this);
    fastEntryCheck->setObjectName("fastEntryCheck");
    fastEntryCheck->setToolTip("Keeps the editor open with the same date and account, so you can type many transactions in a row");
    form->addRow(fastEntryCheck);
    watch(fastEntryCheck);

    selectionCheckboxesCheck = new QCheckBox("Show checkboxes for selecting several transactions", this);
    selectionCheckboxesCheck->setObjectName("selectionCheckboxesCheck");
    selectionCheckboxesCheck->setToolTip("Without checkboxes, select several transactions with Ctrl+click or Shift+click");
    form->addRow(selectionCheckboxesCheck);
    watch(selectionCheckboxesCheck);

    suggestCategoriesCheck = new QCheckBox("Suggest categories and payee names from my rules when adding a transaction", this);
    suggestCategoriesCheck->setObjectName("suggestCategoriesCheck");
    suggestCategoriesCheck->setToolTip("Imports have their own choice; see the Review step of the importer");
    form->addRow(suggestCategoriesCheck);
    watch(suggestCategoriesCheck);
}

void SettingsPage::buildAppearanceSection()
{
    QFormLayout *form = addFormSection("Appearance", "appearance");

    themeCombo = new QComboBox(this);
    themeCombo->setObjectName("themeCombo");
    themeCombo->addItem("Dark", "Dark");
    themeCombo->addItem("Light", "Light");
    themeCombo->addItem("Match system", "System");
    form->addRow("Theme", themeCombo);
    watch(themeCombo);

    categoryColorsCheck = new QCheckBox("Use category colours on the Dashboard and Budget page", this);
    categoryColorsCheck->setObjectName("categoryColorsCheck");
    categoryColorsCheck->setToolTip("Coloured spending bars and dots before budget names. "
                                    "Without it they stay neutral; category tags are always coloured.");
    form->addRow(categoryColorsCheck);
    watch(categoryColorsCheck);
}

void SettingsPage::buildImportSection()
{
    QFormLayout *form = addFormSection("Import", "import");

    autoDetectDelimiterCheck = new QCheckBox("Detect the CSV delimiter automatically", this);
    autoDetectDelimiterCheck->setObjectName("autoDetectDelimiterCheck");
    form->addRow(autoDetectDelimiterCheck);
    watch(autoDetectDelimiterCheck);

    defaultDelimiterCombo = new QComboBox(this);
    defaultDelimiterCombo->setObjectName("defaultDelimiterCombo");
    defaultDelimiterCombo->addItem("Comma ( , )", QString(","));
    defaultDelimiterCombo->addItem("Semicolon ( ; )", QString(";"));
    defaultDelimiterCombo->addItem("Tab", QString("\t"));
    defaultDelimiterCombo->addItem("Pipe ( | )", QString("|"));
    form->addRow("Default delimiter", defaultDelimiterCombo);
    watch(defaultDelimiterCombo);

    autoDetectDecimalCheck = new QCheckBox("Detect the decimal separator automatically", this);
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

void SettingsPage::createRule(const Rule &draft)
{
    const QList<QListWidgetItem *> items = sectionList->findItems("Rules", Qt::MatchExactly);
    if (!items.isEmpty()) {
        sectionList->setCurrentItem(items.first());
    }
    rulesSettings->createRule(draft);
}

void SettingsPage::reloadData()
{
    categoriesSettings->reload();
    rulesSettings->reload();
    accountTypesSettings->reload();
}

void SettingsPage::buildAboutSection()
{
    addSection("About", "info", new AboutWidget(this));
}

void SettingsPage::buildCategoriesSection()
{
    // Saved right away, so they don't take part in Apply / Revert
    categoriesSettings = new CategoriesSettings(this);
    connect(categoriesSettings, &CategoriesSettings::categoriesChanged, this, &SettingsPage::dataChanged);
    addSection("Categories", "tag", categoriesSettings);

    // Rules point to categories, so they reload when categories change (a merge moves rules along)
    rulesSettings = new RulesSettings(this);
    connect(rulesSettings, &RulesSettings::rulesChanged, this, &SettingsPage::dataChanged);
    connect(rulesSettings, &RulesSettings::transactionsChanged, this, &SettingsPage::dataChanged);
    connect(categoriesSettings, &CategoriesSettings::categoriesChanged, rulesSettings, &RulesSettings::reload);
    addSection("Rules", "search", rulesSettings);

    accountTypesSettings = new AccountTypesSettings(this);
    connect(accountTypesSettings, &AccountTypesSettings::accountTypesChanged, this, &SettingsPage::dataChanged);
    addSection("Account types", "database", accountTypesSettings);
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

    // Stored values are case-insensitive, matching Theme::fromName()
    const QString theme = settings.getTheme().toLower();
    for (int i = 0; i < themeCombo->count(); ++i) {
        if (themeCombo->itemData(i).toString().toLower() == theme) {
            themeCombo->setCurrentIndex(i);
        }
    }

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
    settings.setCategoryColorsInOverviewsEnabled(categoryColorsCheck->isChecked());

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
    emit settingsChanged();
}

void SettingsPage::revert()
{
    loadSettings();
}

void SettingsPage::restoreDefaults()
{
    const auto answer = QMessageBox::question(
        this, "Restore default settings?",
        "This resets currency, transaction, appearance and import settings to their defaults. "
        "Your accounts, transactions, categories and account types are not affected.",
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
