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
    explicit WorkerThread(QObject *parent = nullptr);
    ~WorkerThread() override;

    void setProcessor(DataProcessor *processor);
    void setIntervalMs(int intervalMs);

public slots:
    void setLatestData(const QVector<double> &values);
    void requestProcessing();
    void stop();

signals:
    void dataProcessed(const QVector<double> &values);

protected:
    void run() override;

private:
    QVector<double> latestDataSnapshot() const;
    QVector<double> calculateValues(const QVector<double> &values) const;

    QPointer<DataProcessor> m_processor;
    int m_intervalMs = 200;
    std::atomic_bool m_processingRequested = false;
    double m_outputOffset = 1.0;
    mutable QReadWriteLock m_dataLock;
    QVector<double> m_latestData;
    QMutex m_waitMutex;
    QWaitCondition m_waitCondition;
};

#endif // WORKERTHREAD_H
