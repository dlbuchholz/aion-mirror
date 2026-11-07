#include <QtTest>
#include <QPainterPath>
#include <QPainter>
#include <QListView>
#include <QDir>
#include <QAbstractButton>
#include <QCheckBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QListWidget>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QAbstractSpinBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QSqlQuery>
#include <QStackedWidget>

#include "account.h"
#include "budget.h"
#include "db/databasemanager.h"
#include "user.h"
#include "transaction.h"
#include "category.h"
#include "dialog/applyrulesdialog.h"
#include "dialog/balancecorrectiondialog.h"
#include "dialog/deleteaccountdialog.h"
#include "dialog/editaccountdialog.h"
#include "dialog/importmanagerdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "startwindow.h"
#include "tests/test_db_fixture.h"
#include "theme.h"
#include "widgets/budgetpage.h"
#include "widgets/emptystate.h"
#include "widgets/periodpicker.h"
#include "widgets/reportspage.h"
#include "widgets/transactionspage.h"
#include "widgets/sidebarwidget.h"

// Draws every page and dialog in both themes. Painting code only runs when something is
// actually drawn, so this catches crashes and asserts in it; the light and dark pictures
// must differ, which shows the theme reached the widgets.
class TestRendering : public QObject {
    Q_OBJECT
    TestDbFixture db;
    QMap<QString, QVariant> originalSettings;

    static QImage draw(QWidget& widget, QSize size = QSize(1100, 700)) {
        widget.setAttribute(Qt::WA_DontShowOnScreen);
        widget.resize(size);
        widget.show();
        QCoreApplication::processEvents();
        const QImage image = widget.grab().toImage();
        widget.hide();
        return image;
    }

    static QColor background(const QImage& image) {
        return image.pixelColor(image.width() - 5, image.height() - 5);
    }

    void seed() {
        QSqlQuery query(DatabaseManager::instance().database());
        QVERIFY(UserDAO().createUser(User(1, "alex", "Alex Example")));
        const int giro = AccountDAO::createAccount(1, "Girokonto", "Checking", 100000);
        QVERIFY(query.exec("UPDATE categories SET color = 'teal' WHERE name = 'Groceries'"));
        QVERIFY(query.exec("SELECT id FROM categories WHERE name = 'Groceries'") && query.next());
        const int groceries = query.value(0).toInt();
        const QDate today = QDate::currentDate();
        TransactionDAO transactions;
        for (int month = 0; month < 6; ++month) {
            transactions.addTransaction(Transaction(-1, giro, "ACME", 250000, today.addMonths(-month), "Salary", 0, "income"));
            transactions.addTransaction(Transaction(-1, giro, "Rewe", 31200 + month * 1000, today.addMonths(-month), "", groceries, "expense"));
        }
        BudgetDAO budgets;
        QVERIFY(budgets.setMonthlyLimit(1, groceries, today, 20000));            // Over budget: red bars too
    }

private slots:
    void initTestCase() {
        db.init();
        QSettings& settings = Settings::instance().getQSettings();
        for (const QString& key : settings.allKeys()) originalSettings.insert(key, settings.value(key));
        Settings::instance().setSelectionCheckboxesEnabled(true);              // Draw the checkboxes too
    }
    void cleanupTestCase() {
        Theme::applyTheme(Theme::Type::Dark);
        QSettings& settings = Settings::instance().getQSettings();
        settings.clear();
        for (auto it = originalSettings.cbegin(); it != originalSettings.cend(); ++it) settings.setValue(it.key(), it.value());
    }
    void init() {
        db.yankDb();
        seed();
    }
    void cleanup() { db.close(); }

