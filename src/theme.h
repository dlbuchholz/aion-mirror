#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QColor>
#include <QAbstractButton>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QLineEdit>
#include <QSpinBox>
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QStyleFactory>
#include <QPalette>
#include "icons.h"

class Theme {
public:
    enum class Type {
        Dark,
        Light,
        System
    };

    // Colour roles that QPalette has no slot for (sidebar, semantic colours)
    struct Colors {
        QColor sidebarBackground;
        QColor sidebarBorder;
        QColor itemHover;
        QColor itemSelected;
        QColor textPrimary;
        QColor textSecondary;
        QColor textMuted;
        QColor accent;
        QColor onAccent;
        QColor danger;
        QColor positive;                             // Income amounts
        QColor track;
        QColor warning;                              // Text and icons on a warning tint
        QColor onWarning;                            // Text and icons on a solid warning fill
    };

    // Maps the stored setting value ("Dark", "Light", "System") to a theme type
    static Type fromName(const QString& name) {
        if (name.compare("Light", Qt::CaseInsensitive) == 0) return Type::Light;
        if (name.compare("System", Qt::CaseInsensitive) == 0) return Type::System;
        return Type::Dark;
    }

    // Sets the theme for the application. System follows the OS light/dark preference.
    static void applyTheme(Type themeType) {
        switch (themeType) {
        case Type::Dark:
            applyDarkTheme();
            break;
        case Type::Light:
            applyLightTheme();
            break;
        case Type::System:
        default:
            applySystemTheme();
            break;
        }
    }

    static bool isDark() {
        return darkActive;
    }

    static Colors colors() {
        return colors(darkActive);
    }

    // Of either theme, whichever is active: for previews of the other one
    static Colors colors(bool dark) {
        if (dark) {
            return {
                QColor(30, 30, 34),          // sidebarBackground
                QColor(255, 255, 255, 22),   // sidebarBorder
                QColor(255, 255, 255, 16),   // itemHover
                QColor(255, 255, 255, 30),   // itemSelected
                QColor(236, 236, 238),       // textPrimary
                QColor(205, 205, 210),       // textSecondary
                QColor(160, 160, 166),       // textMuted
                QColor(42, 130, 218),        // accent
                Qt::white,                   // onAccent
                QColor(229, 83, 75),         // danger
                QColor(87, 196, 125),        // positive
                QColor(255, 255, 255, 38),   // track
                QColor(250, 199, 117),       // warning
                QColor(45, 30, 5)            // onWarning
            };
        }
        return {
            QColor(233, 233, 236),           // sidebarBackground
            QColor(0, 0, 0, 20),             // sidebarBorder
            QColor(0, 0, 0, 13),             // itemHover
            QColor(0, 0, 0, 24),             // itemSelected
            QColor(30, 30, 30),              // textPrimary
            QColor(74, 74, 74),              // textSecondary
            QColor(110, 110, 114),           // textMuted
            QColor(42, 130, 218),            // accent
            Qt::white,                       // onAccent
            QColor(201, 55, 44),             // danger
            QColor(30, 135, 70),             // positive
            QColor(0, 0, 0, 26),             // track
            QColor(133, 79, 11),             // warning
            QColor(65, 36, 2)                // onWarning
        };
    }

    // The window background of either theme
    static QColor pageBackground(bool dark) {
        return dark ? QColor(38, 38, 42) : QColor(245, 245, 245);
    }

    static constexpr int controlHeight = 32;        // Fields, drop-downs, steppers and segments in a row

