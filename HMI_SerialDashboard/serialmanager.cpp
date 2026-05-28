#include "serialmanager.h"

#include <QDebug>

namespace {
constexpr bool kVerboseDataTrace = false;
}

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_tcpSimulationSource(this)
    , m_tcpSocketSource(this)
    , m_serialPortSource(this)
    , m_modbusTcpSource(ModbusDataSource::Transport::Tcp, this)
    , m_modbusRtuSource(ModbusDataSource::Transport::Rtu, this)
{
    const auto connectSource = [this](DataSource *source) {
        connect(source, &DataSource::dataReceived,
                this, &SerialManager::forwardDataReceived);
        connect(source, &DataSource::errorOccurred,
                this, &SerialManager::errorOccurred);
    };

    connectSource(&m_tcpSimulationSource);
    connectSource(&m_tcpSocketSource);
    connectSource(&m_serialPortSource);
    connectSource(&m_modbusTcpSource);
    connectSource(&m_modbusRtuSource);

    connect(&m_serialPortSource, &DataSource::availablePortsChanged,
            this, &SerialManager::availableSerialPortsChanged);
}

SerialManager::~SerialManager()
{
    stop();
}

void SerialManager::setConnectionType(ConnectionType type)
{
    if (isRunning()) {
        stop();
    }

    m_connectionType = type;
}

SerialManager::ConnectionType SerialManager::connectionType() const
{
    return m_connectionType;
}

void SerialManager::setSerialPortName(const QString &portName)
{
    m_serialPortSource.setPortName(portName);
}

void SerialManager::setBaudRate(qint32 baudRate)
{
    m_serialPortSource.setBaudRate(baudRate);
}

void SerialManager::setTcpEndpoint(const QString &host, quint16 port)
{
    m_tcpSocketSource.setEndpoint(host, port);
}

void SerialManager::setTcpSimulationEnabled(bool enabled)
{
    m_tcpSimulationEnabled = enabled;
    if (!enabled) {
        stopTcpSimulation();
    }
}

void SerialManager::setSimulationIntervalMs(int intervalMs)
{
    m_tcpSimulationSource.setIntervalMs(intervalMs);
}

void SerialManager::setFrameConfig(const ProtocolFrameConfig &config)
{
    m_tcpSocketSource.setFrameConfig(config);
    m_serialPortSource.setFrameConfig(config);
}

void SerialManager::setSerialAutoReconnectEnabled(bool enabled)
{
    m_serialPortSource.setAutoReconnectEnabled(enabled);
}

void SerialManager::setSerialReconnectIntervalMs(int intervalMs)
{
    m_serialPortSource.setReconnectIntervalMs(intervalMs);
}

QStringList SerialManager::availableSerialPorts() const
{
    return m_serialPortSource.availablePortNames();
}

void SerialManager::setModbusTcpEndpoint(const QString &host, quint16 port)
{
    m_modbusTcpSource.setTcpEndpoint(host, port);
}

void SerialManager::setModbusSerialPortName(const QString &portName)
{
    m_modbusRtuSource.setSerialPortName(portName);
}

void SerialManager::setModbusBaudRate(qint32 baudRate)
{
    m_modbusRtuSource.setBaudRate(baudRate);
}

void SerialManager::setModbusUnitId(int unitId)
{
    m_modbusTcpSource.setUnitId(unitId);
    m_modbusRtuSource.setUnitId(unitId);
}

void SerialManager::setModbusRegisterRange(int startAddress, int registerCount)
{
    m_modbusTcpSource.setRegisterRange(startAddress, registerCount);
    m_modbusRtuSource.setRegisterRange(startAddress, registerCount);
}

void SerialManager::setModbusPollIntervalMs(int intervalMs)
{
    m_modbusTcpSource.setPollIntervalMs(intervalMs);
    m_modbusRtuSource.setPollIntervalMs(intervalMs);
}

void SerialManager::setModbusTimeoutMs(int timeoutMs)
{
    m_modbusTcpSource.setTimeoutMs(timeoutMs);
    m_modbusRtuSource.setTimeoutMs(timeoutMs);
}

bool SerialManager::isRunning() const
{
    const DataSource *source = m_activeSource ? m_activeSource : selectedSource();
    return source && source->isRunning();
}

bool SerialManager::isTcpSimulationEnabled() const
{
    return m_tcpSimulationEnabled;
}

bool SerialManager::start()
{
    stopAllSources();

    m_activeSource = selectedSource();
    if (!m_activeSource) {
        emit errorOccurred(QStringLiteral("No data source selected."));
        return false;
    }

    const bool started = m_activeSource->start();
    qDebug().noquote() << "[SerialManager] source start result =" << started;
    return started;
}

void SerialManager::stop()
{
    stopAllSources();
    m_activeSource = nullptr;
}

QString SerialManager::readOnce()
{
    DataSource *source = m_activeSource ? m_activeSource : selectedSource();
    return source ? source->readOnce() : QString {};
}

bool SerialManager::startTcpSimulation()
{
    m_connectionType = ConnectionType::TcpSocket;
    m_tcpSimulationEnabled = true;
    return start();
}

void SerialManager::stopTcpSimulation()
{
    m_tcpSimulationSource.stop();
    if (m_activeSource == &m_tcpSimulationSource) {
        m_activeSource = nullptr;
    }
}

void SerialManager::refreshSerialPorts()
{
    m_serialPortSource.refreshAvailablePorts();
}

void SerialManager::forwardDataReceived(const QString &data)
{
    if (kVerboseDataTrace) {
        qDebug().noquote() << "[SerialManager] emit dataReceived(QString) =" << data;
    }
    emit dataReceived(data);
}

DataSource *SerialManager::selectedSource()
{
    if (m_connectionType == ConnectionType::SerialPort) {
        return &m_serialPortSource;
    }

    if (m_connectionType == ConnectionType::ModbusTcp) {
        return &m_modbusTcpSource;
    }

    if (m_connectionType == ConnectionType::ModbusRtu) {
        return &m_modbusRtuSource;
    }

    return m_tcpSimulationEnabled ? static_cast<DataSource *>(&m_tcpSimulationSource)
                                  : static_cast<DataSource *>(&m_tcpSocketSource);
}

const DataSource *SerialManager::selectedSource() const
{
    if (m_connectionType == ConnectionType::SerialPort) {
        return &m_serialPortSource;
    }

    if (m_connectionType == ConnectionType::ModbusTcp) {
        return &m_modbusTcpSource;
    }

    if (m_connectionType == ConnectionType::ModbusRtu) {
        return &m_modbusRtuSource;
    }

    return m_tcpSimulationEnabled ? static_cast<const DataSource *>(&m_tcpSimulationSource)
                                  : static_cast<const DataSource *>(&m_tcpSocketSource);
}

void SerialManager::stopAllSources()
{
    m_tcpSimulationSource.stop();
    m_tcpSocketSource.stop();
    m_serialPortSource.stop();
    m_modbusTcpSource.stop();
    m_modbusRtuSource.stop();
}
