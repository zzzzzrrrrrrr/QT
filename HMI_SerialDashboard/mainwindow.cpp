#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDebug>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QLineEdit>
#include <QMessageBox>
#include <QPointF>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableView>
#include <QTextStream>
#include <QVBoxLayout>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupRuntimeUiExtensions();
    connectSignals();
    initializeModules();

    ui->statusbar->showMessage(tr("Ready"));
}

MainWindow::~MainWindow()
{
    handleStopClicked();
    delete ui;
}

void MainWindow::handleStartClicked()
{
    qDebug().noquote() << "[Step2][MainWindow] Start clicked";

    configureInputSource();
    m_pendingLatestValues.clear();
    m_pendingUiSamples.clear();
    m_droppedUiSamples = 0;
    m_hasPendingUiUpdate = false;

    if (!m_workerThread.isRunning()) {
        m_workerThread.start();
    }

    if (m_serialManager.start()) {
        if (m_loggingEnabled) {
            if (!m_dataLogger.startSession()) {
                appendLogMessage(tr("CSV logging is disabled for this session because the log file could not start."));
            }
        }

        ui->startButton->setEnabled(false);
        ui->stopButton->setEnabled(true);
        updateHeaderState(tr("Running"));
        ui->statusbar->showMessage(tr("Data acquisition started"));
        appendLogMessage(tr("Data acquisition started"));
        qDebug().noquote() << "[Step2][MainWindow] signal chain test is running";
    }
}

void MainWindow::handleStopClicked()
{
    qDebug().noquote() << "[Step2][MainWindow] Stop clicked";

    flushPendingUiUpdates();
    m_serialManager.stop();
    m_dataLogger.stopSession();

    if (m_workerThread.isRunning()) {
        m_workerThread.stop();
        m_workerThread.wait(1000);
    }

    if (ui) {
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        updateHeaderState(tr("Stopped"));
        ui->statusbar->showMessage(tr("Data acquisition stopped"));
        appendLogMessage(tr("Data acquisition stopped"));
    }
}

void MainWindow::handleSettingsClicked()
{
    openSettingsDialog();
}

void MainWindow::handleAlarmAcknowledgeClicked()
{
    m_alarmManager.acknowledgeCurrentAlarm();
    updateAlarmUi(m_alarmManager.currentState());
}

void MainWindow::handleAlarmSilenceClicked()
{
    m_alarmManager.setSilenced(!m_alarmManager.isSilenced());
    updateAlarmUi(m_alarmManager.currentState());
}

void MainWindow::handleAlarmExportClicked()
{
    const QString csv = m_alarmManager.historyAsCsv();
    if (m_alarmManager.history().isEmpty()) {
        QMessageBox::information(this, tr("Export Alarm History"), tr("No alarm history to export."));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Export Alarm History"),
        QDir(defaultDataLogDirectory()).filePath(QStringLiteral("alarm_history.csv")),
        tr("CSV Files (*.csv)"));
    if (filePath.isEmpty()) {
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("Export Alarm History"),
                             tr("Unable to write alarm history: %1").arg(filePath));
        return;
    }

    QTextStream stream(&file);
    stream << csv;
    appendLogMessage(tr("Alarm history exported: %1").arg(filePath));
}

void MainWindow::handleAlarmClearClicked()
{
    m_alarmManager.clearHistory();
    appendLogMessage(tr("Alarm history cleared"));
}

void MainWindow::handleAvailableSerialPortsChanged(const QStringList &ports)
{
    appendLogMessage(ports.isEmpty()
                         ? tr("Serial ports changed: none available")
                         : tr("Serial ports changed: %1").arg(ports.join(QStringLiteral(", "))));
}

void MainWindow::handleProcessedData(const QVector<double> &values)
{
    const AlarmManager::AlarmState alarmState = m_alarmManager.evaluate(values);

    if (m_loggingEnabled && m_dataLogger.isRecording()) {
        m_dataLogger.logSample(values, alarmState.active, alarmState.message);
    }

    m_pendingLatestValues = values;
    m_pendingAlarmState = alarmState;
    m_hasPendingUiUpdate = true;
    m_pendingUiSamples.append(values);

    while (m_pendingUiSamples.size() > m_maxSamplesPerUiRefresh) {
        m_pendingUiSamples.removeFirst();
        ++m_droppedUiSamples;
    }
}

void MainWindow::handleStatusMessage(const QString &message)
{
    ui->statusbar->showMessage(message);
    appendLogMessage(message);
}

void MainWindow::flushPendingUiUpdates()
{
    if (!m_hasPendingUiUpdate) {
        return;
    }

    const QVector<double> latestValues = m_pendingLatestValues;
    const QVector<QVector<double>> samples = m_pendingUiSamples;
    const AlarmManager::AlarmState alarmState = m_pendingAlarmState;
    const int droppedSamples = m_droppedUiSamples;

    m_pendingLatestValues.clear();
    m_pendingUiSamples.clear();
    m_droppedUiSamples = 0;
    m_hasPendingUiUpdate = false;

    const QString valuesText = formatValues(latestValues);
    ui->dataView->setPlainText(tr("Processed values:\n%1").arg(valuesText));
    updateValueCards(latestValues);
    appendHistoryRows(samples);
    updateChartBatch(samples);
    updateAlarmUi(alarmState);

    const QString statusText = droppedSamples > 0
                                   ? tr("UI refreshed, %1 samples coalesced").arg(droppedSamples)
                                   : tr("UI refreshed");
    ui->statusbar->showMessage(statusText);
}

void MainWindow::initializeModules()
{
    loadConfiguration();
    ensureDefaultConfiguration();
    applyConfiguration();
    saveConfiguration();

    ui->stopButton->setEnabled(false);
    updateHeaderState(tr("Idle"));
    updateValueCards({});
    updateAlarmUi(m_alarmManager.currentState());
    refreshAlarmHistoryTable();
}

void MainWindow::connectSignals()
{
    connect(ui->startButton, &QPushButton::clicked,
            this, &MainWindow::handleStartClicked);
    connect(ui->stopButton, &QPushButton::clicked,
            this, &MainWindow::handleStopClicked);
    connect(ui->settingsButton, &QPushButton::clicked,
            this, &MainWindow::handleSettingsClicked);
    connect(m_alarmAckButton, &QPushButton::clicked,
            this, &MainWindow::handleAlarmAcknowledgeClicked);
    connect(m_alarmSilenceButton, &QPushButton::clicked,
            this, &MainWindow::handleAlarmSilenceClicked);
    connect(m_alarmExportButton, &QPushButton::clicked,
            this, &MainWindow::handleAlarmExportClicked);
    connect(m_alarmClearButton, &QPushButton::clicked,
            this, &MainWindow::handleAlarmClearClicked);

    connect(&m_serialManager, &SerialManager::dataReceived,
            &m_dataProcessor, &DataProcessor::processRawData);
    connect(&m_serialManager, &SerialManager::errorOccurred,
            this, &MainWindow::handleStatusMessage);
    connect(&m_serialManager, &SerialManager::availableSerialPortsChanged,
            this, &MainWindow::handleAvailableSerialPortsChanged);
    connect(&m_dataProcessor, &DataProcessor::dataUpdated,
            &m_workerThread, &WorkerThread::setLatestData);
    connect(&m_workerThread, &WorkerThread::dataProcessed,
            this, &MainWindow::handleProcessedData);
    connect(&m_alarmManager, &AlarmManager::alarmStateChanged,
            this, [this](bool active, const QString &message) {
                appendLogMessage(active ? tr("Alarm active: %1").arg(message)
                                        : tr("Alarm cleared"));
            });
    connect(&m_alarmManager, &AlarmManager::alarmAcknowledged,
            this, [this](const QString &message) {
                appendLogMessage(tr("Alarm acknowledged: %1").arg(message));
                refreshAlarmHistoryTable();
            });
    connect(&m_alarmManager, &AlarmManager::alarmSilenced,
            this, [this](bool silenced) {
                appendLogMessage(silenced ? tr("Alarm silenced")
                                          : tr("Alarm sound enabled"));
            });
    connect(&m_alarmManager, &AlarmManager::alarmHistoryChanged,
            this, &MainWindow::refreshAlarmHistoryTable);
    connect(&m_dataLogger, &DataLogger::logMessage,
            this, &MainWindow::appendLogMessage);
    connect(&m_dataLogger, &DataLogger::errorOccurred,
            this, &MainWindow::handleStatusMessage);
    connect(&m_uiRefreshTimer, &QTimer::timeout,
            this, &MainWindow::flushPendingUiUpdates);
}

