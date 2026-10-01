#include "settings.hpp"

#include <QApplication>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleHints>

#include <gtest/gtest.h>

namespace {

// A style whose standard palette follows a dark desktop, as Fusion does under KDE's dark theme.
class DarkDesktopStyle : public QProxyStyle {
public:
    DarkDesktopStyle() : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion"))) {}
    QPalette standardPalette() const override { return settings::palette(true); }
};

}  // namespace

TEST(Settings, LightIsLightEvenOnADarkDesktop) {
    const QString previousStyle = QApplication::style()->name();
    const QPalette previousPalette = QApplication::palette();
    QApplication::setStyle(new DarkDesktopStyle);
    settings::setTheme(settings::Theme::Light);
    const QPalette p = QApplication::palette();
    EXPECT_GT(p.color(QPalette::Window).lightness(), 200);
    EXPECT_GT(p.color(QPalette::Base).lightness(), 200);
    EXPECT_LT(p.color(QPalette::Text).lightness(), 60);
    EXPECT_LT(p.color(QPalette::ButtonText).lightness(), 60);
    settings::setTheme(settings::Theme::Dark);
    EXPECT_LT(QApplication::palette().color(QPalette::Window).lightness(), 80);
    QApplication::setStyle(QStyleFactory::create(previousStyle));
    QApplication::setPalette(previousPalette);
    QGuiApplication::styleHints()->unsetColorScheme();
}
