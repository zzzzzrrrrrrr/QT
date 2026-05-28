#include "alarmmanager.h"

#include <QTextStream>
#include <QtGlobal>

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

void AlarmManager::setTemperatureLowLimit(double limit)
{
    m_temperatureLowLimit = limit;
}

void AlarmManager::setPressureLowLimit(double limit)
{
    m_pressureLowLimit = limit;
}

void AlarmManager::setFlowLowLimit(double limit)
{
    m_flowLowLimit = limit;
}

void AlarmManager::setHysteresis(double hysteresis)
{
    m_hysteresis = qMax(0.0, hysteresis);
}

void AlarmManager::setLatchingEnabled(bool enabled)
{
    m_latchingEnabled = enabled;
}

void AlarmManager::setMaxHistoryRecords(int maxRecords)
{
    m_maxHistoryRecords = qBound(10, maxRecords, 100000);
    while (m_history.size() > m_maxHistoryRecords) {
        m_history.removeFirst();
    }
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

double AlarmManager::temperatureLowLimit() const
{
    return m_temperatureLowLimit;
}

double AlarmManager::pressureLowLimit() const
{
    return m_pressureLowLimit;
}

double AlarmManager::flowLowLimit() const
{
    return m_flowLowLimit;
}

double AlarmManager::hysteresis() const
{
    return m_hysteresis;
}

bool AlarmManager::isLatchingEnabled() const
{
    return m_latchingEnabled;
}

AlarmManager::AlarmState AlarmManager::evaluate(const QVector<double> &values)
{
    const QVector<AlarmEvaluation> physicalEvaluations = evaluateRules(values);
    m_lastPhysicalAlarmActive = !physicalEvaluations.isEmpty();
    if (m_lastPhysicalAlarmActive) {
        m_currentAlarmKeys.clear();
        for (const AlarmEvaluation &evaluation : physicalEvaluations) {
            m_currentAlarmKeys.append(evaluation.key);
        }
    }

    AlarmState nextState = makeState(physicalEvaluations);

    if (!nextState.active
        && m_latchingEnabled
        && m_currentState.active
        && !m_currentState.acknowledged) {
        nextState = m_currentState;
        nextState.latched = true;
        nextState.silenced = m_silenced;
        nextState.message = QStringLiteral("Latched: %1").arg(nextState.messages.join(QStringLiteral("; ")));
    }

    updateCurrentState(nextState);
    return m_currentState;
}

AlarmManager::AlarmState AlarmManager::currentState() const
{
    return m_currentState;
}

QVector<AlarmManager::AlarmRecord> AlarmManager::history() const
{
    return m_history;
}

QString AlarmManager::historyAsCsv() const
{
    QString csv;
    QTextStream stream(&csv);
    stream << "timestamp,active,acknowledged,silenced,latched,severity,channel,value,limit,message\n";

    for (const AlarmRecord &record : m_history) {
        stream << record.timestamp.toString(Qt::ISODateWithMs) << ','
               << (record.active ? "1" : "0") << ','
               << (record.acknowledged ? "1" : "0") << ','
               << (record.silenced ? "1" : "0") << ','
               << (record.latched ? "1" : "0") << ','
               << record.severity << ','
               << csvEscape(record.channel) << ','
               << QString::number(record.value, 'f', 3) << ','
               << QString::number(record.limit, 'f', 3) << ','
               << csvEscape(record.message) << '\n';
    }

    return csv;
}

bool AlarmManager::isSilenced() const
{
    return m_silenced;
}

void AlarmManager::acknowledgeCurrentAlarm()
{
    if (!m_currentState.active || m_currentState.acknowledged) {
        return;
    }

    m_currentState.acknowledged = true;
    appendHistoryRecord(m_currentState);
    emit alarmAcknowledged(m_currentState.message);
    emit alarmHistoryChanged();

    if (m_currentState.latched && !m_lastPhysicalAlarmActive) {
        updateCurrentState(makeNormalState());
    }
}

void AlarmManager::setSilenced(bool silenced)
{
    if (m_silenced == silenced) {
        return;
    }

    m_silenced = silenced;
    m_currentState.silenced = m_currentState.active && m_silenced;

    if (m_currentState.active) {
        appendHistoryRecord(m_currentState);
    }

    emit alarmSilenced(m_silenced);
    emit alarmHistoryChanged();
}

void AlarmManager::clearHistory()
{
    if (m_history.isEmpty()) {
        return;
    }

    m_history.clear();
    emit alarmHistoryChanged();
}

AlarmManager::AlarmState AlarmManager::makeNormalState() const
{
    AlarmState state;
    state.active = false;
    state.message = QStringLiteral("Normal");
    state.messages = { state.message };
    return state;
}

AlarmManager::AlarmState AlarmManager::makeState(const QVector<AlarmEvaluation> &evaluations) const
{
    if (evaluations.isEmpty()) {
        return makeNormalState();
    }

    AlarmState state;
    state.active = true;
    state.severity = 0;

    QStringList messages;
    for (const AlarmEvaluation &evaluation : evaluations) {
        messages.append(evaluation.message);
        if (evaluation.severity > state.severity) {
            state.severity = evaluation.severity;
            state.channel = evaluation.channel;
            state.value = evaluation.value;
            state.limit = evaluation.limit;
        }
    }

    state.messages = messages;
    state.message = messages.join(QStringLiteral("; "));
    return state;
}

void AlarmManager::updateCurrentState(const AlarmState &state)
{
    AlarmState nextState = state;
    const bool sameActiveAlarm = nextState.active
                                 && m_currentState.active
                                 && nextState.messages == m_currentState.messages
                                 && nextState.latched == m_currentState.latched;

    if (!nextState.active) {
        m_silenced = false;
        nextState.silenced = false;
        nextState.acknowledged = false;
        nextState.latched = false;
        m_currentAlarmKeys.clear();
    } else {
        nextState.silenced = m_silenced;
        nextState.acknowledged = sameActiveAlarm ? m_currentState.acknowledged : false;
    }

    const bool changed = nextState.active != m_currentState.active
                         || nextState.message != m_currentState.message
                         || nextState.latched != m_currentState.latched;

    m_currentState = nextState;

    if (changed) {
        appendHistoryRecord(m_currentState);
        emit alarmStateChanged(m_currentState.active, m_currentState.message);
        emit alarmHistoryChanged();
    }
}

void AlarmManager::appendHistoryRecord(const AlarmState &state)
{
    AlarmRecord record;
    record.timestamp = QDateTime::currentDateTime();
    record.active = state.active;
    record.acknowledged = state.acknowledged;
    record.silenced = state.silenced;
    record.latched = state.latched;
    record.severity = state.severity;
    record.channel = state.channel;
    record.value = state.value;
    record.limit = state.limit;
    record.message = state.message;

    m_history.append(record);

    while (m_history.size() > m_maxHistoryRecords) {
        m_history.removeFirst();
    }
}

QVector<AlarmManager::AlarmEvaluation> AlarmManager::evaluateRules(const QVector<double> &values) const
{
    QVector<AlarmEvaluation> evaluations;

    if (values.size() > 0) {
        appendHighEvaluation(&evaluations,
                             QStringLiteral("Temperature"),
                             QStringLiteral("temperature.high"),
                             values.at(0),
                             m_temperatureHighLimit);
        appendLowEvaluation(&evaluations,
                            QStringLiteral("Temperature"),
                            QStringLiteral("temperature.low"),
                            values.at(0),
                            m_temperatureLowLimit);
    }

    if (values.size() > 1) {
        appendHighEvaluation(&evaluations,
                             QStringLiteral("Pressure"),
                             QStringLiteral("pressure.high"),
                             values.at(1),
                             m_pressureHighLimit);
        appendLowEvaluation(&evaluations,
                            QStringLiteral("Pressure"),
                            QStringLiteral("pressure.low"),
                            values.at(1),
                            m_pressureLowLimit);
    }

    if (values.size() > 2) {
        appendHighEvaluation(&evaluations,
                             QStringLiteral("Flow"),
                             QStringLiteral("flow.high"),
                             values.at(2),
                             m_flowHighLimit);
        appendLowEvaluation(&evaluations,
                            QStringLiteral("Flow"),
                            QStringLiteral("flow.low"),
                            values.at(2),
                            m_flowLowLimit);
    }

    return evaluations;
}

void AlarmManager::appendHighEvaluation(QVector<AlarmEvaluation> *evaluations,
                                        const QString &channel,
                                        const QString &key,
                                        double value,
                                        double limit) const
{
    const bool wasActive = m_currentAlarmKeys.contains(key);
    const double triggerLimit = wasActive ? limit - m_hysteresis : limit;
    if (value <= triggerLimit) {
        return;
    }

    AlarmEvaluation evaluation;
    evaluation.key = key;
    evaluation.channel = channel;
    evaluation.value = value;
    evaluation.limit = limit;
    evaluation.severity = 2;
    evaluation.message = QStringLiteral("%1 high: %2 > %3")
                             .arg(channel)
                             .arg(value, 0, 'f', 3)
                             .arg(limit, 0, 'f', 3);
    evaluations->append(evaluation);
}

void AlarmManager::appendLowEvaluation(QVector<AlarmEvaluation> *evaluations,
                                       const QString &channel,
                                       const QString &key,
                                       double value,
                                       double limit) const
{
    const bool wasActive = m_currentAlarmKeys.contains(key);
    const double triggerLimit = wasActive ? limit + m_hysteresis : limit;
    if (value >= triggerLimit) {
        return;
    }

    AlarmEvaluation evaluation;
    evaluation.key = key;
    evaluation.channel = channel;
    evaluation.value = value;
    evaluation.limit = limit;
    evaluation.severity = 2;
    evaluation.message = QStringLiteral("%1 low: %2 < %3")
                             .arg(channel)
                             .arg(value, 0, 'f', 3)
                             .arg(limit, 0, 'f', 3);
    evaluations->append(evaluation);
}

QString AlarmManager::csvEscape(const QString &text)
{
    QString escaped = text;
    escaped.replace('"', QStringLiteral("\"\""));

    if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
        escaped.prepend('"');
        escaped.append('"');
    }

    return escaped;
}
