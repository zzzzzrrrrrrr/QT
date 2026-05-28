#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
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
        QStringList messages;
        QString channel;
        double value = 0.0;
        double limit = 0.0;
        int severity = 0;
        bool acknowledged = false;
        bool silenced = false;
        bool latched = false;
    };

    struct AlarmRecord {
        QDateTime timestamp;
        bool active = false;
        bool acknowledged = false;
        bool silenced = false;
        bool latched = false;
        int severity = 0;
        QString channel;
        double value = 0.0;
        double limit = 0.0;
        QString message;
    };

    explicit AlarmManager(QObject *parent = nullptr);

    void setTemperatureHighLimit(double limit);
    void setPressureHighLimit(double limit);
    void setFlowHighLimit(double limit);
    void setTemperatureLowLimit(double limit);
    void setPressureLowLimit(double limit);
    void setFlowLowLimit(double limit);
    void setHysteresis(double hysteresis);
    void setLatchingEnabled(bool enabled);
    void setMaxHistoryRecords(int maxRecords);

    double temperatureHighLimit() const;
    double pressureHighLimit() const;
    double flowHighLimit() const;
    double temperatureLowLimit() const;
    double pressureLowLimit() const;
    double flowLowLimit() const;
    double hysteresis() const;
    bool isLatchingEnabled() const;

    AlarmState evaluate(const QVector<double> &values);
    AlarmState currentState() const;
    QVector<AlarmRecord> history() const;
    QString historyAsCsv() const;
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
    struct AlarmEvaluation {
        QString key;
        QString channel;
        QString message;
        double value = 0.0;
        double limit = 0.0;
        int severity = 0;
    };

    AlarmState makeNormalState() const;
    AlarmState makeState(const QVector<AlarmEvaluation> &evaluations) const;
    void updateCurrentState(const AlarmState &state);
    void appendHistoryRecord(const AlarmState &state);
    QVector<AlarmEvaluation> evaluateRules(const QVector<double> &values) const;
    void appendHighEvaluation(QVector<AlarmEvaluation> *evaluations,
                              const QString &channel,
                              const QString &key,
                              double value,
                              double limit) const;
    void appendLowEvaluation(QVector<AlarmEvaluation> *evaluations,
                             const QString &channel,
                             const QString &key,
                             double value,
                             double limit) const;
    static QString csvEscape(const QString &text);

    double m_temperatureHighLimit = 32.0;
    double m_pressureHighLimit = 108.0;
    double m_flowHighLimit = 70.0;
    double m_temperatureLowLimit = -20.0;
    double m_pressureLowLimit = 0.0;
    double m_flowLowLimit = 0.0;
    double m_hysteresis = 0.5;
    bool m_latchingEnabled = false;
    QStringList m_currentAlarmKeys;
    bool m_lastPhysicalAlarmActive = false;
    bool m_silenced = false;
    AlarmState m_currentState;
    QVector<AlarmRecord> m_history;
    int m_maxHistoryRecords = 500;
};

#endif // ALARMMANAGER_H
