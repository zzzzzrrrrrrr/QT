#include "datasource.h"

#include <QDebug>
#include <QIODevice>
#include <QtMath>

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
    qDebug().noquote() << "[DataSource][TcpSimulation] data =" << payload;
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
#if HMI_HAS_QT_SERIALPORT
    connect(&m_serialPort, &QSerialPort::readyRead,
            this, &SerialPortDataSource::handleReadyRead);
    connect(&m_serialPort, &QSerialPort::errorOccurred,
            this, &SerialPortDataSource::handleSerialError);
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

bool SerialPortDataSource::start()
{
#if HMI_HAS_QT_SERIALPORT
    stop();

    if (m_portName.isEmpty()) {
        emit errorOccurred(QStringLiteral("Serial port name is empty."));
        return false;
    }

    m_serialPort.setPortName(m_portName);
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

void SerialPortDataSource::stop()
{
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

#if HMI_HAS_QT_SERIALPORT
void SerialPortDataSource::handleReadyRead()
{
    readOnce();
}

void SerialPortDataSource::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error != QSerialPort::NoError) {
        emit errorOccurred(m_serialPort.errorString());
    }
}
#endif
