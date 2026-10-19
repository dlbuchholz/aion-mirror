#include <QtTest>
#include <QAbstractButton>
#include <QListWidget>
#include <QSqlQuery>
#include <QStackedWidget>

#include "account.h"
#include "budget.h"
#include "dialog/balancecorrectiondialog.h"
#include "dialog/importmanagerdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "startwindow.h"
#include "tests/test_db_fixture.h"
#include "theme.h"
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
        QVERIFY(query.exec("INSERT INTO users (id, username, name) VALUES (1, 'alex', 'Alex Example')"));
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
