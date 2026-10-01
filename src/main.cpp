#include "mainwindow.hpp"

#include <QApplication>
#include <QLocale>
#include <QScreen>
#include <QTranslator>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTranslator translator;
    if (translator.load(QLocale(), "calculate", "_", ":/i18n")) app.installTranslator(&translator);
    MainWindow window;
    const QRect screen = window.screen()->availableGeometry();
    window.resize(screen.width() / 2, screen.height());  // the size the keys are designed for
    window.show();
    return app.exec();
}
