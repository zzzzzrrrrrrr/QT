#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include "dataprocessor.h"

#include <QMutex>
#include <QPointer>
#include <QReadWriteLock>
#include <QThread>
#include <QWaitCondition>
#include <QVector>

#include <atomic>

/**
 * @brief WorkerThread periodically republishes processed HMI data.
 *
 * The thread reads the latest snapshot from DataProcessor in the background
 * and emits dataProcessed() for the UI layer.
 */
class WorkerThread : public QThread
{
    Q_OBJECT

public:
    enum class ProcessingMode {
        PassThrough,
        ScaleOffset,
        LowPass
    };

    explicit WorkerThread(QObject *parent = nullptr);
    ~WorkerThread() override;

    void setProcessor(DataProcessor *processor);
    void setIntervalMs(int intervalMs);
    void setProcessingMode(ProcessingMode mode);
    void setProcessingMode(const QString &modeName);
    void setScaleOffset(double scale, double offset);
    void setLowPassAlpha(double alpha);

public slots:
    void setLatestData(const QVector<double> &values);
    void requestProcessing();
    void stop();

signals:
    void dataProcessed(const QVector<double> &values);

protected:
    void run() override;

private:
    struct ProcessingSettings {
        ProcessingMode mode = ProcessingMode::PassThrough;
        double scale = 1.0;
        double offset = 0.0;
        double alpha = 0.35;
    };

    QVector<double> latestDataSnapshot() const;
    QVector<double> calculateValues(const QVector<double> &values);
    ProcessingSettings processingSettings() const;

    QPointer<DataProcessor> m_processor;
    int m_intervalMs = 200;
    std::atomic_bool m_processingRequested = false;
    mutable QReadWriteLock m_dataLock;
    QVector<double> m_latestData;
    mutable QReadWriteLock m_processingLock;
    ProcessingSettings m_processingSettings;
    QVector<double> m_lowPassState;
    QMutex m_waitMutex;
    QWaitCondition m_waitCondition;
};

#endif // WORKERTHREAD_H
