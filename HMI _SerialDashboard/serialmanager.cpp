#include "serialmanager.h"

#include <QDebug>
#include <QIODevice>
#include <QtMath>

SerialManager::SerialManager(QObject *parent)
    : QObject(parent)
{
    m_simulationTimer.setInterval(m_simulationIntervalMs);

#if HMI_HAS_QT_SERIALPORT
    connect(&m_serialPort, &QSerialPort::readyRead,
            this, &SerialManager::handleReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialManager::handleSerialError);
#endif

    connect(&m_tcpSocket, &QTcpSocket::readyRead,
            this, &SerialManager::handleReadyRead);
    connect(&m_tcpSocket, &QTcpSocket::errorOccurred,
            this, &SerialManager::handleSocketError);
    connect(&m_simulationTimer, &QTimer::timeout,
            this, &SerialManager::sendSimulatedTcpData);
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
    m_serialPortName = portName.trimmed();
}

void SerialManager::setBaudRate(qint32 baudRate)
{
    m_baudRate = baudRate;
}

void SerialManager::setTcpEndpoint(const QString &host, quint16 port)
{
    m_tcpHost = host.trimmed();
    m_tcpPort = port;
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
    m_simulationIntervalMs = qMax(50, intervalMs);
    m_simulationTimer.setInterval(m_simulationIntervalMs);
}

bool SerialManager::isRunning() const
{
    if (m_connectionType == ConnectionType::SerialPort) {
#if HMI_HAS_QT_SERIALPORT
        return m_serialPort.isOpen();
#else
        return false;
#endif
    }

    if (m_tcpSimulationEnabled) {
        return m_simulationTimer.isActive();
    }

    return m_tcpSocket.state() == QAbstractSocket::ConnectedState
           || m_tcpSocket.state() == QAbstractSocket::ConnectingState;
}

bool SerialManager::isTcpSimulationEnabled() const
{
    return m_tcpSimulationEnabled;
}

bool SerialManager::start()
{
    stop();

    if (m_connectionType == ConnectionType::SerialPort) {
        return startSerialPort();
    }

    return startTcpSocket();
}

void SerialManager::stop()
{
    stopTcpSimulation();

#if HMI_HAS_QT_SERIALPORT
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
#endif

    if (m_tcpSocket.state() != QAbstractSocket::UnconnectedState) {
        m_tcpSocket.disconnectFromHost();
        if (m_tcpSocket.state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket.abort();
        }
    }
}

QString SerialManager::readOnce()
{
    if (m_tcpSimulationEnabled && m_connectionType == ConnectionType::TcpSocket) {
        const QString data = buildSimulatedPayload();
        emitIfNotEmpty(data);
        return data;
    }

    const QString data = readAvailableData();
    emitIfNotEmpty(data);
    return data;
}

bool SerialManager::startTcpSimulation()
{
    m_connectionType = ConnectionType::TcpSocket;
    m_tcpSimulationEnabled = true;

    if (!m_simulationTimer.isActive()) {
        m_simulationCounter = 0;
        m_simulationTimer.start(m_simulationIntervalMs);
    }

    qDebug().noquote() << "[Step2][SerialManager] TCP simulation started,"
                       << "interval =" << m_simulationIntervalMs << "ms";

    sendSimulatedTcpData();
    return true;
}

void SerialManager::stopTcpSimulation()
{
    if (m_simulationTimer.isActive()) {
        m_simulationTimer.stop();
        qDebug().noquote() << "[Step2][SerialManager] TCP simulation stopped";
    }
}

void SerialManager::handleReadyRead()
{
    readOnce();
}

#if HMI_HAS_QT_SERIALPORT
void SerialManager::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    emit errorOccurred(m_serialPort.errorString());
}
#endif

void SerialManager::handleSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_tcpSocket.errorString());
}

void SerialManager::sendSimulatedTcpData()
{
    const QString payload = buildSimulatedPayload();

    // Step 2 verification: simulated TCP payload enters the signal chain here.
    qDebug().noquote() << "[Step2][SerialManager] simulated TCP data =" << payload;

    emitIfNotEmpty(payload);
}

bool SerialManager::startSerialPort()
{
#if HMI_HAS_QT_SERIALPORT
    if (m_serialPortName.isEmpty()) {
        emit errorOccurred(QStringLiteral("Serial port name is empty."));
        return false;
    }

    m_serialPort.setPortName(m_serialPortName);
    m_serialPort.setBaudRate(m_baudRate);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setParity(QSerialPort::NoParity);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        emit errorOccurred(m_serialPort.errorString());
        return false;
    }

    return true;
#else
    emit errorOccurred(QStringLiteral("Qt SerialPort module is not available in this Qt kit."));
    return false;
#endif
}

bool SerialManager::startTcpSocket()
{
    if (m_tcpSimulationEnabled) {
        return startTcpSimulation();
    }

    if (m_tcpHost.isEmpty() || m_tcpPort == 0) {
        emit errorOccurred(QStringLiteral("TCP endpoint is invalid."));
        return false;
    }

    m_tcpSocket.connectToHost(m_tcpHost, m_tcpPort);
    return m_tcpSocket.state() == QAbstractSocket::ConnectingState
           || m_tcpSocket.state() == QAbstractSocket::ConnectedState;
}

QString SerialManager::buildSimulatedPayload()
{
    ++m_simulationCounter;

    const double temperature = 25.0 + qSin(m_simulationCounter * 0.25) * 5.0;
    const double pressure = 100.0 + qCos(m_simulationCounter * 0.18) * 8.0;
    const double flowRate = 40.0 + (m_simulationCounter % 20) * 1.5;

    return QStringLiteral("%1,%2,%3")
        .arg(temperature, 0, 'f', 3)
        .arg(pressure, 0, 'f', 3)
        .arg(flowRate, 0, 'f', 3);
}

QString SerialManager::readAvailableData()
{
    QByteArray bytes;

#if HMI_HAS_QT_SERIALPORT
    if (m_connectionType == ConnectionType::SerialPort && m_serialPort.isOpen()) {
        bytes = m_serialPort.readAll();
    } else if (m_connectionType == ConnectionType::TcpSocket
               && m_tcpSocket.state() == QAbstractSocket::ConnectedState) {
        bytes = m_tcpSocket.readAll();
    }
#else
    if (m_connectionType == ConnectionType::TcpSocket
        && m_tcpSocket.state() == QAbstractSocket::ConnectedState) {
        bytes = m_tcpSocket.readAll();
    }
#endif

    return QString::fromUtf8(bytes);
}

void SerialManager::emitIfNotEmpty(const QString &data)
{
    if (!data.isEmpty()) {
        qDebug().noquote() << "[Step2][SerialManager] emit dataReceived(QString) =" << data;
        emit dataReceived(data);
    }
}
