#ifndef DATASOURCE_H
#define DATASOURCE_H

#include "protocolframer.h"

#include <QAbstractSocket>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#ifndef HMI_HAS_QT_SERIALPORT
#define HMI_HAS_QT_SERIALPORT 0
#endif

#ifndef HMI_HAS_QT_SERIALBUS
#define HMI_HAS_QT_SERIALBUS 0
#endif

#if HMI_HAS_QT_SERIALPORT
#include <QSerialPort>
#endif

#if HMI_HAS_QT_SERIALBUS
#include <QModbusClient>
#include <QModbusDataUnit>
#include <QModbusDevice>
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
    void availablePortsChanged(const QStringList &ports);

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
    void setFrameConfig(const ProtocolFrameConfig &config);

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

private slots:
    void handleReadyRead();
    void handleSocketError(QAbstractSocket::SocketError error);

private:
    QString emitDecodedFrames(const QByteArray &bytes);

    QTcpSocket m_socket;
    ProtocolFramer m_framer;
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
    void setAutoReconnectEnabled(bool enabled);
    void setReconnectIntervalMs(int intervalMs);
    void setFrameConfig(const ProtocolFrameConfig &config);
    QStringList availablePortNames() const;

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

public slots:
    void refreshAvailablePorts();

private slots:
#if HMI_HAS_QT_SERIALPORT
    void handleReadyRead();
    void handleSerialError(QSerialPort::SerialPortError error);
#endif
    void handleReconnectTimeout();

private:
    QString resolvedPortName() const;
    bool openConfiguredPort();
    void scheduleReconnect(const QString &reason);
    QString emitDecodedFrames(const QByteArray &bytes);

    QString m_portName = QStringLiteral("COM1");
    qint32 m_baudRate = 9600;
    bool m_autoReconnectEnabled = true;
    bool m_stopRequested = false;
    int m_reconnectIntervalMs = 1000;
    QStringList m_lastAvailablePorts;
    QTimer m_reconnectTimer;
    QTimer m_portScanTimer;
    ProtocolFramer m_framer;

#if HMI_HAS_QT_SERIALPORT
    QSerialPort m_serialPort;
#endif
};

class ModbusDataSource : public DataSource
{
    Q_OBJECT

public:
    enum class Transport {
        Tcp,
        Rtu
    };
    Q_ENUM(Transport)

    explicit ModbusDataSource(Transport transport, QObject *parent = nullptr);
    ~ModbusDataSource() override;

    void setTcpEndpoint(const QString &host, quint16 port);
    void setSerialPortName(const QString &portName);
    void setBaudRate(qint32 baudRate);
    void setUnitId(int unitId);
    void setRegisterRange(int startAddress, int registerCount);
    void setPollIntervalMs(int intervalMs);
    void setTimeoutMs(int timeoutMs);

    bool start() override;
    void stop() override;
    QString readOnce() override;
    bool isRunning() const override;

private slots:
    void pollRegisters();
#if HMI_HAS_QT_SERIALBUS
    void handleDeviceError(QModbusDevice::Error error);
    void handleStateChanged(QModbusDevice::State state);
#endif

private:
#if HMI_HAS_QT_SERIALBUS
    bool createClient();
    void configureClient();
    void handleReadReply(QModbusReply *reply);
    QString payloadFromUnit(const QModbusDataUnit &unit) const;
    QModbusClient *m_client = nullptr;
#endif

    Transport m_transport;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 502;
    QString m_serialPortName = QStringLiteral("COM1");
    qint32 m_baudRate = 9600;
    int m_unitId = 1;
    int m_startAddress = 0;
    int m_registerCount = 3;
    int m_pollIntervalMs = 500;
    int m_timeoutMs = 1000;
    QTimer m_pollTimer;
};

#endif // DATASOURCE_H