    void everyPageDrawsInBothThemes() {
        QMap<QString, QColor> backgrounds;
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            QCOMPARE(Theme::isDark(), theme == Theme::Type::Dark);
            MainWindow window(1);
            auto* sidebar = window.findChild<SidebarWidget*>();
            for (const auto page : {SidebarWidget::Page::Dashboard, SidebarWidget::Page::Transactions,
                                    SidebarWidget::Page::Budget, SidebarWidget::Page::Reports, SidebarWidget::Page::Settings}) {
                emit sidebar->pageRequested(page);
                const QImage image = draw(window);
                QVERIFY(!image.isNull());
                if (page == SidebarWidget::Page::Dashboard) {
                    backgrounds.insert(theme == Theme::Type::Dark ? "dark" : "light", background(image));
                }
            }
            // Every settings section, including the category list with its editor open
            emit sidebar->pageRequested(SidebarWidget::Page::Settings);
            auto* sections = window.findChild<QListWidget*>();
            for (int row = 0; row < sections->count(); ++row) {
                sections->setCurrentRow(row);
                QVERIFY(!draw(window).isNull());
            }
            for (QAbstractButton* row : window.findChildren<QAbstractButton*>("categoryRow")) {
                if (row->accessibleName() == "Groceries") row->click();
            }
            QVERIFY(!draw(window).isNull());
            sidebar->setCollapsed(true, false);                                  // The narrow sidebar
            QVERIFY(!draw(window).isNull());
            sidebar->setCollapsed(false, false);
        }
        QVERIFY(backgrounds["light"].lightness() > backgrounds["dark"].lightness());
    }

    // The README's screenshots: demo data, the best pages in both themes, and a collage. Skipped
    // unless asked for; to take them again, run on the real window system (no QT_QPA_PLATFORM) with
    //   AION_SCREENSHOTS=docs/screenshots  test_rendering readmeScreenshots
    void readmeScreenshots() {
        if (qEnvironmentVariableIsEmpty("AION_SCREENSHOTS")) QSKIP("Screenshots only");
        const QString out = qEnvironmentVariable("AION_SCREENSHOTS");
        const int user = 3;
        QVERIFY(UserDAO().createUser(User(user, "kevin", "Kevin Gilbert")));
        Settings::instance().setExpensesInRed(true);      // Restored with the other settings afterwards
        QSqlQuery query(DatabaseManager::instance().database());
        CategoryDAO categories(user);
        const auto categoryId = [&categories](const QString& name) {
            for (const Category& category : categories.getAllCategories()) {
                if (category.getName() == name) return category.getCategoryId();
            }
            return 0;
        };
        QVERIFY(categories.addCategory(Category(-1, "Eating out", "expense", "orange")) > 0);
        QVERIFY(categories.addCategory(Category(-1, "Transport", "expense", "sky")) > 0);
        QVERIFY(categories.addCategory(Category(-1, "Shopping", "expense", "pink")) > 0);
        for (const auto& [name, color] : QList<QPair<QString, QString>>{{"Groceries", "teal"}, {"Rent", "indigo"},
                 {"Utilities", "amber"}, {"Entertainment", "purple"}, {"Salary", "green"}, {"Bonus", "lime"}}) {
            QVERIFY(query.exec(QString("UPDATE categories SET color = '%1' WHERE name = '%2' AND user_id = %3").arg(color, name).arg(user)));
        }
        const int checking = AccountDAO::createAccount(user, "Checking", "Checking", 0);
        const int savings = AccountDAO::createAccount(user, "Savings", "Savings", 0);
        const int card = AccountDAO::createAccount(user, "Credit card", "Checking", 0);
        TransactionDAO transactions;
        const QDate today = QDate::currentDate();
        const auto add = [&transactions](int account, const char* payee, Cents cents, const QDate& day, int category,
                                         const char* type, const char* description = "") {
            if (day > QDate::currentDate()) return;
            transactions.addTransaction(Transaction(-1, account, payee, cents, day, description, category, type));
        };
        // Six months, a little different each month
        for (int back = 5; back >= 0; --back) {
            const QDate month = QDate(today.year(), today.month(), 1).addMonths(-back);
            const int wobble = (back * 37) % 11;
            add(checking, "Acme Studio", 385000, month.addDays(0), categoryId("Salary"), "income", "Salary");
            add(checking, "Oak Street Apartments", 129500, month.addDays(1), categoryId("Rent"), "expense", "Rent");
            add(checking, "City Power & Water", 8900 + wobble * 310, month.addDays(4), categoryId("Utilities"), "expense");
            add(card, "StreamBox", 1399, month.addDays(6), categoryId("Entertainment"), "expense", "Subscription");
            add(card, "Metro Transit", 4900, month.addDays(2), categoryId("Transport"), "expense", "Monthly pass");
            for (int week = 0; week < 4; ++week) {
                add(checking, week % 2 ? "FreshMart" : "Green Grocer", 6240 + ((week + back) * 913) % 3400,
                    month.addDays(3 + week * 7), categoryId("Groceries"), "expense");
            }
            add(card, "Corner Café", 1850 + wobble * 120, month.addDays(9), categoryId("Eating out"), "expense");
            add(card, "Noodle Bar", 3260 + wobble * 90, month.addDays(17), categoryId("Eating out"), "expense", "Dinner with friends");
            add(card, "Bookshop", 2499 + wobble * 200, month.addDays(13), categoryId("Shopping"), "expense");
            add(card, "Cinema Paradiso", 2400, month.addDays(20), categoryId("Entertainment"), "expense");
            add(checking, "Savings", 50000, month.addDays(2), categoryId("Transfer"), "expense", "Monthly saving");
            add(savings, "Checking", 50000, month.addDays(2), categoryId("Transfer"), "income", "Monthly saving");
        }
        // The one the editor shows: last month's dinner
        int dinner = -1;
        {
            const QDate lastMonth = QDate(today.year(), today.month(), 1).addMonths(-1);
            dinner = transactions.addTransaction(Transaction(-1, card, "Noodle Bar", 4180, lastMonth.addDays(24), "Birthday dinner",
                                                             categoryId("Eating out"), "expense"));
            QVERIFY(dinner > 0);
        }
        add(checking, "Acme Studio", 60000, QDate(today.year(), today.month(), 1).addMonths(-2).addDays(14), categoryId("Bonus"), "income", "Project bonus");
        BudgetDAO budgets;
        const QDate firstMonth = QDate(today.year(), today.month(), 1).addMonths(-5);
        QVERIFY(budgets.setMonthlyLimit(user, categoryId("Groceries"), firstMonth, 32000));
        QVERIFY(budgets.setMonthlyLimit(user, categoryId("Eating out"), firstMonth, 6000));
        QVERIFY(budgets.setMonthlyLimit(user, categoryId("Entertainment"), firstMonth, 5000));
        QVERIFY(budgets.setMonthlyLimit(user, categoryId("Shopping"), firstMonth, 2000));    // Over this month
        QVERIFY(budgets.setMonthlyLimit(user, categoryId("Utilities"), firstMonth, 15000));

        const QSize window(1360, 860);
        QMap<QString, QImage> shots;
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            const QString t = theme == Theme::Type::Dark ? "dark" : "light";
            MainWindow main(user);
            auto* sidebar = main.findChild<SidebarWidget*>();
            for (const auto& [page, name] : QList<QPair<SidebarWidget::Page, QString>>{
                     {SidebarWidget::Page::Dashboard, "dashboard"}, {SidebarWidget::Page::Budget, "budget"},
                     {SidebarWidget::Page::Reports, "reports"}}) {
                emit sidebar->pageRequested(page);
                if (page == SidebarWidget::Page::Budget) {      // Last month, complete, one budget over
                    main.findChild<BudgetPage*>()->showMonth(QDate(today.year(), today.month(), 1).addMonths(-1));
                    for (QAbstractButton* row : main.findChildren<QAbstractButton*>("budgetRow")) {
                        if (row->accessibleName() == "Groceries") row->click();   // Unfolded: its month as a chart
                    }
                }
                shots.insert(name + "-" + t, draw(main, window));
            }
            emit sidebar->pageRequested(SidebarWidget::Page::Reports);
            main.findChild<ReportsPage*>()->openReport(ReportsPage::Report::MoneyFlow);
            shots.insert("moneyflow-" + t, draw(main, window));
            emit sidebar->pageRequested(SidebarWidget::Page::Transactions);
            main.setAttribute(Qt::WA_DontShowOnScreen);
            main.resize(window);
            main.show();
            QCoreApplication::processEvents();
            main.findChild<TransactionsPage*>()->openTransaction(dinner);
            QCoreApplication::processEvents();
            shots.insert("transactions-" + t, main.grab().toImage());
            main.hide();
        }
        // The ones the README shows, under "More screenshots"
        QDir().mkpath(out);
        for (const char* name : {"dashboard-light", "transactions-dark", "budget-dark", "reports-light"}) {
            // Lossless and as drawn, at the screen's pixels: scaling and lossy WebP blur the text
            QVERIFY(shots[name].save(out + "/" + name + ".webp", "webp", 100));
        }

        // The collage: the dark Dashboard and money flow at the back, the light Transactions page in front
        const QSize canvas(2400, 1640);
        QImage collage(canvas, QImage::Format_ARGB32_Premultiplied);
        collage.fill(Qt::transparent);
        QPainter painter(&collage);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        const auto place = [&painter](const QImage& shot, const QRect& where) {
            const qreal radius = 22;
            for (int i = 14; i > 0; --i) {                   // A soft shadow
                QPainterPath shadow;
                shadow.addRoundedRect(QRectF(where).adjusted(-i, -i + 10, i, i + 10), radius + i, radius + i);
                painter.fillPath(shadow, QColor(0, 0, 0, 7));
            }
            QPainterPath clip;
            clip.addRoundedRect(QRectF(where), radius, radius);
            painter.save();
            painter.setClipPath(clip);
            painter.drawImage(where, shot.scaled(where.size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            painter.restore();
            painter.setPen(QPen(QColor(0, 0, 0, 40), 1.5));
            painter.drawPath(clip);
        };
        const int width = 1440;
        const int height = width * window.height() / window.width();
        place(shots["moneyflow-dark"], QRect(canvas.width() - width - 40, 40, width, height));
        place(shots["dashboard-dark"], QRect(40, 110, width, height));
        place(shots["transactions-light"], QRect((canvas.width() - width) / 2, canvas.height() - height - 40, width, height));
        painter.end();
        QVERIFY(collage.save(out + "/collage.webp", "webp", 100));
    }

    void dashboardChartShowsMonthsOnHover() {
        Theme::applyTheme(Theme::Type::Light);
        MainWindow window(1);
        draw(window);
        auto* chart = window.findChild<QWidget*>("cashFlowChart");
        QVERIFY(chart);
        for (int x = 10; x < chart->width(); x += chart->width() / 8) {
            QTest::mouseMove(chart, QPoint(x, chart->height() / 2));
        }
        QTest::mouseMove(chart, QPoint(chart->width() - 2, 2));                 // Outside the bars
        QVERIFY(!chart->grab().isNull());
    }

    void dialogsDrawInBothThemes() {
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            StartWindow start;
            QVERIFY(!draw(start, QSize(700, 500)).isNull());
            ImportManagerDialog import(1);
            QVERIFY(!draw(import, QSize(780, 680)).isNull());
            BalanceCorrectionDialog correction(1, -1);
            QVERIFY(!draw(correction, QSize(450, 300)).isNull());
        }
    }

    void iconFilesForStyleSheetsAreTintedAndSharp() {
        const QString red = Icons::file("chevron-down", QColor(200, 0, 0), 14);
        QCOMPARE(Icons::file("chevron-down", QColor(200, 0, 0), 14), red);          // Made once
        QVERIFY(Icons::file("chevron-down", QColor(0, 0, 200), 14) != red);
        QVERIFY(Icons::file("chevron-down", QColor(200, 0, 0), 16) != red);
        const QImage image(red);
        const QImage sharp(QString(red).replace(".png", "@2x.png"));
        QCOMPARE(image.size(), QSize(14, 14));
        QCOMPARE(sharp.size(), QSize(28, 28));
        int painted = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QColor pixel = image.pixelColor(x, y);
                if (pixel.alpha() == 255) {
                    QCOMPARE(pixel.rgb(), QColor(200, 0, 0).rgb());
                    ++painted;
                }
            }
        }
        QVERIFY(painted > 0);
    }

    void theControlStyleFollowsTheTheme() {
        const auto restore = qScopeGuard([] { Theme::applyTheme(Theme::Type::Dark); });
        Theme::applyTheme(Theme::Type::Light);
        const QString light = Theme::controlStyleSheet();
        Theme::applyTheme(Theme::Type::Dark);
        const QString dark = Theme::controlStyleSheet();
        QVERIFY(light != dark);
        for (const QString& sheet : {light, dark}) {
            QString rest = sheet;
            rest.remove(QRegularExpression("url\\([^)]*\\)"));         // A temporary folder's name can be in capitals
            QVERIFY2(!rest.contains(QRegularExpression("[A-Z]{3,}")), qPrintable(rest));   // Every name filled in
            // ...each by its own value, not a shorter name's value inside a longer name
            QVERIFY2(!rest.contains(QRegularExpression("[A-Za-z)]rgba\\(")), qPrintable(rest));
            int images = 0;                                          // Arrows and ticks, every one there
            for (const QRegularExpressionMatch& url : QRegularExpression("url\\(([^)]*)\\)").globalMatch(sheet)) {
                QVERIFY2(QFile::exists(url.captured(1)), qPrintable(url.captured(1)));
                ++images;
            }
            QVERIFY(images >= 5);
        }
        const auto rgba = [](const QColor& color) {
            return QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
        };
        QVERIFY(light.contains("color: " + rgba(Theme::colors(false).textPrimary)));
        QVERIFY(dark.contains("color: " + rgba(Theme::colors(true).textPrimary)));
        // Fields are white in the light theme, not the dark theme's grey
        QVERIFY(light.contains("QLineEdit, QComboBox, QAbstractSpinBox { background-color: " + rgba(Qt::white)));
        // Their border is the light hairline (black at 38 of 255) mixed into the page's grey, as a solid colour
        const QRegularExpressionMatch fieldBorder = QRegularExpression("QAbstractSpinBox \\{[^}]*border: 1px solid rgba\\((\\d+), (\\d+), (\\d+), 255\\)").match(light);
        QVERIFY(fieldBorder.hasMatch());
        const double expected = Theme::pageBackground(false).red() * (1 - 38 / 255.0);
        for (int channel = 1; channel <= 3; ++channel) {
            QVERIFY2(qAbs(fieldBorder.captured(channel).toInt() - expected) <= 1, qPrintable(fieldBorder.captured(0)));
        }
        QVERIFY(!dark.contains("QLineEdit, QComboBox, QAbstractSpinBox { background-color: " + rgba(Qt::white)));
    }

    void cardsAreRoundedAndEveryPanelIsOne() {
        const auto restore = qScopeGuard([] { Theme::applyTheme(Theme::Type::Dark); });
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            const bool dark = theme == Theme::Type::Dark;
            QWidget page;
            page.setAutoFillBackground(true);
            auto* layout = new QVBoxLayout(&page);
            layout->setContentsMargins(10, 10, 10, 10);
            auto* card = new QFrame(&page);
            Theme::makeCard(card);
            layout->addWidget(card);
            draw(page, QSize(220, 140));
            const QImage image = page.grab().toImage();                 // The card on the page, at 1:1
            const qreal dpr = image.devicePixelRatio();
            const auto at = [&image, dpr, card](int x, int y) {
                const QPoint point = card->mapTo(card->parentWidget(), QPoint(x, y)) * dpr;
                return image.pixelColor(point).rgb();
            };
            QCOMPARE(at(card->width() / 2, card->height() / 2), Theme::cardBackground(dark).rgb());
            QCOMPARE(at(1, 1), Theme::pageBackground(dark).rgb());      // Outside the round corner
            QCOMPARE(at(card->width() / 2, 0), Theme::hairline(dark).rgb());

            // A list or scroll area as a card doesn't paint its square background over the corners
            // A link: no box, the link colour, in this theme's shade
            QToolButton link;
            Theme::makeLink(&link);
            QVERIFY(link.property("link").toBool() && link.autoRaise());
            const QColor shade = Theme::linkColor(dark);
            QVERIFY(qApp->styleSheet().contains(QString("QToolButton[link=\"true\"] { color: rgba(%1, %2, %3, 255)")
                                                    .arg(shade.red()).arg(shade.green()).arg(shade.blue())));
            QVERIFY(Theme::linkColor(true).lightness() > Theme::linkColor(false).lightness());   // Lighter on dark grey

            QScrollArea area;
            Theme::makeCard(&area);
            QVERIFY(area.property("card").toBool());
            QVERIFY(!area.viewport()->autoFillBackground());

            // Every page's panels are cards: none left with Fusion's square frame
            MainWindow window(1);
            auto* sidebar = window.findChild<SidebarWidget*>();
            for (const auto shown : {SidebarWidget::Page::Dashboard, SidebarWidget::Page::Transactions,
                                     SidebarWidget::Page::Budget, SidebarWidget::Page::Reports, SidebarWidget::Page::Settings}) {
                emit sidebar->pageRequested(shown);
                draw(window, QSize(1300, 800));
                for (QToolButton* button : window.findChildren<QToolButton*>()) {   // What goes elsewhere is a link
                    if (button->text().endsWith("›")) QVERIFY2(button->property("link").toBool(), qPrintable(button->text()));
                }
                for (QFrame* frame : window.findChildren<QFrame*>()) {
                    if (!frame->isVisibleTo(&window) || frame->frameShape() != QFrame::StyledPanel) continue;   // draw() hides the window again
                    QVERIFY2(frame->property("card").toBool(),
                             qPrintable(QString("%1 %2").arg(frame->metaObject()->className(), frame->objectName())));
                }
            }
        }
    }

    void emptyStatesHaveAnIconAboveTheirText() {
        const auto restore = qScopeGuard([] { Theme::applyTheme(Theme::Type::Dark); });
        Theme::applyTheme(Theme::Type::Light);
        QWidget page;
        auto* layout = new QVBoxLayout(&page);
        auto* empty = new EmptyState("budget", "No budgets yet.", &page);
        layout->addWidget(empty);
        draw(page, QSize(400, 260));
        QCOMPARE(empty->foregroundRole(), QPalette::PlaceholderText);
        QCOMPARE(empty->alignment(), Qt::AlignCenter);
        // The icon is centred, above the text, and icon and text together sit in the middle
        const QRect icon = empty->iconRect();
        QCOMPARE(icon.size(), QSize(EmptyState::IconSize, EmptyState::IconSize));
        QVERIFY(qAbs(icon.center().x() - empty->width() / 2) <= 1);
        const int textHeight = empty->fontMetrics().height();
        const int blockTop = icon.top();
        const int blockBottom = icon.bottom() + EmptyState::Gap + textHeight;
        QVERIFY2(qAbs((blockTop + blockBottom) / 2 - empty->height() / 2) <= 2,
                 qPrintable(QString("%1..%2 of %3").arg(blockTop).arg(blockBottom).arg(empty->height())));
        // Drawn in the muted colour: its strokes, the darkest of its pixels on the light page, are it
        const QImage image = empty->grab().toImage();
        const qreal dpr = image.devicePixelRatio();
        int darkest = 255;
        for (int y = int(icon.top() * dpr); y <= int(icon.bottom() * dpr); ++y) {
            for (int x = int(icon.left() * dpr); x <= int(icon.right() * dpr); ++x) {
                darkest = qMin(darkest, image.pixelColor(x, y).red());
            }
        }
        QVERIFY2(qAbs(darkest - Theme::colors().textMuted.red()) <= 8, qPrintable(QString::number(darkest)));
    }

    void formLabelsSitBesideTheirField() {
        const auto restore = qScopeGuard([] { Theme::applyTheme(Theme::Type::Dark); });
        Theme::applyTheme(Theme::Type::Light);
        QWidget page;
        auto* form = new QFormLayout(&page);
        auto* label = new QLabel("Name", &page);
        auto* field = new QLineEdit(&page);
        form->addRow(label, field);
        auto* typeLabel = new QLabel("Type", &page);             // Beside a drop-down in a row of its own
        auto* row = new QHBoxLayout();
        auto* type = new QComboBox(&page);
        row->addWidget(type);
        form->addRow(typeLabel, row);
        auto* versionLabel = new QLabel("Version", &page);       // Beside plain text: stays as high as it is
        auto* version = new QLabel("1.0", &page);
        form->addRow(versionLabel, version);
        draw(page, QSize(300, 160));
        QCOMPARE(field->height(), Theme::controlHeight);
        for (const auto& [beside, control] : {std::pair<QLabel*, QWidget*>{label, field}, {typeLabel, type}}) {
            const int labelMiddle = beside->geometry().center().y();
            const int controlMiddle = control->mapTo(&page, control->rect().center()).y();
            QVERIFY2(qAbs(labelMiddle - controlMiddle) <= 1,
                     qPrintable(QString("%1 at %2, its control at %3").arg(beside->text()).arg(labelMiddle).arg(controlMiddle)));
        }
        QVERIFY(versionLabel->height() < Theme::controlHeight);
    }

    void dialogsHaveCardsAndTheRightButtons() {
        const int giro = AccountDAO().getAllAccountsForUser(1).first().getAccountId();
        ApplyRulesDialog rules(1);
        DeleteAccountDialog del(1, giro);
        EditAccountDialog edit(1, giro);
        BalanceCorrectionDialog correction(1, giro);
        ImportManagerDialog import(1);
        for (QDialog* dialog : std::initializer_list<QDialog*>{&rules, &del, &edit, &correction, &import}) {
            draw(*dialog, QSize(780, 600));
            for (QFrame* frame : dialog->findChildren<QFrame*>()) {
                if (!frame->isVisibleTo(dialog) || frame->frameShape() != QFrame::StyledPanel) continue;
                QVERIFY2(frame->property("card").toBool(), qPrintable(QString("%1 in %2").arg(frame->metaObject()->className(),
                                                                                              dialog->metaObject()->className())));
            }
        }
        auto* deleteButton = del.findChild<QPushButton*>("deleteAccountButton");
        QCOMPARE(deleteButton->palette().color(QPalette::ButtonText), Theme::colors().danger);   // Red, though set up late

        // An empty list keeps its card, and says so with its icon
        TransactionsPage page;
        page.setUserId(99);
        auto* empty = page.findChild<QLabel*>("emptyLabel");
        QVERIFY(qobject_cast<EmptyState*>(empty));
        QVERIFY(empty->property("card").toBool());
    }

    void theWholeAppHasTheControlLook() {
        const auto restore = qScopeGuard([] { Theme::applyTheme(Theme::Type::Dark); });
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            QVERIFY(qApp->styleSheet().contains(Theme::controlStyleSheet()));   // Of this theme
            QVERIFY(qApp->styleSheet().contains("QToolTip {"));

            // Drawn so: a primary button in the accent colour, a ticked checkbox too
            QWidget window;
            auto* layout = new QVBoxLayout(&window);
            auto* primary = new QPushButton("Add", &window);
            primary->setProperty("primary", true);
            primary->setMinimumWidth(120);
            auto* check = new QCheckBox(&window);
            check->setChecked(true);
            auto* save = new QPushButton("Save", &window);   // A dialog's main button
            save->setDefault(true);
            save->setMinimumWidth(120);
            auto* cancel = new QPushButton("Cancel", &window);
            cancel->setMinimumWidth(120);
            auto* spin = new QSpinBox(&window);              // Its number inset like any field's text, as high
            layout->addWidget(spin);
            layout->addWidget(primary);
            layout->addWidget(check);
            layout->addWidget(save);
            layout->addWidget(cancel);
            draw(window, QSize(200, 220));
            const QColor accent = Theme::colors().accent;
            QCOMPARE(primary->grab().toImage().pixelColor(6, primary->height() / 2).rgb(), accent.rgb());
            QCOMPARE(save->grab().toImage().pixelColor(6, save->height() / 2).rgb(), accent.rgb());
            QVERIFY(cancel->grab().toImage().pixelColor(6, cancel->height() / 2).rgb() != accent.rgb());
            QCOMPARE(spin->findChild<QLineEdit*>()->textMargins().left(), Theme::FieldInset);
            QCOMPARE(spin->height(), Theme::controlHeight);
            for (const QRegularExpressionMatch& match : QRegularExpression("border(?:-[a-z]+)?: 1px solid rgba\\((\\d+), (\\d+), (\\d+), (\\d+)\\)")
                                                            .globalMatch(qApp->styleSheet())) {
                QVERIFY2(match.captured(4) == "255", qPrintable(match.captured(0)));   // Solid, or corners get dark dots
            }
            QCOMPARE(primary->height(), Theme::controlHeight);
            QStyleOptionButton option;
            option.initFrom(check);
            const QRect box = check->style()->subElementRect(QStyle::SE_CheckBoxIndicator, &option, check);
            QCOMPARE(check->grab().toImage().pixelColor(box.left() + 2, box.center().y()).rgb(), accent.rgb());
        }
        Theme::applyTheme(Theme::Type::Light);
        const QString light = qApp->styleSheet();
        Theme::applyTheme(Theme::Type::Dark);
        QVERIFY(qApp->styleSheet() != light);                // Set again with the other theme's colours
    }

    void disabledTextLooksDisabledInBothThemes() {
        for (const Theme::Type theme : {Theme::Type::Light, Theme::Type::Dark}) {
            Theme::applyTheme(theme);
            const QPalette palette = qApp->palette();
            for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) {
                QVERIFY(palette.color(QPalette::Disabled, role) != palette.color(QPalette::Active, role));
            }
        }
    }

    void systemThemeFollowsTheOperatingSystem() {
        Theme::applyTheme(Theme::Type::System);
        const bool systemDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
        QCOMPARE(Theme::isDark(), systemDark);
        QCOMPARE(Theme::fromName("light"), Theme::Type::Light);
        QCOMPARE(Theme::fromName("System"), Theme::Type::System);
        QCOMPARE(Theme::fromName("anything else"), Theme::Type::Dark);
    }
};

QTEST_MAIN(TestRendering)
#include "test_rendering.moc"
