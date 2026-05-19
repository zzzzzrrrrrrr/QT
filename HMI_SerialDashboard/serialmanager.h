#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "datasource.h"

#include <QObject>

/**
 * @brief SerialManager selects the active acquisition data source.
 *
 * It keeps the external API stable while delegating actual IO work to
 * DataSource implementations.
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
    void forwardDataReceived(const QString &data);

private:
    DataSource *selectedSource();
    const DataSource *selectedSource() const;
    void stopAllSources();

    ConnectionType m_connectionType = ConnectionType::SerialPort;
    bool m_tcpSimulationEnabled = false;
    DataSource *m_activeSource = nullptr;
    TcpSimulationDataSource m_tcpSimulationSource;
    TcpSocketDataSource m_tcpSocketSource;
    SerialPortDataSource m_serialPortSource;
};

#endif // SERIALMANAGER_H
