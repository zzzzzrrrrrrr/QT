#ifndef DATAPROCESSOR_H
#define DATAPROCESSOR_H

#include <QMutex>
#include <QObject>
#include <QString>
#include <QVector>

Q_DECLARE_METATYPE(QVector<double>)

/**
 * @brief DataProcessor parses raw device text into numeric samples.
 *
 * It keeps the latest parsed vector so other modules, including worker
 * threads, can safely request the newest data snapshot.
 */
class DataProcessor : public QObject
{
    Q_OBJECT

public:
    explicit DataProcessor(QObject *parent = nullptr);

    QVector<double> getLatestData() const;

public slots:
    void processRawData(const QString &rawData);

signals:
    void dataUpdated(const QVector<double> &values);

private:
    QVector<double> parseValues(const QString &rawData) const;

    mutable QMutex m_mutex;
    QVector<double> m_latestData;
};

#endif // DATAPROCESSOR_H
