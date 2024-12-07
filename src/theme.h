#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QApplication>
#include <QStyleFactory>
#include <QPalette>

class Theme {
public:
    enum class Type {
        Dark,
        Light,
        System
    };

    // Sets the theme for the application
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

private:
    static void applyDarkTheme() {
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
        qApp->setPalette(QApplication::style()->standardPalette());
        qApp->setStyleSheet("");
    }
};

#endif // THEME_H