    // The app's own look for controls, in place of Fusion's square boxes: one height, rounded
    // corners, hairline borders and quiet hovers. applyTheme sets it on the whole app:
    //   QLineEdit, QComboBox, spin and     fields; a QComboBox's list and a QDateEdit's calendar
    //   date boxes, QTextEdit              button too
    //   QCheckBox, QRadioButton            rounded boxes, ticked or dotted in the accent colour
    //   QMenu, QToolTip                    soft popups with rounded items
    //   QToolButton[chip="true"]           a toggle; accent tint while checked
    //   QFrame[segmented="true"]           a track whose tool or push buttons are its segments
    //   QFrame[stepper="true"]             "‹ name ›" in one box; the QToolButton[stepperName="true"] between dividers
    //   QFrame[popup="true"]               a rounded popup; QAbstractButton[thisMonth="true"] is outlined
    //   QPushButton                        a button; [primary="true"] and a dialog's default one are the solid
    //                                      accent ones, [danger="true"] has red text, and a checkable one
    //                                      is tinted while checked
    //   QToolButton[button="true"]         the same, plus [danger], [ghost] (no box), [dropDown] (room for
    //                                      its menu's arrow) and [noIndicator] (a menu without the arrow)
    //   QToolButton[warning="true"]        a toggle asking for attention; solid amber while checked
    //   QFrame[selectionBar="true"]        an accent band for actions on what's selected
    // Its colours are the active theme's, so applyTheme sets it again with each theme.
    static QString controlStyleSheet() {
        const bool dark = darkActive;
        const Colors c = colors();
        const auto rgba = [](const QColor& color) {
            return QString("rgba(%1, %2, %3, %4)").arg(color.red()).arg(color.green()).arg(color.blue()).arg(color.alpha());
        };
        const QString field = rgba(dark ? QColor(46, 46, 51) : QColor(255, 255, 255));
        const QString raised = rgba(dark ? QColor(66, 66, 72) : QColor(255, 255, 255));
        const QString popup = rgba(dark ? QColor(50, 50, 55) : QColor(255, 255, 255));
        const QString track = rgba(dark ? QColor(30, 30, 34) : QColor(232, 232, 235));
        // Borders are solid colours, mixed against the page beforehand: a style sheet draws a rounded
        // border's corners and sides one by one, and where a see-through colour overlaps itself it
        // gets darker, which shows as dots at each corner
        const QColor page = pageBackground(dark);
        const auto solid = [&page, &rgba](const QColor& color) {
            const qreal a = color.alphaF();
            return rgba(QColor::fromRgbF(color.redF() * a + page.redF() * (1 - a), color.greenF() * a + page.greenF() * (1 - a),
                                         color.blueF() * a + page.blueF() * (1 - a)));
        };
        const QString border = solid(dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 38));
        const QString borderHover = solid(dark ? QColor(255, 255, 255, 70) : QColor(0, 0, 0, 80));
        const QString divider = solid(dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 22));
        const QString hover = rgba(c.itemHover);
        const QString accent = rgba(c.accent);
        QColor tint = c.accent;
        tint.setAlpha(dark ? 60 : 36);
        const QString accentTint = rgba(tint);
        const QString accentText = rgba(dark ? QColor(130, 185, 240) : QColor(24, 95, 165));
        const QString text = rgba(c.textPrimary);
        const QString secondary = rgba(c.textSecondary);
        const QString disabled = rgba(dark ? QColor(110, 110, 116) : QColor(170, 170, 174));
        const QString arrow = Icons::file("chevron-down", c.textMuted, 14);
        const QString arrowUp = Icons::file("chevron-up", c.textMuted, 12);
        const QString arrowDown = Icons::file("chevron-down", c.textMuted, 12);
        const QString tick = Icons::file("check", c.onAccent, 12);
        const QString menuTick = Icons::file("check", c.textPrimary, 14);
        const QString onAccent = rgba(c.onAccent);
        const QString accentHover = rgba(dark ? QColor(60, 145, 230) : QColor(30, 115, 200));
        const auto accentAlpha = [&c, &rgba](int alpha) {
            QColor color = c.accent;
            color.setAlpha(alpha);
            return rgba(color);
        };
        const QString accentWash = accentAlpha(dark ? 28 : 18);
        QColor accentEdge = c.accent;
        accentEdge.setAlpha(dark ? 110 : 90);
        const QString accentBorder = solid(accentEdge);
        const QString danger = rgba(c.danger);
        const QString warning = rgba(c.warning);
        const QString onWarning = rgba(c.onWarning);
        const QString warnTint = rgba(dark ? QColor(239, 159, 39, 40) : QColor(250, 238, 218));
        const QString warnBorder = solid(dark ? QColor(239, 159, 39, 110) : QColor(239, 159, 39, 150));
        const QString warnFill = rgba(QColor(239, 159, 39));

        QString sheet = R"(
