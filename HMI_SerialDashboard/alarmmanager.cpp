#include "alarmmanager.h"

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
    , m_currentState(makeNormalState())
{
}

void AlarmManager::setTemperatureHighLimit(double limit)
{
    m_temperatureHighLimit = limit;
}

void AlarmManager::setPressureHighLimit(double limit)
{
    m_pressureHighLimit = limit;
}

void AlarmManager::setFlowHighLimit(double limit)
{
    m_flowHighLimit = limit;
}

double AlarmManager::temperatureHighLimit() const
{
    return m_temperatureHighLimit;
}

double AlarmManager::pressureHighLimit() const
{
    return m_pressureHighLimit;
}

double AlarmManager::flowHighLimit() const
{
    return m_flowHighLimit;
}

AlarmManager::AlarmState AlarmManager::evaluate(const QVector<double> &values)
{
    AlarmState nextState = makeNormalState();

    if (values.size() > 0 && values.at(0) > m_temperatureHighLimit) {
        nextState = makeHighState(QStringLiteral("Temperature"),
                                  values.at(0),
                                  m_temperatureHighLimit);
    } else if (values.size() > 1 && values.at(1) > m_pressureHighLimit) {
        nextState = makeHighState(QStringLiteral("Pressure"),
                                  values.at(1),
                                  m_pressureHighLimit);
    } else if (values.size() > 2 && values.at(2) > m_flowHighLimit) {
        nextState = makeHighState(QStringLiteral("Flow"),
                                  values.at(2),
                                  m_flowHighLimit);
    }

    updateCurrentState(nextState);
    return m_currentState;
}

AlarmManager::AlarmState AlarmManager::currentState() const
{
    return m_currentState;
}

AlarmManager::AlarmState AlarmManager::makeNormalState() const
{
    AlarmState state;
    state.active = false;
    state.message = QStringLiteral("Normal");
    return state;
}

AlarmManager::AlarmState AlarmManager::makeHighState(const QString &channel,
                                                     double value,
                                                     double limit) const
{
    AlarmState state;
    state.active = true;
    state.channel = channel;
    state.value = value;
    state.limit = limit;
    state.message = QStringLiteral("%1 high: %2 > %3")
                        .arg(channel)
                        .arg(value, 0, 'f', 3)
                        .arg(limit, 0, 'f', 3);
    return state;
}

void AlarmManager::updateCurrentState(const AlarmState &state)
{
    const bool changed = state.active != m_currentState.active
                         || state.message != m_currentState.message;

    m_currentState = state;

    if (changed) {
        emit alarmStateChanged(m_currentState.active, m_currentState.message);
    }
}
