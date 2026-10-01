#include "settings.hpp"

#include <QApplication>
#include <QLocale>
#include <QStyle>
#include <QStyleHints>
#include <QTranslator>

namespace settings {

namespace {

QTranslator& translator() {
    static QTranslator instance;
    return instance;
}

Theme& currentTheme() {
    static Theme theme = Theme::System;
    return theme;
}

bool systemIsDark() { return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark; }

}  // namespace

void setLanguage(Language language) {
    QCoreApplication::removeTranslator(&translator());
    const QLocale locale = language == Language::System    ? QLocale::system()
                           : language == Language::Spanish ? QLocale(QLocale::Spanish, QLocale::Spain)
                                                           : QLocale(QLocale::English);
    // English is the source language: no translator. (Removing and installing one tell every window.)
    if (language != Language::English && translator().load(locale, "calculate", "_", ":/i18n"))
        QCoreApplication::installTranslator(&translator());
}

void setTheme(Theme theme) {
    static const bool connected = [] {
        QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, qApp, [] {
            if (currentTheme() == Theme::System) setTheme(Theme::System);
        });
        return true;
    }();
    Q_UNUSED(connected);
    currentTheme() = theme;
    const bool dark = theme == Theme::Dark || (theme == Theme::System && systemIsDark());
    QApplication::setPalette(palette(dark));
}

QPalette palette(bool dark) {
    if (!dark) return QApplication::style()->standardPalette();
    const QColor window(0x2b, 0x2d, 0x30), base(0x1e, 0x1f, 0x22), text(0xe8, 0xe8, 0xe8), button(0x3a, 0x3c, 0x40);
    const QColor highlight(0x3d, 0x8e, 0xd6), dimmed(0x7a, 0x7c, 0x80);
    QPalette p;
    p.setColor(QPalette::Window, window);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, base);
    p.setColor(QPalette::AlternateBase, window);
    p.setColor(QPalette::ToolTipBase, base);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, QColor(0x8c, 0x8f, 0x94));
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, button.lighter(130));
    p.setColor(QPalette::Midlight, button.lighter(115));
    p.setColor(QPalette::Mid, QColor(0x55, 0x57, 0x5c));
    p.setColor(QPalette::Dark, QColor(0x18, 0x19, 0x1b));
    p.setColor(QPalette::Shadow, Qt::black);
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, highlight);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        p.setColor(QPalette::Disabled, role, dimmed);
    return p;
}

}  // namespace settings
