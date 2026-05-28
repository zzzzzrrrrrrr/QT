#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include "datasource.h"

#include <QObject>
#include <QStringList>

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
        TcpSocket,
        ModbusTcp,
        ModbusRtu
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
    void setFrameConfig(const ProtocolFrameConfig &config);
    void setSerialAutoReconnectEnabled(bool enabled);
    void setSerialReconnectIntervalMs(int intervalMs);
    QStringList availableSerialPorts() const;
    void setModbusTcpEndpoint(const QString &host, quint16 port);
    void setModbusSerialPortName(const QString &portName);
    void setModbusBaudRate(qint32 baudRate);
    void setModbusUnitId(int unitId);
    void setModbusRegisterRange(int startAddress, int registerCount);
    void setModbusPollIntervalMs(int intervalMs);
    void setModbusTimeoutMs(int timeoutMs);

    bool isRunning() const;
    bool isTcpSimulationEnabled() const;

public slots:
    bool start();
    void stop();
    QString readOnce();
    bool startTcpSimulation();
    void stopTcpSimulation();
    void refreshSerialPorts();

signals:
    void dataReceived(const QString &data);
    void errorOccurred(const QString &message);
    void availableSerialPortsChanged(const QStringList &ports);

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
    ModbusDataSource m_modbusTcpSource;
    ModbusDataSource m_modbusRtuSource;
};

#endif // SERIALMANAGER_H
