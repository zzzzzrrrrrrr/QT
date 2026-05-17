#include "dataprocessor.h"

#include <QDebug>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>

DataProcessor::DataProcessor(QObject *parent)
    : QObject(parent)
{
}

QVector<double> DataProcessor::getLatestData() const
{
    QMutexLocker locker(&m_mutex);
    return m_latestData;
}

void DataProcessor::processRawData(const QString &rawData)
{
    if (rawData.trimmed().isEmpty()) {
        qDebug().noquote() << "[Step2][DataProcessor] ignored empty raw data";
        return;
    }

    // Step 2 verification: raw QString arrives from SerialManager.
    qDebug().noquote() << "[Step2][DataProcessor] raw data =" << rawData;

    const QVector<double> values = parseValues(rawData);
    if (values.isEmpty()) {
        qDebug().noquote() << "[Step2][DataProcessor] no numeric values parsed";
        return;
    }

    {
        QMutexLocker locker(&m_mutex);
        m_latestData = values;
    }

    qDebug().noquote() << "[Step2][DataProcessor] emit dataUpdated(QVector<double>) =" << values;
    emit dataUpdated(values);
}

QVector<double> DataProcessor::parseValues(const QString &rawData) const
{
    static const QRegularExpression numberPattern(
        QStringLiteral(R"([-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?)"));

    QVector<double> values;
    QRegularExpressionMatchIterator matches = numberPattern.globalMatch(rawData);

    while (matches.hasNext()) {
        bool ok = false;
        const double value = matches.next().captured(0).toDouble(&ok);
        if (ok) {
            values.append(value);
        }
    }

    return values;
}