QLineEdit, QComboBox, QAbstractSpinBox { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 0 8px; min-height: 30px; }
QLineEdit:hover, QComboBox:hover, QAbstractSpinBox:hover { border-color: BORDERHOVER; }
QLineEdit:focus, QComboBox:focus, QComboBox:on, QAbstractSpinBox:focus { border-color: ACCENT; }
QLineEdit:disabled, QComboBox:disabled, QAbstractSpinBox:disabled { color: DISABLED; background-color: TRACK; border-color: DIVIDER; }
QComboBox QLineEdit, QAbstractSpinBox QLineEdit { border: none; background: transparent; padding: 0; min-height: 0; }
QTextEdit, QPlainTextEdit { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 4px; }
QTextEdit:hover, QPlainTextEdit:hover { border-color: BORDERHOVER; }
QTextEdit:focus, QPlainTextEdit:focus { border-color: ACCENT; }
QComboBox::drop-down, QDateEdit::drop-down { border: none; width: 26px; }
QComboBox::down-arrow, QDateEdit::down-arrow { image: url(@arrow@); width: 14px; height: 14px; }
QComboBox QAbstractItemView { background-color: POPUP; color: TEXT; border: 1px solid BORDER; outline: 0; padding: 4px; selection-background-color: HOVER; selection-color: TEXT; }
QAbstractSpinBox { padding-right: 22px; max-height: 30px; }
QAbstractSpinBox::up-button, QAbstractSpinBox::down-button { border: none; background: transparent; width: 20px; subcontrol-origin: border; }
QAbstractSpinBox::up-button { subcontrol-position: top right; margin: 3px 2px 0 0; }
QAbstractSpinBox::down-button { subcontrol-position: bottom right; margin: 0 2px 3px 0; }
QAbstractSpinBox::up-button:hover, QAbstractSpinBox::down-button:hover { background-color: HOVER; border-radius: 4px; }
QAbstractSpinBox::up-arrow { image: url(@up@); width: 12px; height: 12px; }
QAbstractSpinBox::down-arrow { image: url(@down@); width: 12px; height: 12px; }

QCheckBox, QRadioButton { spacing: 8px; }
QCheckBox::indicator, QRadioButton::indicator { width: 16px; height: 16px; background-color: FIELD; border: 1px solid BORDERHOVER; }
QCheckBox::indicator { border-radius: 4px; }
QRadioButton::indicator { border-radius: 9px; }
QCheckBox::indicator:hover, QRadioButton::indicator:hover { border-color: ACCENT; }
QCheckBox::indicator:checked { background-color: ACCENT; border-color: ACCENT; image: url(@tick@); }
QRadioButton::indicator:checked { background-color: ONACCENT; border: 5px solid ACCENT; width: 8px; height: 8px; }
QCheckBox::indicator:disabled, QRadioButton::indicator:disabled { background-color: TRACK; border-color: DIVIDER; }

QMenu { background-color: POPUP; color: TEXT; border: 1px solid BORDER; padding: 4px; }
QMenu::item { padding: 6px 24px 6px 26px; border-radius: 6px; background: transparent; }
QMenu::item:selected { background-color: HOVER; }
QMenu::item:disabled { color: DISABLED; }
QMenu::separator { height: 1px; background-color: DIVIDER; margin: 4px 6px; }
QMenu::indicator { width: 14px; height: 14px; left: 6px; }
QMenu::indicator:checked { image: url(@menutick@); }

QToolButton[chip="true"] { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 0 10px; }
QToolButton[chip="true"]:hover { border-color: BORDERHOVER; }
QToolButton[chip="true"]:checked { background-color: ACCENTTINT; color: ACCENTTEXT; border-color: ACCENT; }

QFrame[segmented="true"] { background-color: TRACK; border: 1px solid BORDER; border-radius: 8px; }
QFrame[segmented="true"] QToolButton, QFrame[segmented="true"] QPushButton { background: transparent; color: SECONDARY; border: 1px solid transparent; border-radius: 6px; padding: 0 11px; min-height: 0; }
QFrame[segmented="true"] QToolButton:hover, QFrame[segmented="true"] QPushButton:hover { color: TEXT; background-color: HOVER; }
QFrame[segmented="true"] QToolButton:checked, QFrame[segmented="true"] QPushButton:checked { background-color: RAISED; color: TEXT; border-color: BORDER; }

QFrame[stepper="true"] { background-color: FIELD; border: 1px solid BORDER; border-radius: 8px; }
QFrame[stepper="true"]:hover { border-color: BORDERHOVER; }
QFrame[stepper="true"] QToolButton { background: transparent; color: TEXT; border: none; border-radius: 6px; padding: 0 6px; }
QFrame[stepper="true"] QToolButton:hover { background-color: HOVER; }
QFrame[stepper="true"] QToolButton[stepperName="true"] { border-radius: 0; border-left: 1px solid DIVIDER; border-right: 1px solid DIVIDER; padding: 0 10px; }