void MainWindow::ensureDefaultConfiguration()
{
    setDefaultValue(QStringLiteral("io.mode"), QStringLiteral("tcp-sim"));
    setDefaultValue(QStringLiteral("serial.portName"), QStringLiteral("COM1"));
    setDefaultValue(QStringLiteral("serial.baudRate"), 9600);
    setDefaultValue(QStringLiteral("serial.autoReconnect"), true);
    setDefaultValue(QStringLiteral("serial.reconnectIntervalMs"), 1000);
    setDefaultValue(QStringLiteral("tcp.host"), QStringLiteral("127.0.0.1"));
    setDefaultValue(QStringLiteral("tcp.port"), 502);
    setDefaultValue(QStringLiteral("modbus.tcp.host"), QStringLiteral("127.0.0.1"));
    setDefaultValue(QStringLiteral("modbus.tcp.port"), 502);
    setDefaultValue(QStringLiteral("modbus.rtu.portName"), QStringLiteral("COM1"));
    setDefaultValue(QStringLiteral("modbus.rtu.baudRate"), 9600);
    setDefaultValue(QStringLiteral("modbus.unitId"), 1);
    setDefaultValue(QStringLiteral("modbus.startAddress"), 0);
    setDefaultValue(QStringLiteral("modbus.registerCount"), 3);
    setDefaultValue(QStringLiteral("modbus.pollIntervalMs"), 500);
    setDefaultValue(QStringLiteral("modbus.timeoutMs"), 1000);
    setDefaultValue(QStringLiteral("simulation.intervalMs"), 250);
    setDefaultValue(QStringLiteral("protocol.framingMode"), QStringLiteral("line"));
    setDefaultValue(QStringLiteral("protocol.delimiter"), QStringLiteral("\\n"));
    setDefaultValue(QStringLiteral("protocol.fixedLength"), 64);
    setDefaultValue(QStringLiteral("protocol.maxFrameBytes"), 4096);
    setDefaultValue(QStringLiteral("worker.intervalMs"), 200);
    setDefaultValue(QStringLiteral("worker.processingMode"), QStringLiteral("pass-through"));
    setDefaultValue(QStringLiteral("worker.scale"), 1.0);
    setDefaultValue(QStringLiteral("worker.offset"), 0.0);
    setDefaultValue(QStringLiteral("worker.lowPassAlpha"), 0.35);
    setDefaultValue(QStringLiteral("alarm.temperatureHigh"), 32.0);
    setDefaultValue(QStringLiteral("alarm.pressureHigh"), 108.0);
    setDefaultValue(QStringLiteral("alarm.flowHigh"), 70.0);
    setDefaultValue(QStringLiteral("alarm.temperatureLow"), -20.0);
    setDefaultValue(QStringLiteral("alarm.pressureLow"), 0.0);
    setDefaultValue(QStringLiteral("alarm.flowLow"), 0.0);
    setDefaultValue(QStringLiteral("alarm.hysteresis"), 0.5);
    setDefaultValue(QStringLiteral("alarm.latchingEnabled"), false);
    setDefaultValue(QStringLiteral("alarm.maxHistoryRecords"), 500);
    setDefaultValue(QStringLiteral("logging.enabled"), true);
    setDefaultValue(QStringLiteral("logging.directory"), defaultDataLogDirectory());
    setDefaultValue(QStringLiteral("logging.flushIntervalMs"), 1000);
    setDefaultValue(QStringLiteral("ui.maxHistoryRows"), 200);
    setDefaultValue(QStringLiteral("ui.maxChartPoints"), 120);
    setDefaultValue(QStringLiteral("ui.refreshIntervalMs"), 200);
    setDefaultValue(QStringLiteral("ui.maxSamplesPerRefresh"), 20);
}

void MainWindow::loadConfiguration()
{
    if (!m_configManager.loadJson(configFilePath())) {
        appendLogMessage(tr("Using default configuration"));
    }
}

void MainWindow::saveConfiguration()
{
    const QFileInfo configInfo(configFilePath());
    QDir().mkpath(configInfo.absolutePath());

    if (!m_configManager.saveJson(configFilePath())) {
        appendLogMessage(tr("Unable to save configuration: %1").arg(configFilePath()));
    }
}

