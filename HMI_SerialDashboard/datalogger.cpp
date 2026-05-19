#include "datalogger.h"

#include <QDateTime>
#include <QDir>
#include <QTextStream>

DataLogger::DataLogger(QObject *parent)
    : QObject(parent)
{
}

DataLogger::~DataLogger()
{
    stopSession();
}

void DataLogger::setOutputDirectory(const QString &directoryPath)
{
    m_outputDirectory = QDir::cleanPath(directoryPath);
}

QString DataLogger::outputDirectory() const
{
    return m_outputDirectory;
}

QString DataLogger::currentFilePath() const
{
    return m_currentFilePath;
}

bool DataLogger::isRecording() const
{
    return m_file.isOpen();
}

bool DataLogger::startSession()
{
    stopSession();

    if (m_outputDirectory.isEmpty()) {
        emit errorOccurred(QStringLiteral("Data log directory is empty."));
        return false;
    }

    QDir dir;
    if (!dir.mkpath(m_outputDirectory)) {
        emit errorOccurred(QStringLiteral("Unable to create data log directory: %1").arg(m_outputDirectory));
        return false;
    }

    m_currentFilePath = createSessionFilePath();
    m_file.setFileName(m_currentFilePath);

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        emit errorOccurred(QStringLiteral("Unable to open data log file: %1").arg(m_currentFilePath));
        return false;
    }

    if (!writeHeader()) {
        stopSession();
        return false;
    }

    emit logMessage(QStringLiteral("Data logging started: %1").arg(m_currentFilePath));
    return true;
}

void DataLogger::stopSession()
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
        emit logMessage(QStringLiteral("Data logging stopped"));
    }
}

bool DataLogger::logSample(const QVector<double> &values,
                           bool alarmActive,
                           const QString &alarmMessage)
{
    if (!m_file.isOpen()) {
        return false;
    }

    const auto valueText = [&values](int index) {
        return index < values.size() ? QString::number(values.at(index), 'f', 3)
                                     : QString {};
    };

    QTextStream stream(&m_file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODateWithMs) << ','
           << valueText(0) << ','
           << valueText(1) << ','
           << valueText(2) << ','
           << (alarmActive ? QStringLiteral("1") : QStringLiteral("0")) << ','
           << csvEscape(alarmMessage) << '\n';
    m_file.flush();
    return true;
}

QString DataLogger::csvEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace('"', QStringLiteral("\"\""));

    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
        escaped.prepend('"');
        escaped.append('"');
    }

    return escaped;
}

QString DataLogger::createSessionFilePath() const
{
    const QString fileName = QStringLiteral("hmi_samples_%1.csv")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    return QDir(m_outputDirectory).filePath(fileName);
}

bool DataLogger::writeHeader()
{
    QTextStream stream(&m_file);
    stream << "timestamp,temperature,pressure,flow,alarm_active,alarm_message\n";
    return stream.status() == QTextStream::Ok;
}
