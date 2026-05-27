#include "alarmmanager.h"
#include "configmanager.h"
#include "datalogger.h"
#include "dataprocessor.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QtGlobal>

#include <cstdlib>
#include <iostream>

namespace {

int fail(const QString &message)
{
    std::cerr << message.toStdString() << '\n';
    return EXIT_FAILURE;
}

bool fuzzyEqual(double left, double right)
{
    return qAbs(left - right) < 0.0001;
}

QString createTestDirectory(const QString &name)
{
    const QString root = QDir::current().filePath(QStringLiteral("build/test_tmp"));
    QDir().mkpath(root);
    const QString path = QDir(root).filePath(QStringLiteral("%1_%2")
                                             .arg(name)
                                             .arg(QCoreApplication::applicationPid()));
    QDir().mkpath(path);
    return path;
}

int testDataProcessor()
{
    DataProcessor processor;
    processor.processRawData(QStringLiteral("temperature=12.5, pressure=-3.25; flow 42"));

    const QVector<double> values = processor.getLatestData();
    if (values.size() != 3) {
        return fail(QStringLiteral("DataProcessor should parse three numbers."));
    }

    if (!fuzzyEqual(values.at(0), 12.5)
        || !fuzzyEqual(values.at(1), -3.25)
        || !fuzzyEqual(values.at(2), 42.0)) {
        return fail(QStringLiteral("DataProcessor parsed unexpected values."));
    }

    return EXIT_SUCCESS;
}

int testAlarmManager()
{
    AlarmManager alarms;
    alarms.setTemperatureHighLimit(30.0);

    AlarmManager::AlarmState state = alarms.evaluate({31.5, 90.0, 10.0});
    if (!state.active || !state.message.contains(QStringLiteral("Temperature"))) {
        return fail(QStringLiteral("AlarmManager should raise temperature high alarm."));
    }

    alarms.acknowledgeCurrentAlarm();
    if (!alarms.currentState().acknowledged) {
        return fail(QStringLiteral("AlarmManager should acknowledge active alarm."));
    }

    alarms.setSilenced(true);
    if (!alarms.currentState().silenced) {
        return fail(QStringLiteral("AlarmManager should silence active alarm."));
    }

    state = alarms.evaluate({25.0, 90.0, 10.0});
    if (state.active || alarms.isSilenced()) {
        return fail(QStringLiteral("AlarmManager should clear alarm and silence state."));
    }

    if (alarms.history().isEmpty()) {
        return fail(QStringLiteral("AlarmManager should keep alarm history."));
    }

    return EXIT_SUCCESS;
}

int testConfigManager()
{
    const QString dirPath = createTestDirectory(QStringLiteral("config"));
    if (dirPath.isEmpty()) {
        return fail(QStringLiteral("Unable to create temporary directory for config test."));
    }

    const QString jsonPath = QDir(dirPath).filePath(QStringLiteral("config.json"));
    ConfigManager config;
    config.setValue(QStringLiteral("io.mode"), QStringLiteral("tcp-sim"));
    config.setValue(QStringLiteral("simulation.intervalMs"), 250);

    if (!config.saveJson(jsonPath)) {
        return fail(QStringLiteral("ConfigManager should save JSON."));
    }

    ConfigManager loaded;
    if (!loaded.loadJson(jsonPath)) {
        return fail(QStringLiteral("ConfigManager should load JSON."));
    }

    if (loaded.getValue(QStringLiteral("io.mode"), QMetaType::QString).toString()
            != QStringLiteral("tcp-sim")
        || loaded.getValue(QStringLiteral("simulation.intervalMs"), QMetaType::Int).toInt() != 250) {
        return fail(QStringLiteral("ConfigManager loaded unexpected values."));
    }

    return EXIT_SUCCESS;
}

int testDataLogger()
{
    const QString dirPath = createTestDirectory(QStringLiteral("logger"));
    if (dirPath.isEmpty()) {
        return fail(QStringLiteral("Unable to create temporary directory for logger test."));
    }

    DataLogger logger;
    logger.setOutputDirectory(dirPath);

    if (!logger.startSession()) {
        return fail(QStringLiteral("DataLogger should start a CSV session."));
    }

    if (!logger.logSample({1.0, 2.0, 3.0}, true, QStringLiteral("Alarm, quoted"))) {
        return fail(QStringLiteral("DataLogger should write one sample."));
    }

    const QString csvPath = logger.currentFilePath();
    logger.stopSession();

    QFile file(csvPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return fail(QStringLiteral("DataLogger output file should exist."));
    }

    const QString content = QString::fromUtf8(file.readAll());
    if (!content.contains(QStringLiteral("timestamp,temperature,pressure,flow"))
        || !content.contains(QStringLiteral("1.000,2.000,3.000,1"))
        || !content.contains(QStringLiteral("\"Alarm, quoted\""))) {
        return fail(QStringLiteral("DataLogger CSV content is unexpected."));
    }

    return EXIT_SUCCESS;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    const int results[] = {
        testDataProcessor(),
        testAlarmManager(),
        testConfigManager(),
        testDataLogger()
    };

    for (int result : results) {
        if (result != EXIT_SUCCESS) {
            return result;
        }
    }

    std::cout << "All HMI unit tests passed.\n";
    return EXIT_SUCCESS;
}
