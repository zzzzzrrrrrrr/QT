#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

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

signals:
    void alarmStateChanged(bool active, const QString &message);

private:
    AlarmState makeNormalState() const;
    AlarmState makeHighState(const QString &channel, double value, double limit) const;
    void updateCurrentState(const AlarmState &state);

    double m_temperatureHighLimit = 32.0;
    double m_pressureHighLimit = 108.0;
    double m_flowHighLimit = 70.0;
    AlarmState m_currentState;
};

#endif // ALARMMANAGER_H
