#include "datasource.h"

#include <QDebug>
#include <QIODevice>
#include <QStringList>
#include <QtMath>

#if HMI_HAS_QT_SERIALPORT
#include <QSerialPortInfo>
#endif

#if HMI_HAS_QT_SERIALBUS
#include <QModbusReply>
#include <QModbusRtuSerialClient>
#include <QModbusTcpClient>
#endif

namespace {
constexpr bool kVerboseDataTrace = false;
}

DataSource::DataSource(QObject *parent)
    : QObject(parent)
{
}

void DataSource::emitIfNotEmpty(const QString &data)
{
    if (!data.isEmpty()) {
        emit dataReceived(data);
    }
}

TcpSimulationDataSource::TcpSimulationDataSource(QObject *parent)
    : DataSource(parent)
{
    m_timer.setInterval(m_intervalMs);
    connect(&m_timer, &QTimer::timeout,
            this, &TcpSimulationDataSource::sendSimulatedData);
}

void TcpSimulationDataSource::setIntervalMs(int intervalMs)
{
    m_intervalMs = qMax(50, intervalMs);
    m_timer.setInterval(m_intervalMs);
}

int TcpSimulationDataSource::intervalMs() const
{
    return m_intervalMs;
}

bool TcpSimulationDataSource::start()
{
    if (!m_timer.isActive()) {
        m_counter = 0;
        m_timer.start(m_intervalMs);
    }

    qDebug().noquote() << "[DataSource][TcpSimulation] started,"
                       << "interval =" << m_intervalMs << "ms";

    sendSimulatedData();
    return true;
}

void TcpSimulationDataSource::stop()
{
    if (m_timer.isActive()) {
        m_timer.stop();
        qDebug().noquote() << "[DataSource][TcpSimulation] stopped";
    }
}

QString TcpSimulationDataSource::readOnce()
{
    const QString payload = buildPayload();
    emitIfNotEmpty(payload);
    return payload;
}

bool TcpSimulationDataSource::isRunning() const
{
    return m_timer.isActive();
}

void TcpSimulationDataSource::sendSimulatedData()
{
    const QString payload = buildPayload();
    if (kVerboseDataTrace) {
        qDebug().noquote() << "[DataSource][TcpSimulation] data =" << payload;
    }
    emitIfNotEmpty(payload);
}

QString TcpSimulationDataSource::buildPayload()
{
    ++m_counter;

    const double temperature = 25.0 + qSin(m_counter * 0.25) * 5.0;
    const double pressure = 100.0 + qCos(m_counter * 0.18) * 8.0;
    const double flowRate = 40.0 + (m_counter % 20) * 1.5;

    return QStringLiteral("%1,%2,%3")
        .arg(temperature, 0, 'f', 3)
        .arg(pressure, 0, 'f', 3)
        .arg(flowRate, 0, 'f', 3);
}

TcpSocketDataSource::TcpSocketDataSource(QObject *parent)
    : DataSource(parent)
{
    connect(&m_socket, &QTcpSocket::readyRead,
            this, &TcpSocketDataSource::handleReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred,
            this, &TcpSocketDataSource::handleSocketError);
}

void TcpSocketDataSource::setEndpoint(const QString &host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

bool TcpSocketDataSource::start()
{
    stop();

    if (m_host.isEmpty() || m_port == 0) {
        emit errorOccurred(QStringLiteral("TCP endpoint is invalid."));
        return false;
    }

    m_socket.connectToHost(m_host, m_port);
    return m_socket.state() == QAbstractSocket::ConnectingState
           || m_socket.state() == QAbstractSocket::ConnectedState;
}

void TcpSocketDataSource::stop()
{
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.disconnectFromHost();
        if (m_socket.state() != QAbstractSocket::UnconnectedState) {
            m_socket.abort();
        }
    }
}

QString TcpSocketDataSource::readOnce()
{
    if (m_socket.state() != QAbstractSocket::ConnectedState) {
        return {};
    }

    const QString data = QString::fromUtf8(m_socket.readAll());
    emitIfNotEmpty(data);
    return data;
}

