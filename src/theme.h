#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QColor>
#include <QAbstractButton>
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

    // Gives a checkable button the accent colour while it is checked (Fusion's pressed look is too subtle)
    static void highlightWhenChecked(QAbstractButton* button) {
        auto update = [button](bool checked) {
            if (checked) {
                QPalette palette = button->palette();
                palette.setColor(QPalette::Button, colors().accent);
                palette.setColor(QPalette::ButtonText, colors().onAccent);
                button->setPalette(palette);
            } else {
                button->setPalette(QPalette());      // Back to the inherited application palette
            }
        };
        QObject::connect(button, &QAbstractButton::toggled, button, update);
        update(button->isChecked());
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
                QColor(255, 255, 255, 38)    // track
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
            QColor(0, 0, 0, 26)              // track
        };
    }

    // The window background of either theme
    static QColor pageBackground(bool dark) {
        return dark ? QColor(38, 38, 42) : QColor(245, 245, 245);
    }

    static constexpr int controlHeight = 32;        // Fields, drop-downs, steppers and segments in a row

    // The app's own look for controls, in place of Fusion's square boxes: one height, rounded
    // corners, hairline borders and quiet hovers. Set on a container, it styles what's inside:
    //   QLineEdit and QComboBox            fields
    //   QToolButton[chip="true"]           a toggle; accent tint while checked
    //   QFrame[segmented="true"]           a track whose QToolButtons are its segments
    //   QFrame[stepper="true"]             "‹ name ›" in one box; the QToolButton[stepperName="true"] between dividers
    //   QFrame[popup="true"]               a rounded popup; QAbstractButton[thisMonth="true"] is outlined
    // Set it again when the theme changes, as its colours are the active theme's.
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
        const QString border = rgba(dark ? QColor(255, 255, 255, 34) : QColor(0, 0, 0, 38));
        const QString borderHover = rgba(dark ? QColor(255, 255, 255, 70) : QColor(0, 0, 0, 80));
        const QString divider = rgba(dark ? QColor(255, 255, 255, 22) : QColor(0, 0, 0, 22));
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

        QString sheet = R"(
QLineEdit, QComboBox { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 0 8px; }
QLineEdit:hover, QComboBox:hover { border-color: BORDERHOVER; }
QLineEdit:focus, QComboBox:focus, QComboBox:on { border-color: ACCENT; }
QComboBox::drop-down { border: none; width: 26px; }
QComboBox::down-arrow { image: url(ARROW); width: 14px; height: 14px; }

QToolButton[chip="true"] { background-color: FIELD; color: TEXT; border: 1px solid BORDER; border-radius: 8px; padding: 0 10px; }
QToolButton[chip="true"]:hover { border-color: BORDERHOVER; }
QToolButton[chip="true"]:checked { background-color: ACCENTTINT; color: ACCENTTEXT; border-color: ACCENT; }

QFrame[segmented="true"] { background-color: TRACK; border: 1px solid BORDER; border-radius: 8px; }
QFrame[segmented="true"] QToolButton { background: transparent; color: SECONDARY; border: 1px solid transparent; border-radius: 6px; padding: 0 11px; }
QFrame[segmented="true"] QToolButton:hover { color: TEXT; background-color: HOVER; }
QFrame[segmented="true"] QToolButton:checked { background-color: RAISED; color: TEXT; border-color: BORDER; }

QFrame[stepper="true"] { background-color: FIELD; border: 1px solid BORDER; border-radius: 8px; }
QFrame[stepper="true"]:hover { border-color: BORDERHOVER; }
QFrame[stepper="true"] QToolButton { background: transparent; color: TEXT; border: none; border-radius: 6px; padding: 0 6px; }
QFrame[stepper="true"] QToolButton:hover { background-color: HOVER; }
QFrame[stepper="true"] QToolButton[stepperName="true"] { border-radius: 0; border-left: 1px solid DIVIDER; border-right: 1px solid DIVIDER; padding: 0 10px; }

QFrame[popup="true"] { background-color: POPUP; border: 1px solid BORDER; border-radius: 10px; }
QFrame[popup="true"] QAbstractButton { background: transparent; color: TEXT; border: 1px solid transparent; border-radius: 6px; padding: 5px 10px; text-align: left; }
QFrame[popup="true"] QAbstractButton:hover { background-color: HOVER; }
QFrame[popup="true"] QAbstractButton:disabled { color: DISABLED; }
QFrame[popup="true"] QAbstractButton:checked { background-color: ACCENTTINT; color: ACCENTTEXT; }
QFrame[popup="true"] QAbstractButton[thisMonth="true"] { border-color: BORDERHOVER; }
QFrame[popup="true"] QFrame#divider { background-color: DIVIDER; border: none; }
QFrame[popup="true"] QLabel { color: TEXT; background: transparent; }
)";
        // Longest names first, so BORDER doesn't replace the start of BORDERHOVER
        const QList<QPair<QString, QString>> values = {
            {"BORDERHOVER", borderHover}, {"ACCENTTINT", accentTint}, {"ACCENTTEXT", accentText},
            {"SECONDARY", secondary}, {"DISABLED", disabled}, {"DIVIDER", divider}, {"BORDER", border},
            {"ACCENT", accent}, {"RAISED", raised}, {"FIELD", field}, {"TRACK", track}, {"HOVER", hover},
            {"POPUP", popup}, {"TEXT", text},
            {"ARROW", arrow},                        // Last: a random folder name could hold a name above
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

    // Fusion draws tooltips with a hard frame in the text colour; a soft one fits the app better
    static void styleToolTips(const QColor& background, const QColor& text, const QColor& border) {
        const QString borderColor = QString("rgba(%1, %2, %3, %4)")
                                        .arg(border.red()).arg(border.green()).arg(border.blue()).arg(border.alpha());
        qApp->setStyleSheet(QString("QToolTip { background-color: %1; color: %2; border: 1px solid %3; padding: 4px 6px; }")
                                .arg(background.name(), text.name(), borderColor));
    }

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
