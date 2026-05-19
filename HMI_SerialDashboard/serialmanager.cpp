#include "serialmanager.h"

#include <QDebug>

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
    , m_tcpSimulationSource(this)
    , m_tcpSocketSource(this)
    , m_serialPortSource(this)
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

void SerialManager::forwardDataReceived(const QString &data)
{
    qDebug().noquote() << "[SerialManager] emit dataReceived(QString) =" << data;
    emit dataReceived(data);
}

DataSource *SerialManager::selectedSource()
{
    if (m_connectionType == ConnectionType::SerialPort) {
        return &m_serialPortSource;
    }

    return m_tcpSimulationEnabled ? static_cast<DataSource *>(&m_tcpSimulationSource)
                                  : static_cast<DataSource *>(&m_tcpSocketSource);
}

const DataSource *SerialManager::selectedSource() const
{
    if (m_connectionType == ConnectionType::SerialPort) {
        return &m_serialPortSource;
    }

    return m_tcpSimulationEnabled ? static_cast<const DataSource *>(&m_tcpSimulationSource)
                                  : static_cast<const DataSource *>(&m_tcpSocketSource);
}

void SerialManager::stopAllSources()
{
    m_tcpSimulationSource.stop();
    m_tcpSocketSource.stop();
    m_serialPortSource.stop();
}