void MainWindow::applyConfiguration()
{
    configureInputSource();
    m_serialManager.setSerialPortName(
        m_configManager.getValue(QStringLiteral("serial.portName"), QMetaType::QString).toString());
    m_serialManager.setBaudRate(
        m_configManager.getValue(QStringLiteral("serial.baudRate"), QMetaType::Int).toInt());
    m_serialManager.setSerialAutoReconnectEnabled(
        m_configManager.getValue(QStringLiteral("serial.autoReconnect"), QMetaType::Bool).toBool());
    m_serialManager.setSerialReconnectIntervalMs(
        m_configManager.getValue(QStringLiteral("serial.reconnectIntervalMs"), QMetaType::Int).toInt());
    m_serialManager.setTcpEndpoint(
        m_configManager.getValue(QStringLiteral("tcp.host"), QMetaType::QString).toString(),
        static_cast<quint16>(m_configManager.getValue(QStringLiteral("tcp.port"), QMetaType::Int).toUInt()));
    m_serialManager.setSimulationIntervalMs(
        m_configManager.getValue(QStringLiteral("simulation.intervalMs"), QMetaType::Int).toInt());
    ProtocolFrameConfig frameConfig;
    frameConfig.mode = ProtocolFrameConfig::modeFromString(
        m_configManager.getValue(QStringLiteral("protocol.framingMode"), QMetaType::QString).toString());
    frameConfig.delimiter = ProtocolFrameConfig::parseEscapedBytes(
        m_configManager.getValue(QStringLiteral("protocol.delimiter"), QMetaType::QString).toString(),
        QByteArray("\n"));
    frameConfig.fixedLength =
        m_configManager.getValue(QStringLiteral("protocol.fixedLength"), QMetaType::Int).toInt();
    frameConfig.maxFrameBytes =
        m_configManager.getValue(QStringLiteral("protocol.maxFrameBytes"), QMetaType::Int).toInt();
    m_serialManager.setFrameConfig(frameConfig);
    m_serialManager.setModbusTcpEndpoint(
        m_configManager.getValue(QStringLiteral("modbus.tcp.host"), QMetaType::QString).toString(),
        static_cast<quint16>(m_configManager.getValue(QStringLiteral("modbus.tcp.port"),
                                                      QMetaType::Int).toUInt()));
    m_serialManager.setModbusSerialPortName(
        m_configManager.getValue(QStringLiteral("modbus.rtu.portName"), QMetaType::QString).toString());
    m_serialManager.setModbusBaudRate(
        m_configManager.getValue(QStringLiteral("modbus.rtu.baudRate"), QMetaType::Int).toInt());
    m_serialManager.setModbusUnitId(
        m_configManager.getValue(QStringLiteral("modbus.unitId"), QMetaType::Int).toInt());
    m_serialManager.setModbusRegisterRange(
        m_configManager.getValue(QStringLiteral("modbus.startAddress"), QMetaType::Int).toInt(),
        m_configManager.getValue(QStringLiteral("modbus.registerCount"), QMetaType::Int).toInt());
    m_serialManager.setModbusPollIntervalMs(
        m_configManager.getValue(QStringLiteral("modbus.pollIntervalMs"), QMetaType::Int).toInt());
    m_serialManager.setModbusTimeoutMs(
        m_configManager.getValue(QStringLiteral("modbus.timeoutMs"), QMetaType::Int).toInt());

    m_workerThread.setProcessor(&m_dataProcessor);
    m_workerThread.setIntervalMs(
        m_configManager.getValue(QStringLiteral("worker.intervalMs"), QMetaType::Int).toInt());
    m_workerThread.setProcessingMode(
        m_configManager.getValue(QStringLiteral("worker.processingMode"), QMetaType::QString).toString());
    m_workerThread.setScaleOffset(
        m_configManager.getValue(QStringLiteral("worker.scale"), QMetaType::Double).toDouble(),
        m_configManager.getValue(QStringLiteral("worker.offset"), QMetaType::Double).toDouble());
    m_workerThread.setLowPassAlpha(
        m_configManager.getValue(QStringLiteral("worker.lowPassAlpha"), QMetaType::Double).toDouble());

    m_alarmManager.setTemperatureHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.temperatureHigh"), QMetaType::Double).toDouble());
    m_alarmManager.setPressureHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.pressureHigh"), QMetaType::Double).toDouble());
    m_alarmManager.setFlowHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.flowHigh"), QMetaType::Double).toDouble());
    m_alarmManager.setTemperatureLowLimit(
        m_configManager.getValue(QStringLiteral("alarm.temperatureLow"), QMetaType::Double).toDouble());
    m_alarmManager.setPressureLowLimit(
        m_configManager.getValue(QStringLiteral("alarm.pressureLow"), QMetaType::Double).toDouble());
    m_alarmManager.setFlowLowLimit(
        m_configManager.getValue(QStringLiteral("alarm.flowLow"), QMetaType::Double).toDouble());
    m_alarmManager.setHysteresis(
        m_configManager.getValue(QStringLiteral("alarm.hysteresis"), QMetaType::Double).toDouble());
    m_alarmManager.setLatchingEnabled(
        m_configManager.getValue(QStringLiteral("alarm.latchingEnabled"), QMetaType::Bool).toBool());
    m_alarmManager.setMaxHistoryRecords(
        m_configManager.getValue(QStringLiteral("alarm.maxHistoryRecords"), QMetaType::Int).toInt());

    m_loggingEnabled = m_configManager.getValue(QStringLiteral("logging.enabled"), QMetaType::Bool).toBool();
    m_dataLogger.setOutputDirectory(
        m_configManager.getValue(QStringLiteral("logging.directory"), QMetaType::QString).toString());
    m_dataLogger.setFlushIntervalMs(
        m_configManager.getValue(QStringLiteral("logging.flushIntervalMs"), QMetaType::Int).toInt());
    m_maxHistoryRows = m_configManager.getValue(QStringLiteral("ui.maxHistoryRows"), QMetaType::Int).toInt();
    m_uiRefreshIntervalMs = qMax(
        50,
        m_configManager.getValue(QStringLiteral("ui.refreshIntervalMs"), QMetaType::Int).toInt());
    m_maxSamplesPerUiRefresh = qBound(
        1,
        m_configManager.getValue(QStringLiteral("ui.maxSamplesPerRefresh"), QMetaType::Int).toInt(),
        500);
    m_uiRefreshTimer.setTimerType(Qt::CoarseTimer);
    m_uiRefreshTimer.start(m_uiRefreshIntervalMs);

#if HMI_HAS_QT_CHARTS
    m_maxChartPoints = m_configManager.getValue(QStringLiteral("ui.maxChartPoints"), QMetaType::Int).toInt();
    if (m_axisX) {
        m_axisX->setRange(0, m_maxChartPoints);
    }
#endif

    ui->sampleRateValueLabel->setText(
        tr("%1 ms").arg(m_configManager.getValue(QStringLiteral("simulation.intervalMs"),
                                                 QMetaType::Int).toInt()));
}

void MainWindow::configureInputSource()
{
    QString mode = m_configManager
                       .getValue(QStringLiteral("io.mode"), QMetaType::QString)
                       .toString()
                       .trimmed()
                       .toLower();
    if (!isInputModeAvailable(mode)) {
        const QString requestedMode = mode;
        mode = fallbackInputMode(mode);
        m_configManager.setValue(QStringLiteral("io.mode"), mode);
        appendLogMessage(tr("Input mode %1 is not available in this Qt kit; switched to %2.")
                             .arg(requestedMode, mode));
    }

    if (mode == QStringLiteral("serial")) {
        m_serialManager.setTcpSimulationEnabled(false);
        m_serialManager.setConnectionType(SerialManager::ConnectionType::SerialPort);
        ui->modeValueLabel->setText(tr("Serial"));
        return;
    }

    if (mode == QStringLiteral("tcp")) {
        m_serialManager.setConnectionType(SerialManager::ConnectionType::TcpSocket);
        m_serialManager.setTcpSimulationEnabled(false);
        ui->modeValueLabel->setText(tr("TCP"));
        return;
    }

    if (mode == QStringLiteral("modbus-tcp")) {
        m_serialManager.setTcpSimulationEnabled(false);
        m_serialManager.setConnectionType(SerialManager::ConnectionType::ModbusTcp);
        ui->modeValueLabel->setText(tr("Modbus TCP"));
        return;
    }

    if (mode == QStringLiteral("modbus-rtu")) {
        m_serialManager.setTcpSimulationEnabled(false);
        m_serialManager.setConnectionType(SerialManager::ConnectionType::ModbusRtu);
        ui->modeValueLabel->setText(tr("Modbus RTU"));
        return;
    }

    m_serialManager.setConnectionType(SerialManager::ConnectionType::TcpSocket);
    m_serialManager.setTcpSimulationEnabled(true);
    ui->modeValueLabel->setText(tr("TCP Sim"));
}

