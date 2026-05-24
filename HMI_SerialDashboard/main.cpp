#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QMetaType>
#include <QTranslator>
#include <QVector>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qRegisterMetaType<QVector<double>>("QVector<double>");

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "HMI_SerialDashboard_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            app.installTranslator(&translator);
            break;
        }
    }

    MainWindow window;
    window.show();

    return app.exec();
}