bool TcpSocketDataSource::isRunning() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState
           || m_socket.state() == QAbstractSocket::ConnectingState;
}

void TcpSocketDataSource::handleReadyRead()
{
    readOnce();
}

void TcpSocketDataSource::handleSocketError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    emit errorOccurred(m_socket.errorString());
}

SerialPortDataSource::SerialPortDataSource(QObject *parent)
    : DataSource(parent)
{
    m_reconnectTimer.setSingleShot(true);
    m_reconnectTimer.setInterval(m_reconnectIntervalMs);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &SerialPortDataSource::handleReconnectTimeout);

    m_portScanTimer.setInterval(1000);
    connect(&m_portScanTimer, &QTimer::timeout,
            this, &SerialPortDataSource::refreshAvailablePorts);

#if HMI_HAS_QT_SERIALPORT
    connect(&m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortDataSource::handleReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortDataSource::handleSerialError);

    refreshAvailablePorts();
    m_portScanTimer.start();
#endif
}

void SerialPortDataSource::setPortName(const QString &portName)
{
    m_portName = portName.trimmed();
}

void SerialPortDataSource::setBaudRate(qint32 baudRate)
{
    m_baudRate = baudRate;
}

void SerialPortDataSource::setAutoReconnectEnabled(bool enabled)
{
    m_autoReconnectEnabled = enabled;
    if (!enabled) {
        m_reconnectTimer.stop();
    }
}

void SerialPortDataSource::setReconnectIntervalMs(int intervalMs)
{
    m_reconnectIntervalMs = qMax(200, intervalMs);
    m_reconnectTimer.setInterval(m_reconnectIntervalMs);
}

QStringList SerialPortDataSource::availablePortNames() const
{
#if HMI_HAS_QT_SERIALPORT
    QStringList names;
    const QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &port : ports) {
        names.append(port.portName());
    }
    names.sort(Qt::CaseInsensitive);
    return names;
#else
    return {};
#endif
}

bool SerialPortDataSource::start()
{
#if HMI_HAS_QT_SERIALPORT
    m_stopRequested = false;
    m_reconnectTimer.stop();
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }

    return openConfiguredPort();
#else
    emit errorOccurred(QStringLiteral("Qt SerialPort module is not available in this Qt kit."));
    return false;
#endif
}

void SerialPortDataSource::stop()
{
    m_stopRequested = true;
    m_reconnectTimer.stop();

#if HMI_HAS_QT_SERIALPORT
    if (m_serialPort.isOpen()) {
        m_serialPort.close();
    }
#endif
}

QString SerialPortDataSource::readOnce()
{
#if HMI_HAS_QT_SERIALPORT
    if (!m_serialPort.isOpen()) {
        return {};
    }

    const QString data = QString::fromUtf8(m_serialPort.readAll());
    emitIfNotEmpty(data);
    return data;
#else
    return {};
#endif
}

bool SerialPortDataSource::isRunning() const
{
#if HMI_HAS_QT_SERIALPORT
    return m_serialPort.isOpen();
#else
    return false;
#endif
}

void SerialPortDataSource::refreshAvailablePorts()
{
    const QStringList ports = availablePortNames();
    if (ports == m_lastAvailablePorts) {
        return;
    }

    m_lastAvailablePorts = ports;
    emit availablePortsChanged(ports);
}

#if HMI_HAS_QT_SERIALPORT
void SerialPortDataSource::handleReadyRead()
{
    readOnce();
}

void SerialPortDataSource::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::NoError) {
        return;
    }

    const QString message = m_serialPort.errorString();
    emit errorOccurred(message);

    if (error == QSerialPort::ResourceError
        || error == QSerialPort::DeviceNotFoundError
        || error == QSerialPort::PermissionError
        || error == QSerialPort::OpenError) {
        if (m_serialPort.isOpen()) {
            m_serialPort.close();
        }
        scheduleReconnect(message);
    }
}
#endif

