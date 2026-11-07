#include <QtTest>
#include <QAbstractButton>
#include <QCheckBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>
#include <QListWidget>
#include <QScrollArea>
#include <QAbstractSpinBox>
#include <QSpinBox>
#include <QLineEdit>
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
                for (QFrame* frame : window.findChildren<QFrame*>()) {
                    if (!frame->isVisibleTo(&window) || frame->frameShape() != QFrame::StyledPanel) continue;   // draw() hides the window again
                    QVERIFY2(frame->property("card").toBool(),
                             qPrintable(QString("%1 %2").arg(frame->metaObject()->className(), frame->objectName())));
                }
            }
        }
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