bool MainWindow::isInputModeAvailable(const QString &mode) const
{
    const QString normalized = mode.trimmed().toLower();
    if (normalized == QStringLiteral("serial")) {
        return HMI_HAS_QT_SERIALPORT;
    }
    if (normalized == QStringLiteral("modbus-tcp") || normalized == QStringLiteral("modbus-rtu")) {
        return HMI_HAS_QT_SERIALBUS;
    }
    return true;
}

QString MainWindow::fallbackInputMode(const QString &mode) const
{
    Q_UNUSED(mode)
    return QStringLiteral("tcp-sim");
}

void MainWindow::openSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("HMI Settings"));
    dialog.resize(640, 760);

    auto *dialogLayout = new QVBoxLayout(&dialog);
    auto *scrollArea = new QScrollArea(&dialog);
    scrollArea->setWidgetResizable(true);
    auto *settingsContainer = new QWidget(scrollArea);
    auto *settingsLayout = new QVBoxLayout(settingsContainer);
    settingsLayout->setContentsMargins(0, 0, 0, 0);
    settingsLayout->setSpacing(8);
    scrollArea->setWidget(settingsContainer);
    dialogLayout->addWidget(scrollArea, 1);

    auto *sourceGroup = new QGroupBox(tr("Data Source"), &dialog);
    auto *sourceForm = new QFormLayout(sourceGroup);
    auto *modeCombo = new QComboBox(sourceGroup);
    modeCombo->addItem(tr("TCP Simulation"), QStringLiteral("tcp-sim"));
    modeCombo->addItem(tr("TCP Socket"), QStringLiteral("tcp"));
    modeCombo->addItem(tr("Serial Port"), QStringLiteral("serial"));
    modeCombo->addItem(tr("Modbus TCP"), QStringLiteral("modbus-tcp"));
    modeCombo->addItem(tr("Modbus RTU"), QStringLiteral("modbus-rtu"));
    const auto disableModeItem = [modeCombo](const QString &mode, const QString &reason) {
        const int index = modeCombo->findData(mode);
        if (index >= 0) {
            modeCombo->setItemData(index, 0, Qt::UserRole - 1);
            modeCombo->setItemData(index, reason, Qt::ToolTipRole);
        }
    };
    if (!isInputModeAvailable(QStringLiteral("serial"))) {
        disableModeItem(QStringLiteral("serial"), tr("Qt SerialPort module is not available in this Qt kit."));
    }
    if (!isInputModeAvailable(QStringLiteral("modbus-tcp"))) {
        disableModeItem(QStringLiteral("modbus-tcp"), tr("Qt SerialBus module is not available in this Qt kit."));
        disableModeItem(QStringLiteral("modbus-rtu"), tr("Qt SerialBus module is not available in this Qt kit."));
    }
    QString configuredMode = m_configManager.getValue(QStringLiteral("io.mode"),
                                                      QMetaType::QString).toString();
    if (!isInputModeAvailable(configuredMode)) {
        configuredMode = fallbackInputMode(configuredMode);
    }
    modeCombo->setCurrentIndex(qMax(0, modeCombo->findData(configuredMode)));

    auto *serialPortCombo = new QComboBox(sourceGroup);
    serialPortCombo->setEditable(true);
    const QString configuredSerialPort =
        m_configManager.getValue(QStringLiteral("serial.portName"), QMetaType::QString).toString();
    const auto reloadSerialPorts = [this, serialPortCombo, configuredSerialPort]() {
        const QString currentText = serialPortCombo->currentText().trimmed().isEmpty()
                                        ? configuredSerialPort
                                        : serialPortCombo->currentText().trimmed();
        serialPortCombo->clear();
        const QStringList ports = m_serialManager.availableSerialPorts();
        serialPortCombo->addItems(ports);
        if (!ports.contains(QStringLiteral("AUTO"))) {
            serialPortCombo->insertItem(0, QStringLiteral("AUTO"));
        }
        serialPortCombo->setCurrentText(currentText.isEmpty() ? QStringLiteral("AUTO") : currentText);
    };
    reloadSerialPorts();

    auto *serialPortRow = new QWidget(sourceGroup);
    auto *serialPortRowLayout = new QHBoxLayout(serialPortRow);
    serialPortRowLayout->setContentsMargins(0, 0, 0, 0);
    auto *refreshPortsButton = new QPushButton(tr("Refresh"), serialPortRow);
    serialPortRowLayout->addWidget(serialPortCombo, 1);
    serialPortRowLayout->addWidget(refreshPortsButton);
    connect(refreshPortsButton, &QPushButton::clicked, this, [this, reloadSerialPorts]() {
        m_serialManager.refreshSerialPorts();
        reloadSerialPorts();
    });

    auto *baudSpin = new QSpinBox(sourceGroup);
    baudSpin->setRange(1200, 921600);
    baudSpin->setValue(m_configManager.getValue(QStringLiteral("serial.baudRate"), QMetaType::Int).toInt());
    auto *serialReconnectCheck = new QCheckBox(tr("Auto reconnect serial input"), sourceGroup);
    serialReconnectCheck->setChecked(
        m_configManager.getValue(QStringLiteral("serial.autoReconnect"), QMetaType::Bool).toBool());
    auto *serialReconnectIntervalSpin = new QSpinBox(sourceGroup);
    serialReconnectIntervalSpin->setRange(200, 60000);
    serialReconnectIntervalSpin->setSuffix(tr(" ms"));
    serialReconnectIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("serial.reconnectIntervalMs"), QMetaType::Int).toInt());
    auto *tcpHostEdit = new QLineEdit(
        m_configManager.getValue(QStringLiteral("tcp.host"), QMetaType::QString).toString(),
        sourceGroup);
    auto *tcpPortSpin = new QSpinBox(sourceGroup);
    tcpPortSpin->setRange(1, 65535);
    tcpPortSpin->setValue(m_configManager.getValue(QStringLiteral("tcp.port"), QMetaType::Int).toInt());
    auto *simulationIntervalSpin = new QSpinBox(sourceGroup);
    simulationIntervalSpin->setRange(50, 5000);
    simulationIntervalSpin->setSuffix(tr(" ms"));
    simulationIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("simulation.intervalMs"), QMetaType::Int).toInt());

    sourceForm->addRow(tr("Mode"), modeCombo);
    sourceForm->addRow(tr("Serial Port"), serialPortRow);
    sourceForm->addRow(tr("Baud Rate"), baudSpin);
    sourceForm->addRow(serialReconnectCheck);
    sourceForm->addRow(tr("Reconnect Interval"), serialReconnectIntervalSpin);
    sourceForm->addRow(tr("TCP Host"), tcpHostEdit);
    sourceForm->addRow(tr("TCP Port"), tcpPortSpin);
    sourceForm->addRow(tr("Simulation Interval"), simulationIntervalSpin);

    auto *protocolGroup = new QGroupBox(tr("Protocol Framing"), &dialog);
    auto *protocolForm = new QFormLayout(protocolGroup);
    auto *framingModeCombo = new QComboBox(protocolGroup);
    framingModeCombo->addItem(tr("Raw Chunk"), QStringLiteral("raw"));
    framingModeCombo->addItem(tr("Line Delimited"), QStringLiteral("line"));
    framingModeCombo->addItem(tr("Custom Delimiter"), QStringLiteral("delimiter"));
    framingModeCombo->addItem(tr("Fixed Length"), QStringLiteral("fixed"));
    framingModeCombo->setCurrentIndex(qMax(0, framingModeCombo->findData(
                                               m_configManager.getValue(QStringLiteral("protocol.framingMode"),
                                                                        QMetaType::QString).toString())));
    auto *delimiterEdit = new QLineEdit(
        m_configManager.getValue(QStringLiteral("protocol.delimiter"), QMetaType::QString).toString(),
        protocolGroup);
    delimiterEdit->setPlaceholderText(QStringLiteral("\\n, \\r\\n, |, \\x03"));
    auto *fixedLengthSpin = new QSpinBox(protocolGroup);
    fixedLengthSpin->setRange(1, 65535);
    fixedLengthSpin->setValue(
        m_configManager.getValue(QStringLiteral("protocol.fixedLength"), QMetaType::Int).toInt());
    auto *maxFrameBytesSpin = new QSpinBox(protocolGroup);
    maxFrameBytesSpin->setRange(128, 1024 * 1024);
    maxFrameBytesSpin->setValue(
        m_configManager.getValue(QStringLiteral("protocol.maxFrameBytes"), QMetaType::Int).toInt());

    protocolForm->addRow(tr("Frame Mode"), framingModeCombo);
    protocolForm->addRow(tr("Delimiter"), delimiterEdit);
    protocolForm->addRow(tr("Fixed Length"), fixedLengthSpin);
    protocolForm->addRow(tr("Max Frame Bytes"), maxFrameBytesSpin);

    auto *modbusGroup = new QGroupBox(tr("Modbus"), &dialog);
    auto *modbusForm = new QFormLayout(modbusGroup);
    auto *modbusTcpHostEdit = new QLineEdit(
        m_configManager.getValue(QStringLiteral("modbus.tcp.host"), QMetaType::QString).toString(),
        modbusGroup);
    auto *modbusTcpPortSpin = new QSpinBox(modbusGroup);
    modbusTcpPortSpin->setRange(1, 65535);
    modbusTcpPortSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.tcp.port"), QMetaType::Int).toInt());
    auto *modbusRtuPortCombo = new QComboBox(modbusGroup);
    modbusRtuPortCombo->setEditable(true);
    modbusRtuPortCombo->addItems(m_serialManager.availableSerialPorts());
    modbusRtuPortCombo->setCurrentText(
        m_configManager.getValue(QStringLiteral("modbus.rtu.portName"), QMetaType::QString).toString());
    auto *modbusRtuBaudSpin = new QSpinBox(modbusGroup);
    modbusRtuBaudSpin->setRange(1200, 921600);
    modbusRtuBaudSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.rtu.baudRate"), QMetaType::Int).toInt());
    auto *modbusUnitSpin = new QSpinBox(modbusGroup);
    modbusUnitSpin->setRange(1, 247);
    modbusUnitSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.unitId"), QMetaType::Int).toInt());
    auto *modbusStartAddressSpin = new QSpinBox(modbusGroup);
    modbusStartAddressSpin->setRange(0, 65535);
    modbusStartAddressSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.startAddress"), QMetaType::Int).toInt());
    auto *modbusRegisterCountSpin = new QSpinBox(modbusGroup);
    modbusRegisterCountSpin->setRange(1, 125);
    modbusRegisterCountSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.registerCount"), QMetaType::Int).toInt());
    auto *modbusPollIntervalSpin = new QSpinBox(modbusGroup);
    modbusPollIntervalSpin->setRange(100, 60000);
    modbusPollIntervalSpin->setSuffix(tr(" ms"));
    modbusPollIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.pollIntervalMs"), QMetaType::Int).toInt());
    auto *modbusTimeoutSpin = new QSpinBox(modbusGroup);
    modbusTimeoutSpin->setRange(100, 60000);
    modbusTimeoutSpin->setSuffix(tr(" ms"));
    modbusTimeoutSpin->setValue(
        m_configManager.getValue(QStringLiteral("modbus.timeoutMs"), QMetaType::Int).toInt());

    modbusForm->addRow(tr("TCP Host"), modbusTcpHostEdit);
    modbusForm->addRow(tr("TCP Port"), modbusTcpPortSpin);
    modbusForm->addRow(tr("RTU Port"), modbusRtuPortCombo);
    modbusForm->addRow(tr("RTU Baud Rate"), modbusRtuBaudSpin);
    modbusForm->addRow(tr("Unit ID"), modbusUnitSpin);
    modbusForm->addRow(tr("Start Address"), modbusStartAddressSpin);
    modbusForm->addRow(tr("Register Count"), modbusRegisterCountSpin);
    modbusForm->addRow(tr("Poll Interval"), modbusPollIntervalSpin);
    modbusForm->addRow(tr("Timeout"), modbusTimeoutSpin);

    auto *processingGroup = new QGroupBox(tr("Processing"), &dialog);
    auto *processingForm = new QFormLayout(processingGroup);
    auto *processingModeCombo = new QComboBox(processingGroup);
    processingModeCombo->addItem(tr("Pass Through"), QStringLiteral("pass-through"));
    processingModeCombo->addItem(tr("Scale + Offset"), QStringLiteral("scale-offset"));
    processingModeCombo->addItem(tr("Low-pass Filter"), QStringLiteral("low-pass"));
    processingModeCombo->setCurrentIndex(qMax(0, processingModeCombo->findData(
                                                  m_configManager.getValue(QStringLiteral("worker.processingMode"),
                                                                           QMetaType::QString).toString())));
    auto *workerIntervalSpin = new QSpinBox(processingGroup);
    workerIntervalSpin->setRange(20, 5000);
    workerIntervalSpin->setSuffix(tr(" ms"));
    workerIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("worker.intervalMs"), QMetaType::Int).toInt());
    auto *workerScaleSpin = new QDoubleSpinBox(processingGroup);
    workerScaleSpin->setRange(-100000.0, 100000.0);
    workerScaleSpin->setDecimals(6);
    workerScaleSpin->setValue(
        m_configManager.getValue(QStringLiteral("worker.scale"), QMetaType::Double).toDouble());
    auto *workerOffsetSpin = new QDoubleSpinBox(processingGroup);
    workerOffsetSpin->setRange(-100000.0, 100000.0);
    workerOffsetSpin->setDecimals(6);
    workerOffsetSpin->setValue(
        m_configManager.getValue(QStringLiteral("worker.offset"), QMetaType::Double).toDouble());
    auto *lowPassAlphaSpin = new QDoubleSpinBox(processingGroup);
    lowPassAlphaSpin->setRange(0.01, 1.0);
    lowPassAlphaSpin->setDecimals(3);
    lowPassAlphaSpin->setSingleStep(0.05);
    lowPassAlphaSpin->setValue(
        m_configManager.getValue(QStringLiteral("worker.lowPassAlpha"), QMetaType::Double).toDouble());

    processingForm->addRow(tr("Mode"), processingModeCombo);
    processingForm->addRow(tr("Worker Interval"), workerIntervalSpin);
    processingForm->addRow(tr("Scale"), workerScaleSpin);
    processingForm->addRow(tr("Offset"), workerOffsetSpin);
    processingForm->addRow(tr("Low-pass Alpha"), lowPassAlphaSpin);

    auto *alarmGroup = new QGroupBox(tr("Alarm Limits"), &dialog);
    auto *alarmForm = new QFormLayout(alarmGroup);
    auto makeLimitSpin = [](double value) {
        auto *spin = new QDoubleSpinBox();
        spin->setRange(-100000.0, 100000.0);
        spin->setDecimals(3);
        spin->setValue(value);
        return spin;
    };
    auto *temperatureLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.temperatureHigh"), QMetaType::Double).toDouble());
    auto *pressureLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.pressureHigh"), QMetaType::Double).toDouble());
    auto *flowLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.flowHigh"), QMetaType::Double).toDouble());
    auto *temperatureLowLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.temperatureLow"), QMetaType::Double).toDouble());
    auto *pressureLowLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.pressureLow"), QMetaType::Double).toDouble());
    auto *flowLowLimitSpin = makeLimitSpin(
        m_configManager.getValue(QStringLiteral("alarm.flowLow"), QMetaType::Double).toDouble());
    auto *alarmHysteresisSpin = new QDoubleSpinBox(alarmGroup);
    alarmHysteresisSpin->setRange(0.0, 100000.0);
    alarmHysteresisSpin->setDecimals(3);
    alarmHysteresisSpin->setValue(
        m_configManager.getValue(QStringLiteral("alarm.hysteresis"), QMetaType::Double).toDouble());
    auto *alarmLatchCheck = new QCheckBox(tr("Latch alarms until acknowledged"), alarmGroup);
    alarmLatchCheck->setChecked(
        m_configManager.getValue(QStringLiteral("alarm.latchingEnabled"), QMetaType::Bool).toBool());
    auto *alarmHistoryLimitSpin = new QSpinBox(alarmGroup);
    alarmHistoryLimitSpin->setRange(10, 100000);
    alarmHistoryLimitSpin->setValue(
        m_configManager.getValue(QStringLiteral("alarm.maxHistoryRecords"), QMetaType::Int).toInt());
    alarmForm->addRow(tr("Temperature High"), temperatureLimitSpin);
    alarmForm->addRow(tr("Temperature Low"), temperatureLowLimitSpin);
    alarmForm->addRow(tr("Pressure High"), pressureLimitSpin);
    alarmForm->addRow(tr("Pressure Low"), pressureLowLimitSpin);
    alarmForm->addRow(tr("Flow High"), flowLimitSpin);
    alarmForm->addRow(tr("Flow Low"), flowLowLimitSpin);
    alarmForm->addRow(tr("Hysteresis"), alarmHysteresisSpin);
    alarmForm->addRow(alarmLatchCheck);
    alarmForm->addRow(tr("History Limit"), alarmHistoryLimitSpin);

    auto *loggingGroup = new QGroupBox(tr("Logging And UI"), &dialog);
    auto *loggingForm = new QFormLayout(loggingGroup);
    auto *loggingEnabledCheck = new QCheckBox(tr("Enable CSV data logging"), loggingGroup);
    loggingEnabledCheck->setChecked(
        m_configManager.getValue(QStringLiteral("logging.enabled"), QMetaType::Bool).toBool());
    auto *loggingDirectoryEdit = new QLineEdit(
        m_configManager.getValue(QStringLiteral("logging.directory"), QMetaType::QString).toString(),
        loggingGroup);
    auto *historyRowsSpin = new QSpinBox(loggingGroup);
    historyRowsSpin->setRange(10, 10000);
    historyRowsSpin->setValue(
        m_configManager.getValue(QStringLiteral("ui.maxHistoryRows"), QMetaType::Int).toInt());
    auto *chartPointsSpin = new QSpinBox(loggingGroup);
    chartPointsSpin->setRange(20, 5000);
    chartPointsSpin->setValue(
        m_configManager.getValue(QStringLiteral("ui.maxChartPoints"), QMetaType::Int).toInt());
    auto *uiRefreshIntervalSpin = new QSpinBox(loggingGroup);
    uiRefreshIntervalSpin->setRange(50, 5000);
    uiRefreshIntervalSpin->setSuffix(tr(" ms"));
    uiRefreshIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("ui.refreshIntervalMs"), QMetaType::Int).toInt());
    auto *samplesPerRefreshSpin = new QSpinBox(loggingGroup);
    samplesPerRefreshSpin->setRange(1, 500);
    samplesPerRefreshSpin->setValue(
        m_configManager.getValue(QStringLiteral("ui.maxSamplesPerRefresh"), QMetaType::Int).toInt());
    auto *logFlushIntervalSpin = new QSpinBox(loggingGroup);
    logFlushIntervalSpin->setRange(100, 60000);
    logFlushIntervalSpin->setSuffix(tr(" ms"));
    logFlushIntervalSpin->setValue(
        m_configManager.getValue(QStringLiteral("logging.flushIntervalMs"), QMetaType::Int).toInt());

    loggingForm->addRow(loggingEnabledCheck);
    loggingForm->addRow(tr("CSV Directory"), loggingDirectoryEdit);
    loggingForm->addRow(tr("CSV Flush Interval"), logFlushIntervalSpin);
    loggingForm->addRow(tr("History Rows"), historyRowsSpin);
    loggingForm->addRow(tr("Chart Points"), chartPointsSpin);
    loggingForm->addRow(tr("UI Refresh Interval"), uiRefreshIntervalSpin);
    loggingForm->addRow(tr("Samples Per Refresh"), samplesPerRefreshSpin);

    settingsLayout->addWidget(sourceGroup);
    settingsLayout->addWidget(protocolGroup);
    settingsLayout->addWidget(modbusGroup);
    settingsLayout->addWidget(processingGroup);
    settingsLayout->addWidget(alarmGroup);
    settingsLayout->addWidget(loggingGroup);
    settingsLayout->addStretch(1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialogLayout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const bool wasRunning = m_serialManager.isRunning();
    if (wasRunning) {
        handleStopClicked();
    }

    m_configManager.setValue(QStringLiteral("io.mode"), modeCombo->currentData().toString());
    m_configManager.setValue(QStringLiteral("serial.portName"), serialPortCombo->currentText().trimmed());
    m_configManager.setValue(QStringLiteral("serial.baudRate"), baudSpin->value());
    m_configManager.setValue(QStringLiteral("serial.autoReconnect"), serialReconnectCheck->isChecked());
    m_configManager.setValue(QStringLiteral("serial.reconnectIntervalMs"), serialReconnectIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("tcp.host"), tcpHostEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("tcp.port"), tcpPortSpin->value());
    m_configManager.setValue(QStringLiteral("simulation.intervalMs"), simulationIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("protocol.framingMode"), framingModeCombo->currentData().toString());
    m_configManager.setValue(QStringLiteral("protocol.delimiter"), delimiterEdit->text());
    m_configManager.setValue(QStringLiteral("protocol.fixedLength"), fixedLengthSpin->value());
    m_configManager.setValue(QStringLiteral("protocol.maxFrameBytes"), maxFrameBytesSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.tcp.host"), modbusTcpHostEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("modbus.tcp.port"), modbusTcpPortSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.rtu.portName"), modbusRtuPortCombo->currentText().trimmed());
    m_configManager.setValue(QStringLiteral("modbus.rtu.baudRate"), modbusRtuBaudSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.unitId"), modbusUnitSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.startAddress"), modbusStartAddressSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.registerCount"), modbusRegisterCountSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.pollIntervalMs"), modbusPollIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("modbus.timeoutMs"), modbusTimeoutSpin->value());
    m_configManager.setValue(QStringLiteral("worker.processingMode"), processingModeCombo->currentData().toString());
    m_configManager.setValue(QStringLiteral("worker.intervalMs"), workerIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("worker.scale"), workerScaleSpin->value());
    m_configManager.setValue(QStringLiteral("worker.offset"), workerOffsetSpin->value());
    m_configManager.setValue(QStringLiteral("worker.lowPassAlpha"), lowPassAlphaSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.temperatureHigh"), temperatureLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.temperatureLow"), temperatureLowLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.pressureHigh"), pressureLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.pressureLow"), pressureLowLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.flowHigh"), flowLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.flowLow"), flowLowLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.hysteresis"), alarmHysteresisSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.latchingEnabled"), alarmLatchCheck->isChecked());
    m_configManager.setValue(QStringLiteral("alarm.maxHistoryRecords"), alarmHistoryLimitSpin->value());
    m_configManager.setValue(QStringLiteral("logging.enabled"), loggingEnabledCheck->isChecked());
    m_configManager.setValue(QStringLiteral("logging.directory"), loggingDirectoryEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("logging.flushIntervalMs"), logFlushIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("ui.maxHistoryRows"), historyRowsSpin->value());
    m_configManager.setValue(QStringLiteral("ui.maxChartPoints"), chartPointsSpin->value());
    m_configManager.setValue(QStringLiteral("ui.refreshIntervalMs"), uiRefreshIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("ui.maxSamplesPerRefresh"), samplesPerRefreshSpin->value());

    applyConfiguration();
    saveConfiguration();
    appendLogMessage(tr("Settings saved"));
}

