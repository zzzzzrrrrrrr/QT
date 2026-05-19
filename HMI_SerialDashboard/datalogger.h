#ifndef DATALOGGER_H
#define DATALOGGER_H

#include <QFile>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief DataLogger writes processed HMI samples to timestamped CSV files.
 */
class DataLogger : public QObject
{
    Q_OBJECT

public:
    explicit DataLogger(QObject *parent = nullptr);
    ~DataLogger() override;

    void setOutputDirectory(const QString &directoryPath);
    QString outputDirectory() const;
    QString currentFilePath() const;
    bool isRecording() const;

public slots:
    bool startSession();
    void stopSession();
    bool logSample(const QVector<double> &values, bool alarmActive, const QString &alarmMessage);

signals:
    void logMessage(const QString &message);
    void errorOccurred(const QString &message);

private:
    static QString csvEscape(const QString &text);
    QString createSessionFilePath() const;
    bool writeHeader();

    QString m_outputDirectory;
    QString m_currentFilePath;
    QFile m_file;
};

#endif // DATALOGGER_H
