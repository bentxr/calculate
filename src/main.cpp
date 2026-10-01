#include "mainwindow.hpp"
#include "settings.hpp"

#include <QApplication>
#include <QScreen>
#include <QStyleFactory>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));  // the same look on every desktop
    settings::setLanguage(settings::Language::System);
    settings::setTheme(settings::startTheme);
    MainWindow window;
    const QRect screen = window.screen()->availableGeometry();
    window.resize(screen.width() / 2, screen.height());  // the size the keys are designed for
    window.show();
    return app.exec();
}
