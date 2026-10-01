#include "mainwindow.hpp"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    QTranslator translator;
    if (translator.load(QLocale(), "calculate", "_", ":/i18n")) app.installTranslator(&translator);
    MainWindow window;
    window.resize(1000, 700);
    window.show();
    return app.exec();
}
