#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QColor>
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

    static bool isDark() {
        return darkActive;
    }

    static Colors colors() {
        if (darkActive) {
            return {
                QColor(48, 48, 48),          // sidebarBackground
                QColor(255, 255, 255, 20),   // sidebarBorder
                QColor(255, 255, 255, 18),   // itemHover
                QColor(255, 255, 255, 34),   // itemSelected
                QColor(240, 240, 240),       // textPrimary
                QColor(196, 196, 196),       // textSecondary
                QColor(140, 140, 140),       // textMuted
                QColor(42, 130, 218),        // accent
                Qt::white,                   // onAccent
                QColor(229, 83, 75),         // danger
                QColor(255, 255, 255, 30)    // track
            };
        }
        return {
            QColor(233, 233, 236),           // sidebarBackground
            QColor(0, 0, 0, 20),             // sidebarBorder
            QColor(0, 0, 0, 13),             // itemHover
            QColor(0, 0, 0, 24),             // itemSelected
            QColor(30, 30, 30),              // textPrimary
            QColor(74, 74, 74),              // textSecondary
            QColor(128, 128, 128),           // textMuted
            QColor(42, 130, 218),            // accent
            Qt::white,                       // onAccent
            QColor(201, 55, 44),             // danger
            QColor(0, 0, 0, 26)              // track
        };
    }

private:
    static inline bool darkActive = true;

    static void applyDarkTheme() {
        darkActive = true;
        QApplication::setStyle(QStyleFactory::create("Fusion"));
        QPalette darkPalette;
        darkPalette.setColor(QPalette::AlternateBase, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::BrightText, QColor(100, 100, 100));
        darkPalette.setColor(QPalette::Base, QColor(70, 70, 70));
        darkPalette.setColor(QPalette::Button, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::ButtonText, Qt::white);
        darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::HighlightedText, Qt::white);
        darkPalette.setColor(QPalette::Light, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
        darkPalette.setColor(QPalette::Text, Qt::white);
        darkPalette.setColor(QPalette::Window, QColor(60, 60, 60));
        darkPalette.setColor(QPalette::WindowText, Qt::white);

        qApp->setPalette(darkPalette);
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
        lightPalette.setColor(QPalette::Window, QColor(245, 245, 245));
        lightPalette.setColor(QPalette::WindowText, Qt::black);

        qApp->setPalette(lightPalette);
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