void MainWindow::setupRuntimeUiExtensions()
{
    auto *extensionLayout = new QVBoxLayout(ui->extensionArea);
    extensionLayout->setContentsMargins(0, 8, 0, 0);
    extensionLayout->setSpacing(8);

    setupChart();
    setupHistoryTable();
    setupAlarmControls();
    setupAlarmHistoryTable();
    setupLogView();
    setupLedIndicator();
}

void MainWindow::setupChart()
{
#if HMI_HAS_QT_CHARTS
    m_temperatureSeries = new QLineSeries(this);
    m_pressureSeries = new QLineSeries(this);
    m_flowSeries = new QLineSeries(this);

    m_temperatureSeries->setName(tr("Temperature"));
    m_pressureSeries->setName(tr("Pressure"));
    m_flowSeries->setName(tr("Flow"));

    m_chart = new QChart();
    m_chart->addSeries(m_temperatureSeries);
    m_chart->addSeries(m_pressureSeries);
    m_chart->addSeries(m_flowSeries);
    m_chart->legend()->setVisible(true);
    m_chart->setTitle(tr("Realtime Processed Data"));

    m_axisX = new QValueAxis(this);
    m_axisY = new QValueAxis(this);
    m_axisX->setRange(0, m_maxChartPoints);
    m_axisX->setLabelFormat("%d");
    m_axisY->setRange(0, 120);
    m_axisY->setLabelFormat("%.1f");

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);
    m_temperatureSeries->attachAxis(m_axisX);
    m_temperatureSeries->attachAxis(m_axisY);
    m_pressureSeries->attachAxis(m_axisX);
    m_pressureSeries->attachAxis(m_axisY);
    m_flowSeries->attachAxis(m_axisX);
    m_flowSeries->attachAxis(m_axisY);

    m_chartView = new QChartView(m_chart, ui->extensionArea);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(220);
    ui->extensionArea->layout()->addWidget(m_chartView);
