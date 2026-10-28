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
