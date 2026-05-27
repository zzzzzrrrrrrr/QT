#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

/**
 * @brief AlarmManager evaluates processed HMI values against alarm limits.
 */
class AlarmManager : public QObject
{
    Q_OBJECT

public:
    struct AlarmState {
        bool active = false;
        QString message = QStringLiteral("Normal");
        QString channel;
        double value = 0.0;
        double limit = 0.0;
        bool acknowledged = false;
        bool silenced = false;
    };

    struct AlarmRecord {
        QDateTime timestamp;
        bool active = false;
        bool acknowledged = false;
        bool silenced = false;
        QString message;
    };

    explicit AlarmManager(QObject *parent = nullptr);

    void setTemperatureHighLimit(double limit);
    void setPressureHighLimit(double limit);
    void setFlowHighLimit(double limit);

    double temperatureHighLimit() const;
    double pressureHighLimit() const;
    double flowHighLimit() const;

    AlarmState evaluate(const QVector<double> &values);
    AlarmState currentState() const;
    QVector<AlarmRecord> history() const;
    bool isSilenced() const;

public slots:
    void acknowledgeCurrentAlarm();
    void setSilenced(bool silenced);
    void clearHistory();

signals:
    void alarmStateChanged(bool active, const QString &message);
    void alarmAcknowledged(const QString &message);
    void alarmSilenced(bool silenced);
    void alarmHistoryChanged();

private:
    AlarmState makeNormalState() const;
    AlarmState makeHighState(const QString &channel, double value, double limit) const;
    void updateCurrentState(const AlarmState &state);
    void appendHistoryRecord(const AlarmState &state);

    double m_temperatureHighLimit = 32.0;
    double m_pressureHighLimit = 108.0;
    double m_flowHighLimit = 70.0;
    bool m_silenced = false;
    AlarmState m_currentState;
    QVector<AlarmRecord> m_history;
    int m_maxHistoryRecords = 500;
};

#endif // ALARMMANAGER_H