#else
    auto *chartFallback = new QLabel(tr("Qt Charts module is not available."), ui->extensionArea);
    chartFallback->setMinimumHeight(80);
    chartFallback->setAlignment(Qt::AlignCenter);
    ui->extensionArea->layout()->addWidget(chartFallback);
#endif
}

void MainWindow::setupHistoryTable()
{
    auto *tableView = new QTableView(ui->extensionArea);
    m_historyModel = new QStandardItemModel(this);
    m_historyModel->setHorizontalHeaderLabels({
        tr("Time"),
        tr("Temperature"),
        tr("Pressure"),
        tr("Flow")
    });

    tableView->setModel(m_historyModel);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setAlternatingRowColors(true);
    tableView->setMinimumHeight(140);

    ui->extensionArea->layout()->addWidget(tableView);
}

void MainWindow::setupAlarmControls()
{
    auto *container = new QWidget(ui->extensionArea);
    auto *layout = new QHBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    m_alarmAckButton = new QPushButton(tr("Acknowledge Alarm"), container);
    m_alarmSilenceButton = new QPushButton(tr("Silence Alarm"), container);
    m_alarmExportButton = new QPushButton(tr("Export History"), container);
    m_alarmClearButton = new QPushButton(tr("Clear History"), container);

    layout->addWidget(m_alarmAckButton);
    layout->addWidget(m_alarmSilenceButton);
    layout->addWidget(m_alarmExportButton);
    layout->addWidget(m_alarmClearButton);
    layout->addStretch(1);

    ui->extensionArea->layout()->addWidget(container);
}