QFrame[popup="true"] { background-color: POPUP; border: 1px solid BORDER; border-radius: 10px; }
QFrame[popup="true"] QAbstractButton { background: transparent; color: TEXT; border: 1px solid transparent; border-radius: 6px; padding: 5px 10px; text-align: left; min-height: 0; }
QFrame[popup="true"] QAbstractButton:hover { background-color: HOVER; }
QFrame[popup="true"] QAbstractButton:disabled { color: DISABLED; }
QFrame[popup="true"] QAbstractButton:checked { background-color: ACCENTTINT; color: ACCENTTEXT; }
QFrame[popup="true"] QAbstractButton[thisMonth="true"] { border-color: BORDERHOVER; }
QFrame[popup="true"] QFrame#divider { background-color: DIVIDER; border: none; }
QFrame[popup="true"] QLabel { color: TEXT; background: transparent; }

QPushButton, QToolButton[button="true"] { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 0 12px; }
QPushButton { min-height: 30px; }
QPushButton:hover, QToolButton[button="true"]:hover { border-color: BORDERHOVER; }
QPushButton:pressed, QToolButton[button="true"]:pressed { background-color: HOVER; }
QPushButton:checked { background-color: ACCENTTINT; color: ACCENTTEXT; border-color: ACCENT; }
QPushButton:disabled, QToolButton[button="true"]:disabled { color: DISABLED; border-color: DIVIDER; }
QPushButton[primary="true"], QPushButton:default { background-color: ACCENT; color: ONACCENT; border-color: ACCENT; }
QPushButton[primary="true"]:hover, QPushButton:default:hover { background-color: ACCENTHOVER; border-color: ACCENTHOVER; }
QPushButton[primary="true"]:disabled, QPushButton:default:disabled { background-color: ACCENTTINT; color: DISABLED; border-color: transparent; }
QPushButton[danger="true"], QToolButton[button="true"][danger="true"] { color: DANGER; }
QToolButton[button="true"][ghost="true"] { background: transparent; border-color: transparent; padding: 0 6px; }
QToolButton[button="true"][ghost="true"]:hover { background-color: HOVER; }
QToolButton[button="true"][dropDown="true"] { padding-right: 28px; }
QToolButton[button="true"]::menu-indicator { image: url(@arrow@); subcontrol-origin: padding; subcontrol-position: center right; right: 8px; width: 14px; height: 14px; }
QToolButton[button="true"][noIndicator="true"] { padding: 0; }
QToolButton[button="true"][noIndicator="true"]::menu-indicator { image: none; width: 0; }

QToolButton[warning="true"] { background-color: WARNTINT; color: WARNING; border: 1px solid WARNBORDER; border-radius: 8px; padding: 0 10px; }
QToolButton[warning="true"]:hover { border-color: WARNFILL; }
QToolButton[warning="true"]:checked { background-color: WARNFILL; color: ONWARNING; border-color: WARNFILL; }

QFrame[selectionBar="true"] { background-color: ACCENTWASH; border: 1px solid ACCENTBORDER; border-radius: 10px; }
QFrame[selectionBar="true"] QLabel { color: ACCENTTEXT; font-weight: bold; background: transparent; }
)";
        // Longer names before those they contain, so BORDER doesn't replace the end of ACCENTBORDER
        const QList<QPair<QString, QString>> values = {
            {"ACCENTBORDER", accentBorder}, {"ACCENTHOVER", accentHover}, {"ACCENTWASH", accentWash},
            {"ONACCENT", onAccent}, {"ONWARNING", onWarning}, {"WARNBORDER", warnBorder}, {"WARNTINT", warnTint},
            {"WARNFILL", warnFill}, {"WARNING", warning}, {"DANGER", danger},
            {"BORDERHOVER", borderHover}, {"ACCENTTINT", accentTint}, {"ACCENTTEXT", accentText},
            {"SECONDARY", secondary}, {"DISABLED", disabled}, {"DIVIDER", divider}, {"BORDER", border},
            {"ACCENT", accent}, {"RAISED", raised}, {"FIELD", field}, {"TRACK", track}, {"HOVER", hover},
            {"POPUP", popup}, {"TEXT", text},
            // Icon files last, by names in lower case: a random folder name could hold a name above
            {"@arrow@", arrow}, {"@up@", arrowUp}, {"@down@", arrowDown}, {"@tick@", tick}, {"@menutick@", menuTick},
        };
        for (const auto& [name, value] : values) {
            sheet.replace(name, value);
        }
        return sheet;
    }

