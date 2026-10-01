#include "settings.hpp"

#include <QApplication>
#include <QLocale>
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

QStringList inEveryLanguage(const char* context, const QString& source) {
    static QTranslator spanish;
    static const bool loaded = spanish.load(QLocale(QLocale::Spanish, QLocale::Spain), "calculate", "_", ":/i18n");
    QStringList texts{source};
    if (loaded) {
        const QString text = spanish.translate(context, source.toUtf8().constData());
        if (!text.isEmpty()) texts << text;
    }
    return texts;
}

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
    // Ask the platform for the same scheme, so window frames and native dialogs follow where it can.
    if (theme == Theme::System) QGuiApplication::styleHints()->unsetColorScheme();
    else QGuiApplication::styleHints()->setColorScheme(theme == Theme::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light);
    const bool dark = theme == Theme::Dark || (theme == Theme::System && systemIsDark());
    QApplication::setPalette(palette(dark));
}

// Both palettes are ours: a style's standard palette may follow the desktop (Fusion under a dark KDE
// theme gives a dark one), and "Light" must be light everywhere.
QPalette palette(bool dark) {
    struct Colours {
        QColor window, base, text, button, placeholder, mid, dimmed;
    };
    const Colours c = dark ? Colours{{0x2b, 0x2d, 0x30}, {0x1e, 0x1f, 0x22}, {0xe8, 0xe8, 0xe8}, {0x3a, 0x3c, 0x40},
                                     {0x8c, 0x8f, 0x94}, {0x55, 0x57, 0x5c}, {0x7a, 0x7c, 0x80}}
                           : Colours{{0xef, 0xef, 0xef}, {0xff, 0xff, 0xff}, {0x1e, 0x1e, 0x1e}, {0xe4, 0xe4, 0xe4},
                                     {0x7a, 0x7a, 0x7a}, {0xb8, 0xb8, 0xb8}, {0x9a, 0x9a, 0x9a}};
    const QColor highlight(0x3d, 0x8e, 0xd6);
    QPalette p;
    p.setColor(QPalette::Window, c.window);
    p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.base);
    p.setColor(QPalette::AlternateBase, c.window);
    p.setColor(QPalette::ToolTipBase, c.base);
    p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::PlaceholderText, c.placeholder);
    p.setColor(QPalette::Text, c.text);
    p.setColor(QPalette::Button, c.button);
    p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::BrightText, Qt::white);
    p.setColor(QPalette::Light, c.button.lighter(130));
    p.setColor(QPalette::Midlight, c.button.lighter(115));
    p.setColor(QPalette::Mid, c.mid);
    p.setColor(QPalette::Dark, dark ? QColor(0x18, 0x19, 0x1b) : c.mid.darker(130));
    p.setColor(QPalette::Shadow, dark ? QColor(Qt::black) : QColor(0x70, 0x70, 0x70));
    p.setColor(QPalette::Highlight, highlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::Link, highlight);
    for (QPalette::ColorRole role : {QPalette::Text, QPalette::ButtonText, QPalette::WindowText})
        p.setColor(QPalette::Disabled, role, c.dimmed);
    return p;
}

}  // namespace settings