void MainWindow::setupAlarmHistoryTable()
{
    auto *tableView = new QTableView(ui->extensionArea);
    m_alarmHistoryModel = new QStandardItemModel(this);
    m_alarmHistoryModel->setHorizontalHeaderLabels({
        tr("Time"),
        tr("State"),
        tr("Ack"),
        tr("Silenced"),
        tr("Latched"),
        tr("Severity"),
        tr("Channel"),
        tr("Value"),
        tr("Limit"),
        tr("Message")
    });

    tableView->setModel(m_alarmHistoryModel);
    tableView->horizontalHeader()->setStretchLastSection(true);
    tableView->verticalHeader()->setVisible(false);
    tableView->setAlternatingRowColors(true);
    tableView->setMinimumHeight(110);

    ui->extensionArea->layout()->addWidget(tableView);
}

void MainWindow::setupLogView()
{
    m_logView = new QTextEdit(ui->extensionArea);
    m_logView->setReadOnly(true);
    m_logView->document()->setMaximumBlockCount(300);
    m_logView->setMinimumHeight(100);

    ui->extensionArea->layout()->addWidget(m_logView);
}

void MainWindow::setupLedIndicator()
{
    m_ledIndicator = ui->statusLedLabel;
    m_ledIndicator->setFixedSize(18, 18);
    m_ledIndicator->setToolTip(tr("Green: normal, Red: alarm"));

    updateAlarmUi(m_alarmManager.currentState());
}

