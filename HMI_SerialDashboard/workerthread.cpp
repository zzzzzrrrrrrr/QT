#include "workerthread.h"

#include <QDebug>
#include <QMutexLocker>
#include <QReadLocker>
#include <QWriteLocker>
#include <QtGlobal>
#include <algorithm>

namespace {
constexpr bool kVerboseDataTrace = false;
}

WorkerThread::WorkerThread(QObject *parent)
    : QThread(parent)
{
}

WorkerThread::~WorkerThread()
{
    stop();
    wait();
}

void WorkerThread::setProcessor(DataProcessor *processor)
{
    m_processor = processor;
}

void WorkerThread::setIntervalMs(int intervalMs)
{
    m_intervalMs = std::max(20, intervalMs);
}

void WorkerThread::setProcessingMode(ProcessingMode mode)
{
    QWriteLocker locker(&m_processingLock);
    m_processingSettings.mode = mode;
    if (mode != ProcessingMode::LowPass) {
        m_lowPassState.clear();
    }
}

void WorkerThread::setProcessingMode(const QString &modeName)
{
    const QString normalized = modeName.trimmed().toLower();
    if (normalized == QStringLiteral("scale-offset")) {
        setProcessingMode(ProcessingMode::ScaleOffset);
    } else if (normalized == QStringLiteral("low-pass")) {
        setProcessingMode(ProcessingMode::LowPass);
    } else {
        setProcessingMode(ProcessingMode::PassThrough);
    }
}

void WorkerThread::setScaleOffset(double scale, double offset)
{
    QWriteLocker locker(&m_processingLock);
    m_processingSettings.scale = scale;
    m_processingSettings.offset = offset;
}

void WorkerThread::setLowPassAlpha(double alpha)
{
    QWriteLocker locker(&m_processingLock);
    m_processingSettings.alpha = qBound(0.01, alpha, 1.0);
}

void WorkerThread::setLatestData(const QVector<double> &values)
{
    {
        QWriteLocker locker(&m_dataLock);
        m_latestData = values;
    }

    // New data arrived from DataProcessor; wake the worker without polling.
    requestProcessing();
}

void WorkerThread::requestProcessing()
{
    m_processingRequested.store(true, std::memory_order_release);
    m_waitCondition.wakeOne();
    if (kVerboseDataTrace) {
        qDebug().noquote() << "[Step2][WorkerThread] processing requested";
    }
}

void WorkerThread::stop()
{
    requestInterruption();
    m_waitCondition.wakeAll();
}

void WorkerThread::run()
{
    QVector<double> previousValues;

    while (!isInterruptionRequested()) {
        {
            QMutexLocker locker(&m_waitMutex);
            if (!m_processingRequested.load(std::memory_order_acquire)
                && !isInterruptionRequested()) {
                m_waitCondition.wait(&m_waitMutex, m_intervalMs);
            }
        }

        if (isInterruptionRequested()) {
            break;
        }

        const bool requested = m_processingRequested.exchange(false, std::memory_order_acquire);

        const QVector<double> values = latestDataSnapshot();
        if (!values.isEmpty() && (requested || values != previousValues)) {
            previousValues = values;
            const QVector<double> processedValues = calculateValues(values);

            if (kVerboseDataTrace) {
                qDebug().noquote() << "[Step2][WorkerThread] processed data =" << processedValues;
            }

            emit dataProcessed(processedValues);
        }
    }
}

QVector<double> WorkerThread::latestDataSnapshot() const
{
    {
        QReadLocker locker(&m_dataLock);
        if (!m_latestData.isEmpty()) {
            return m_latestData;
        }
    }

    return m_processor ? m_processor->getLatestData() : QVector<double> {};
}

QVector<double> WorkerThread::calculateValues(const QVector<double> &values)
{
    const ProcessingSettings settings = processingSettings();

    QVector<double> processed;
    processed.resize(values.size());

    for (qsizetype i = 0; i < values.size(); ++i) {
        processed[i] = values[i] * settings.scale + settings.offset;
    }

    if (settings.mode == ProcessingMode::PassThrough) {
        return values;
    }

    if (settings.mode != ProcessingMode::LowPass) {
        return processed;
    }

    QWriteLocker locker(&m_processingLock);
    const double alpha = m_processingSettings.alpha;
    if (m_lowPassState.size() != processed.size()) {
        m_lowPassState = processed;
        return processed;
    }

    for (qsizetype i = 0; i < processed.size(); ++i) {
        m_lowPassState[i] = alpha * processed[i] + (1.0 - alpha) * m_lowPassState[i];
    }

    return m_lowPassState;
}

WorkerThread::ProcessingSettings WorkerThread::processingSettings() const
{
    QReadLocker locker(&m_processingLock);
    return m_processingSettings;
}
