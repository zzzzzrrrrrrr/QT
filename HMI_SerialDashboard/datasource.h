#ifndef DATASOURCE_H
#define DATASOURCE_H

#include <QAbstractSocket>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

#ifndef HMI_HAS_QT_SERIALPORT
#define HMI_HAS_QT_SERIALPORT 0
#endif

#if HMI_HAS_QT_SERIALPORT
#include <QSerialPort>
#endif

/**
 * @brief DataSource is the common interface for all acquisition inputs.
 *
 * Every source emits raw text through dataReceived(), so the processing and UI
 * pipeline stays independent from TCP simulation, TCP socket, or serial input.
 */
class DataSource : public QObject
{
    Q_OBJECT

public:
    explicit DataSource(QObject *parent = nullptr);
    ~DataSource() override = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual QString readOnce() = 0;
    virtual bool isRunning() const = 0;

signals:
    void dataReceived(const QString &data);
    void errorOccurred(const QString &message);

protected:
    void emitIfNotEmpty(const QString &data);
};

class TcpSimulationDataSource : public DataSource
{
    Q_OBJECT

public:
    explicit TcpSimulationDataSource(QObject *parent = nullptr);

    void setIntervalMs(int intervalMs);
    int intervalMs() const;

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

private slots:
    void sendSimulatedData();

private:
    QString buildPayload();

    QTimer m_timer;
    int m_intervalMs = 500;
    int m_counter = 0;
};

class TcpSocketDataSource : public DataSource
{
    Q_OBJECT

public:
    explicit TcpSocketDataSource(QObject *parent = nullptr);

    void setEndpoint(const QString &host, quint16 port);

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

private slots:
    void handleReadyRead();
    void handleSocketError(QAbstractSocket::SocketError error);

private:
    QTcpSocket m_socket;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 502;
};

class SerialPortDataSource : public DataSource
{
    Q_OBJECT

public:
    explicit SerialPortDataSource(QObject *parent = nullptr);

    void setPortName(const QString &portName);
    void setBaudRate(qint32 baudRate);

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

private slots:
#if HMI_HAS_QT_SERIALPORT
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
#endif

private:
    QString m_portName = QStringLiteral("COM1");
    qint32 m_baudRate = 9600;

#if HMI_HAS_QT_SERIALPORT
    QSerialPort m_serialPort;
#endif
};

#endif // DATASOURCE_H
