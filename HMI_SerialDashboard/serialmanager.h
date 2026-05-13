#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QAbstractSocket>
#include <QObject>
#include <QTcpSocket>
#include <QTimer>

#ifndef HMI_HAS_QT_SERIALPORT
#define HMI_HAS_QT_SERIALPORT 0
#endif

#if HMI_HAS_QT_SERIALPORT
#include <QSerialPort>
#endif

/**
 * @brief SerialManager hides the data-source details for HMI input.
 *
 * The class can read from either a physical serial port or a TCP socket and
 * emits normalized text payloads through dataReceived().
 */
class SerialManager : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionType {
        SerialPort,
        TcpSocket
    };
    Q_ENUM(ConnectionType)

    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager() override;

    void setConnectionType(ConnectionType type);
    ConnectionType connectionType() const;

    void setSerialPortName(const QString &portName);
    void setBaudRate(qint32 baudRate);
    void setTcpEndpoint(const QString &host, quint16 port);
    void setTcpSimulationEnabled(bool enabled);
    void setSimulationIntervalMs(int intervalMs);

    bool isRunning() const;
    bool isTcpSimulationEnabled() const;

public slots:
    bool start();
    void stop();
    QString readOnce();
    bool startTcpSimulation();
    void stopTcpSimulation();

signals:
    void dataReceived(const QString &data);
    void errorOccurred(const QString &message);

private slots:
    void handleReadyRead();
#if HMI_HAS_QT_SERIALPORT
    void handleSerialError(QSerialPort::SerialPortError error);
#endif
    void handleSocketError(QAbstractSocket::SocketError error);
    void sendSimulatedTcpData();

private:
    bool startSerialPort();
    bool startTcpSocket();
    QString buildSimulatedPayload();
    QString readAvailableData();
    void emitIfNotEmpty(const QString &data);

    ConnectionType m_connectionType = ConnectionType::SerialPort;
#if HMI_HAS_QT_SERIALPORT
    QSerialPort m_serialPort;
#endif
    QTcpSocket m_tcpSocket;
    QString m_serialPortName = QStringLiteral("COM1");
    qint32 m_baudRate = 9600;
    QString m_tcpHost = QStringLiteral("127.0.0.1");
    quint16 m_tcpPort = 502;
    QTimer m_simulationTimer;
    bool m_tcpSimulationEnabled = false;
    int m_simulationIntervalMs = 500;
    int m_simulationCounter = 0;
};

#endif // SERIALMANAGER_H