void SerialPortDataSource::handleReconnectTimeout()
{
#if HMI_HAS_QT_SERIALPORT
    if (m_stopRequested || m_serialPort.isOpen()) {
        return;
    }

    openConfiguredPort();
#endif
}

QString SerialPortDataSource::resolvedPortName() const
{
    const QString configuredPort = m_portName.trimmed();
    if (!configuredPort.isEmpty()
        && configuredPort.compare(QStringLiteral("auto"), Qt::CaseInsensitive) != 0) {
        return configuredPort;
    }

    const QStringList ports = availablePortNames();
    return ports.isEmpty() ? QString {} : ports.first();
}

bool SerialPortDataSource::openConfiguredPort()
{
#if HMI_HAS_QT_SERIALPORT
    refreshAvailablePorts();

    const QString portName = resolvedPortName();
    if (portName.isEmpty()) {
        const QString message = QStringLiteral("No serial ports are available.");
        emit errorOccurred(message);
        scheduleReconnect(message);
        return false;
    }

    m_serialPort.setPortName(portName);
    m_serialPort.setBaudRate(m_baudRate);
    m_serialPort.setDataBits(QSerialPort::Data8);
    m_serialPort.setParity(QSerialPort::NoParity);
    m_serialPort.setStopBits(QSerialPort::OneStop);
    m_serialPort.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serialPort.open(QIODevice::ReadWrite)) {
        const QString message = QStringLiteral("Unable to open serial port %1: %2")
                                    .arg(portName, m_serialPort.errorString());
        emit errorOccurred(message);
        scheduleReconnect(message);
        return false;
    }

    qDebug().noquote() << "[DataSource][Serial] opened" << portName
                       << "baud =" << m_baudRate;
    return true;
#else
    return false;
#endif
}

void SerialPortDataSource::scheduleReconnect(const QString &reason)
{
    if (m_stopRequested || !m_autoReconnectEnabled || m_reconnectTimer.isActive()) {
        return;
    }

    qDebug().noquote() << "[DataSource][Serial] reconnect scheduled:" << reason;
    m_reconnectTimer.start(m_reconnectIntervalMs);
}

ModbusDataSource::ModbusDataSource(Transport transport, QObject *parent)
    : DataSource(parent)
    , m_transport(transport)
{
    m_pollTimer.setInterval(m_pollIntervalMs);
    connect(&m_pollTimer, &QTimer::timeout,
            this, &ModbusDataSource::pollRegisters);
}

ModbusDataSource::~ModbusDataSource()
{
    stop();
}

void ModbusDataSource::setTcpEndpoint(const QString &host, quint16 port)
{
    m_host = host.trimmed();
    m_port = port;
}

void ModbusDataSource::setSerialPortName(const QString &portName)
{
    m_serialPortName = portName.trimmed();
}

void ModbusDataSource::setBaudRate(qint32 baudRate)
{
    m_baudRate = baudRate;
}

void ModbusDataSource::setUnitId(int unitId)
{
    m_unitId = qBound(1, unitId, 247);
}

void ModbusDataSource::setRegisterRange(int startAddress, int registerCount)
{
    m_startAddress = qMax(0, startAddress);
    m_registerCount = qBound(1, registerCount, 125);
}

void ModbusDataSource::setPollIntervalMs(int intervalMs)
{
    m_pollIntervalMs = qMax(100, intervalMs);
    m_pollTimer.setInterval(m_pollIntervalMs);
}

void ModbusDataSource::setTimeoutMs(int timeoutMs)
{
    m_timeoutMs = qMax(100, timeoutMs);
}

bool ModbusDataSource::start()
{
#if HMI_HAS_QT_SERIALBUS
    stop();

    if (!createClient()) {
        return false;
    }

    configureClient();
    if (!m_client->connectDevice()) {
        emit errorOccurred(QStringLiteral("Unable to start Modbus client: %1")
                               .arg(m_client->errorString()));
        return false;
    }

    m_pollTimer.start(m_pollIntervalMs);
    return true;
#else
    emit errorOccurred(QStringLiteral("Qt SerialBus module is not available in this Qt kit."));
    return false;
#endif
}