private:
    static inline bool darkActive = true;

    static void applyDarkTheme() {
        darkActive = true;
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        // Built from button and window colours so Qt derives matching bevel shades
        QPalette darkPalette(QColor(56, 56, 62), pageBackground(true));
        darkPalette.setColor(QPalette::Base, QColor(46, 46, 51));
        darkPalette.setColor(QPalette::AlternateBase, QColor(52, 52, 58));
        darkPalette.setColor(QPalette::Text, QColor(236, 236, 238));
        darkPalette.setColor(QPalette::WindowText, QColor(236, 236, 238));
        darkPalette.setColor(QPalette::ButtonText, QColor(236, 236, 238));
        darkPalette.setColor(QPalette::BrightText, Qt::white);
        darkPalette.setColor(QPalette::PlaceholderText, QColor(150, 150, 156));
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        darkPalette.setColor(QPalette::Link, QColor(90, 160, 235));
        darkPalette.setColor(QPalette::ToolTipBase, QColor(56, 56, 62));
        darkPalette.setColor(QPalette::ToolTipText, QColor(236, 236, 238));
        for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) {
            darkPalette.setColor(QPalette::Disabled, role, QColor(125, 125, 130));
        }

        qApp->setPalette(darkPalette);
        styleToolTips(QColor(56, 56, 62), QColor(236, 236, 238), QColor(255, 255, 255, 40));
    }

    // The app's control look, and tooltips: Fusion draws those with a hard frame in the text
    // colour; a soft one fits the app better
    static void styleToolTips(const QColor& background, const QColor& text, const QColor& border) {
        const qreal a = border.alphaF();                   // Solid, mixed against the tooltip's background
        const QColor edge = QColor::fromRgbF(border.redF() * a + background.redF() * (1 - a),
                                             border.greenF() * a + background.greenF() * (1 - a),
                                             border.blueF() * a + background.blueF() * (1 - a));
        const QString borderColor = QString("rgba(%1, %2, %3, 255)").arg(edge.red()).arg(edge.green()).arg(edge.blue());
        qApp->setStyleSheet(QString("QToolTip { background-color: %1; color: %2; border: 1px solid %3; padding: 4px 6px; }")
                                .arg(background.name(), text.name(), borderColor)
                            + controlStyleSheet());
        static SpinBoxInset inset;                   // Once for the app, whichever theme comes first
        qApp->installEventFilter(&inset);            // Installing it again changes nothing
    }

    // A spin box's number starts where the text of every other field does. Style sheets can't
    // say so: a spin box puts its text field at its left edge, whatever its padding.
    class SpinBoxInset : public QObject {
    protected:
        bool eventFilter(QObject* watched, QEvent* event) override {
            if (event->type() == QEvent::Polish
                && (qobject_cast<QSpinBox*>(watched) || qobject_cast<QDoubleSpinBox*>(watched))) {
                if (auto* edit = watched->findChild<QLineEdit*>()) edit->setTextMargins(FieldInset, 0, 0, 0);
            }
            return false;
        }
    };

public:
    static constexpr int FieldInset = 8;             // From a field's edge to its text, as the style sheet's padding

private:

    static void applyLightTheme() {
        darkActive = false;
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QPalette lightPalette;
        lightPalette.setColor(QPalette::AlternateBase, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::BrightText, QColor(100, 100, 100));
        lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::Button, QColor(240, 240, 240));
        lightPalette.setColor(QPalette::ButtonText, Qt::black);
        lightPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::HighlightedText, Qt::white);
        lightPalette.setColor(QPalette::Light, QColor(255, 255, 255));
        lightPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        lightPalette.setColor(QPalette::Text, Qt::black);
        lightPalette.setColor(QPalette::Window, pageBackground(false));
        lightPalette.setColor(QPalette::WindowText, Qt::black);
        lightPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));   // Not Qt's yellow
        lightPalette.setColor(QPalette::ToolTipText, QColor(30, 30, 30));
        // The colours above are for every state, so disabled text would look enabled
        for (QPalette::ColorRole role : {QPalette::Text, QPalette::WindowText, QPalette::ButtonText}) {
            lightPalette.setColor(QPalette::Disabled, role, QColor(160, 160, 164));
        }

        qApp->setPalette(lightPalette);
        styleToolTips(QColor(255, 255, 255), QColor(30, 30, 30), QColor(0, 0, 0, 45));
    }

    static void applySystemTheme() {
        if (QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark) {
            applyDarkTheme();
        } else {
            applyLightTheme();
        }
    }
};

#endif // THEME_H
