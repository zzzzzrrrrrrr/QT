#include "datalogger.h"

#include <QDateTime>
#include <QDir>
#include <QTextStream>
#include <QtGlobal>

DataLogger::DataLogger(QObject *parent)
    : QObject(parent)
{
    m_flushTimer.setInterval(m_flushIntervalMs);
    connect(&m_flushTimer, &QTimer::timeout,
            this, &DataLogger::flush);
}

DataLogger::~DataLogger()
{
    stopSession();
}

void DataLogger::setOutputDirectory(const QString &directoryPath)
{
    m_outputDirectory = QDir::cleanPath(directoryPath);
}

void DataLogger::setFlushIntervalMs(int intervalMs)
{
    m_flushIntervalMs = qMax(100, intervalMs);
    m_flushTimer.setInterval(m_flushIntervalMs);
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
    m_reportedWriteError = false;

    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::NewOnly)) {
        emit errorOccurred(QStringLiteral("Unable to open data log file: %1").arg(m_currentFilePath));
        return false;
    }

    if (!writeHeader()) {
        stopSession();
        return false;
    }

    emit logMessage(QStringLiteral("Data logging started: %1").arg(m_currentFilePath));
    m_flushTimer.start(m_flushIntervalMs);
    return true;
}

void DataLogger::stopSession()
{
    if (m_file.isOpen()) {
        m_file.flush();
        m_file.close();
        emit logMessage(QStringLiteral("Data logging stopped"));
    }
    m_flushTimer.stop();
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
    const bool ok = stream.status() == QTextStream::Ok;
    if (!ok && !m_reportedWriteError) {
        m_reportedWriteError = true;
        emit errorOccurred(QStringLiteral("Data log write failed: %1").arg(m_currentFilePath));
    }
    return ok;
}

void DataLogger::flush()
{
    if (m_file.isOpen()) {
        m_file.flush();
    }
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
    const QDir directory(m_outputDirectory);
    const QString baseName = QStringLiteral("hmi_samples_%1")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss_zzz")));

    QString candidate = directory.filePath(baseName + QStringLiteral(".csv"));
    int suffix = 1;
    while (QFile::exists(candidate)) {
        candidate = directory.filePath(QStringLiteral("%1_%2.csv").arg(baseName).arg(suffix++));
    }

    return candidate;
}

bool DataLogger::writeHeader()
{
    QTextStream stream(&m_file);
    stream << "timestamp,temperature,pressure,flow,alarm_active,alarm_message\n";
    return stream.status() == QTextStream::Ok;
}