void ModbusDataSource::stop()
{
    m_pollTimer.stop();

#if HMI_HAS_QT_SERIALBUS
    if (m_client) {
        if (m_client->state() != QModbusDevice::UnconnectedState) {
            m_client->disconnectDevice();
        }
        m_client->deleteLater();
        m_client = nullptr;
    }
#endif
}

QString ModbusDataSource::readOnce()
{
    pollRegisters();
    return {};
}

bool ModbusDataSource::isRunning() const
{
#if HMI_HAS_QT_SERIALBUS
    return m_client
           && (m_client->state() == QModbusDevice::ConnectedState
               || m_client->state() == QModbusDevice::ConnectingState);
#else
    return false;
#endif
}

void ModbusDataSource::pollRegisters()
{
#if HMI_HAS_QT_SERIALBUS
    if (!m_client || m_client->state() != QModbusDevice::ConnectedState) {
        return;
    }

    const QModbusDataUnit request(QModbusDataUnit::HoldingRegisters,
                                  m_startAddress,
                                  static_cast<quint16>(m_registerCount));
    QModbusReply *reply = m_client->sendReadRequest(request, m_unitId);
    if (!reply) {
        emit errorOccurred(QStringLiteral("Modbus read request failed: %1")
                               .arg(m_client->errorString()));
        return;
    }

    if (reply->isFinished()) {
        reply->deleteLater();
        return;
    }

    connect(reply, &QModbusReply::finished,
            this, [this, reply]() { handleReadReply(reply); });
#endif
}

#if HMI_HAS_QT_SERIALBUS
void ModbusDataSource::handleDeviceError(QModbusDevice::Error error)
{
    if (error != QModbusDevice::NoError && m_client) {
        emit errorOccurred(QStringLiteral("Modbus error: %1").arg(m_client->errorString()));
    }
}

void ModbusDataSource::handleStateChanged(QModbusDevice::State state)
{
    qDebug().noquote() << "[DataSource][Modbus] state =" << state;
    if (state == QModbusDevice::ConnectedState) {
        pollRegisters();
    }
}

bool ModbusDataSource::createClient()
{
    if (m_transport == Transport::Tcp) {
        m_client = new QModbusTcpClient(this);
    } else {
        m_client = new QModbusRtuSerialClient(this);
    }

    connect(m_client, &QModbusDevice::errorOccurred,
            this, &ModbusDataSource::handleDeviceError);
    connect(m_client, &QModbusDevice::stateChanged,
            this, &ModbusDataSource::handleStateChanged);

    return true;
}

void ModbusDataSource::configureClient()
{
    m_client->setTimeout(m_timeoutMs);
    m_client->setNumberOfRetries(1);

    if (m_transport == Transport::Tcp) {
        m_client->setConnectionParameter(QModbusDevice::NetworkAddressParameter, m_host);
        m_client->setConnectionParameter(QModbusDevice::NetworkPortParameter, m_port);
        return;
    }

    m_client->setConnectionParameter(QModbusDevice::SerialPortNameParameter, m_serialPortName);
    m_client->setConnectionParameter(QModbusDevice::SerialBaudRateParameter, m_baudRate);
}

void ModbusDataSource::handleReadReply(QModbusReply *reply)
{
    if (!reply) {
        return;
    }

    if (reply->error() == QModbusDevice::NoError) {
        const QString payload = payloadFromUnit(reply->result());
        if (kVerboseDataTrace) {
            qDebug().noquote() << "[DataSource][Modbus] data =" << payload;
        }
        emitIfNotEmpty(payload);
    } else {
        emit errorOccurred(QStringLiteral("Modbus reply error: %1").arg(reply->errorString()));
    }

    reply->deleteLater();
}

QString ModbusDataSource::payloadFromUnit(const QModbusDataUnit &unit) const
{
    QStringList values;
    values.reserve(unit.valueCount());

    for (uint i = 0; i < unit.valueCount(); ++i) {
        values.append(QString::number(unit.value(i)));
    }

    return values.join(QStringLiteral(","));
}
#endif