void MainWindow::updateValueCards(const QVector<double> &values)
{
    const auto valueText = [&values](int index) {
        return index < values.size() ? QString::number(values.at(index), 'f', 3)
                                     : QStringLiteral("--");
    };

    ui->temperatureValueLabel->setText(valueText(0));
    ui->pressureValueLabel->setText(valueText(1));
    ui->flowValueLabel->setText(valueText(2));
    ui->lastUpdateValueLabel->setText(
        values.isEmpty() ? QStringLiteral("--")
                         : QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
}

void MainWindow::updateHeaderState(const QString &connectionState)
{
    ui->connectionValueLabel->setText(connectionState);
}

void MainWindow::appendHistoryRow(const QVector<double> &values)
{
    if (!m_historyModel) {
        return;
    }

    QList<QStandardItem *> row;
    row << new QStandardItem(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")));

    for (int i = 0; i < 3; ++i) {
        const QString text = i < values.size() ? QString::number(values.at(i), 'f', 3)
                                               : QStringLiteral("-");
        row << new QStandardItem(text);
    }

    m_historyModel->appendRow(row);

    while (m_historyModel->rowCount() > m_maxHistoryRows) {
        m_historyModel->removeRow(0);
    }
}

void MainWindow::appendHistoryRows(const QVector<QVector<double>> &samples)
{
    for (const QVector<double> &values : samples) {
        appendHistoryRow(values);
    }
}

void MainWindow::appendLogMessage(const QString &message)
{
    if (!m_logView) {
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));
    m_logView->append(QStringLiteral("[%1] %2").arg(timestamp, message));
}

void MainWindow::updateChart(const QVector<double> &values)
{
#if HMI_HAS_QT_CHARTS
    if (!m_temperatureSeries || values.isEmpty()) {
        return;
    }

    const qreal x = m_sampleIndex++;
    if (values.size() > 0) {
        m_temperatureSeries->append(x, values.at(0));
    }
    if (values.size() > 1) {
        m_pressureSeries->append(x, values.at(1));
    }
    if (values.size() > 2) {
        m_flowSeries->append(x, values.at(2));
    }

    auto trimSeries = [this](QLineSeries *series) {
        while (series && series->count() > m_maxChartPoints) {
            series->remove(0);
        }
    };

    trimSeries(m_temperatureSeries);
    trimSeries(m_pressureSeries);
    trimSeries(m_flowSeries);

    const qreal minX = qMax<qreal>(0, x - m_maxChartPoints + 1);
    m_axisX->setRange(minX, qMax<qreal>(m_maxChartPoints, x));
#else
    Q_UNUSED(values)
#endif
}

void MainWindow::updateChartBatch(const QVector<QVector<double>> &samples)
{
#if HMI_HAS_QT_CHARTS
    if (!m_temperatureSeries || samples.isEmpty()) {
        return;
    }

    QList<QPointF> temperaturePoints;
    QList<QPointF> pressurePoints;
    QList<QPointF> flowPoints;
    temperaturePoints.reserve(samples.size());
    pressurePoints.reserve(samples.size());
    flowPoints.reserve(samples.size());

    qreal lastX = m_sampleIndex;
    for (const QVector<double> &values : samples) {
        const qreal x = m_sampleIndex++;
        lastX = x;
        if (values.size() > 0) {
            temperaturePoints.append(QPointF(x, values.at(0)));
        }
        if (values.size() > 1) {
            pressurePoints.append(QPointF(x, values.at(1)));
        }
        if (values.size() > 2) {
            flowPoints.append(QPointF(x, values.at(2)));
        }
    }

    m_temperatureSeries->append(temperaturePoints);
    m_pressureSeries->append(pressurePoints);
    m_flowSeries->append(flowPoints);

    auto trimSeries = [this](QLineSeries *series) {
        while (series && series->count() > m_maxChartPoints) {
            series->remove(0);
        }
    };

    trimSeries(m_temperatureSeries);
    trimSeries(m_pressureSeries);
    trimSeries(m_flowSeries);

    const qreal minX = qMax<qreal>(0, lastX - m_maxChartPoints + 1);
    m_axisX->setRange(minX, qMax<qreal>(m_maxChartPoints, lastX));
#else
    Q_UNUSED(samples)
#endif
}

void MainWindow::updateAlarmUi(const AlarmManager::AlarmState &alarmState)
{
    if (!m_ledIndicator) {
        return;
    }

    const QString color = alarmState.active ? QStringLiteral("#D64545")
                                            : QStringLiteral("#2FA84F");

    QString alarmText = tr("Normal");
    if (alarmState.active && alarmState.latched) {
        alarmText = tr("Latched");
    } else if (alarmState.active && alarmState.silenced) {
        alarmText = tr("Silenced");
    } else if (alarmState.active && alarmState.acknowledged) {
        alarmText = tr("Acknowledged");
    } else if (alarmState.active) {
        alarmText = tr("Alarm");
    }

    ui->alarmValueLabel->setText(alarmText);
    ui->alarmValueLabel->setToolTip(alarmState.message);
    ui->alarmValueLabel->setStyleSheet(alarmState.active
                                           ? QStringLiteral("color: #F5B7B1; font-weight: 700;")
                                           : QStringLiteral("color: #ABEBC6; font-weight: 700;"));
    m_ledIndicator->setStyleSheet(QStringLiteral(
        "border-radius: 9px;"
        "border: 1px solid #555;"
        "background-color: %1;").arg(color));

    if (m_alarmAckButton) {
        m_alarmAckButton->setEnabled(alarmState.active && !alarmState.acknowledged);
    }

    if (m_alarmSilenceButton) {
        m_alarmSilenceButton->setEnabled(alarmState.active || m_alarmManager.isSilenced());
        m_alarmSilenceButton->setText(m_alarmManager.isSilenced()
                                          ? tr("Enable Alarm Sound")
                                          : tr("Silence Alarm"));
    }
}

void MainWindow::refreshAlarmHistoryTable()
{
    if (!m_alarmHistoryModel) {
        return;
    }

    m_alarmHistoryModel->removeRows(0, m_alarmHistoryModel->rowCount());

    const QVector<AlarmManager::AlarmRecord> records = m_alarmManager.history();
    for (const AlarmManager::AlarmRecord &record : records) {
        QList<QStandardItem *> row;
        row << new QStandardItem(record.timestamp.toString(QStringLiteral("HH:mm:ss.zzz")));
        row << new QStandardItem(record.active ? tr("Active") : tr("Cleared"));
        row << new QStandardItem(record.acknowledged ? tr("Yes") : tr("No"));
        row << new QStandardItem(record.silenced ? tr("Yes") : tr("No"));
        row << new QStandardItem(record.latched ? tr("Yes") : tr("No"));
        row << new QStandardItem(QString::number(record.severity));
        row << new QStandardItem(record.channel);
        row << new QStandardItem(QString::number(record.value, 'f', 3));
        row << new QStandardItem(QString::number(record.limit, 'f', 3));
        row << new QStandardItem(record.message);
        m_alarmHistoryModel->appendRow(row);
    }
}

QString MainWindow::formatValues(const QVector<double> &values) const
{
    QStringList textValues;
    textValues.reserve(values.size());

    for (double value : values) {
        textValues.append(QString::number(value, 'f', 3));
    }

    return textValues.join(QStringLiteral(", "));
}

QString MainWindow::configFilePath() const
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("config/hmi_config.json"));
}

QString MainWindow::defaultDataLogDirectory() const
{
    return QDir(QCoreApplication::applicationDirPath())
        .filePath(QStringLiteral("data_logs"));
}

void MainWindow::setDefaultValue(const QString &key, const QVariant &value)
{
    if (!m_configManager.contains(key)) {
        m_configManager.setValue(key, value);
    }
}
