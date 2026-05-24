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
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileInfo>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableView>
#include <QVBoxLayout>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupRuntimeUiExtensions();
    initializeModules();
    connectSignals();

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

    if (!m_workerThread.isRunning()) {
        m_workerThread.start();
    }

    if (m_serialManager.start()) {
        if (m_loggingEnabled) {
            m_dataLogger.startSession();
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

void MainWindow::handleProcessedData(const QVector<double> &values)
{
    const QString valuesText = formatValues(values);
    const AlarmManager::AlarmState alarmState = m_alarmManager.evaluate(values);

    ui->dataView->setPlainText(tr("Processed values:\n%1").arg(valuesText));
    updateValueCards(values);
    appendHistoryRow(values);
    updateChart(values);
    updateAlarmUi(alarmState);
    if (m_loggingEnabled) {
        m_dataLogger.logSample(values, alarmState.active, alarmState.message);
    }
    appendLogMessage(tr("Processed values: %1").arg(valuesText));

    qDebug().noquote() << "[Step2][MainWindow] UI updated with processed data =" << valuesText;
}

void MainWindow::handleStatusMessage(const QString &message)
{
    ui->statusbar->showMessage(message);
    appendLogMessage(message);
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
}

void MainWindow::connectSignals()
{
    connect(ui->startButton, &QPushButton::clicked,
            this, &MainWindow::handleStartClicked);
    connect(ui->stopButton, &QPushButton::clicked,
            this, &MainWindow::handleStopClicked);
    connect(ui->settingsButton, &QPushButton::clicked,
            this, &MainWindow::handleSettingsClicked);

    connect(&m_serialManager, &SerialManager::dataReceived,
            &m_dataProcessor, &DataProcessor::processRawData);
    connect(&m_serialManager, &SerialManager::errorOccurred,
            this, &MainWindow::handleStatusMessage);
    connect(&m_dataProcessor, &DataProcessor::dataUpdated,
            &m_workerThread, &WorkerThread::setLatestData);
    connect(&m_workerThread, &WorkerThread::dataProcessed,
            this, &MainWindow::handleProcessedData);
    connect(&m_alarmManager, &AlarmManager::alarmStateChanged,
            this, [this](bool active, const QString &message) {
                appendLogMessage(active ? tr("Alarm active: %1").arg(message)
                                        : tr("Alarm cleared"));
            });
    connect(&m_dataLogger, &DataLogger::logMessage,
            this, &MainWindow::appendLogMessage);
    connect(&m_dataLogger, &DataLogger::errorOccurred,
            this, &MainWindow::handleStatusMessage);
}

void MainWindow::ensureDefaultConfiguration()
{
    setDefaultValue(QStringLiteral("io.mode"), QStringLiteral("tcp-sim"));
    setDefaultValue(QStringLiteral("serial.portName"), QStringLiteral("COM1"));
    setDefaultValue(QStringLiteral("serial.baudRate"), 9600);
    setDefaultValue(QStringLiteral("tcp.host"), QStringLiteral("127.0.0.1"));
    setDefaultValue(QStringLiteral("tcp.port"), 502);
    setDefaultValue(QStringLiteral("simulation.intervalMs"), 250);
    setDefaultValue(QStringLiteral("worker.intervalMs"), 200);
    setDefaultValue(QStringLiteral("alarm.temperatureHigh"), 32.0);
    setDefaultValue(QStringLiteral("alarm.pressureHigh"), 108.0);
    setDefaultValue(QStringLiteral("alarm.flowHigh"), 70.0);
    setDefaultValue(QStringLiteral("logging.enabled"), true);
    setDefaultValue(QStringLiteral("logging.directory"), defaultDataLogDirectory());
    setDefaultValue(QStringLiteral("ui.maxHistoryRows"), 200);
    setDefaultValue(QStringLiteral("ui.maxChartPoints"), 120);
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
    m_serialManager.setTcpEndpoint(
        m_configManager.getValue(QStringLiteral("tcp.host"), QMetaType::QString).toString(),
        static_cast<quint16>(m_configManager.getValue(QStringLiteral("tcp.port"), QMetaType::Int).toUInt()));
    m_serialManager.setSimulationIntervalMs(
        m_configManager.getValue(QStringLiteral("simulation.intervalMs"), QMetaType::Int).toInt());

    m_workerThread.setProcessor(&m_dataProcessor);
    m_workerThread.setIntervalMs(
        m_configManager.getValue(QStringLiteral("worker.intervalMs"), QMetaType::Int).toInt());

    m_alarmManager.setTemperatureHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.temperatureHigh"), QMetaType::Double).toDouble());
    m_alarmManager.setPressureHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.pressureHigh"), QMetaType::Double).toDouble());
    m_alarmManager.setFlowHighLimit(
        m_configManager.getValue(QStringLiteral("alarm.flowHigh"), QMetaType::Double).toDouble());

    m_loggingEnabled = m_configManager.getValue(QStringLiteral("logging.enabled"), QMetaType::Bool).toBool();
    m_dataLogger.setOutputDirectory(
        m_configManager.getValue(QStringLiteral("logging.directory"), QMetaType::QString).toString());
    m_maxHistoryRows = m_configManager.getValue(QStringLiteral("ui.maxHistoryRows"), QMetaType::Int).toInt();

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
    const QString mode = m_configManager
                             .getValue(QStringLiteral("io.mode"), QMetaType::QString)
                             .toString()
                             .trimmed()
                             .toLower();

    if (mode == QStringLiteral("serial")) {
        m_serialManager.setTcpSimulationEnabled(false);
        m_serialManager.setConnectionType(SerialManager::ConnectionType::SerialPort);
        ui->modeValueLabel->setText(tr("Serial"));
        return;
    }

    m_serialManager.setConnectionType(SerialManager::ConnectionType::TcpSocket);
    m_serialManager.setTcpSimulationEnabled(true);
    ui->modeValueLabel->setText(tr("TCP Sim"));
}

void MainWindow::openSettingsDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("HMI Settings"));
    dialog.resize(480, 520);

    auto *dialogLayout = new QVBoxLayout(&dialog);

    auto *sourceGroup = new QGroupBox(tr("Data Source"), &dialog);
    auto *sourceForm = new QFormLayout(sourceGroup);
    auto *modeCombo = new QComboBox(sourceGroup);
    modeCombo->addItem(tr("TCP Simulation"), QStringLiteral("tcp-sim"));
    modeCombo->addItem(tr("TCP Socket"), QStringLiteral("tcp"));
    modeCombo->addItem(tr("Serial Port"), QStringLiteral("serial"));
    modeCombo->setCurrentIndex(qMax(0, modeCombo->findData(
                                        m_configManager.getValue(QStringLiteral("io.mode"),
                                                                 QMetaType::QString).toString())));

    auto *serialPortEdit = new QLineEdit(
        m_configManager.getValue(QStringLiteral("serial.portName"), QMetaType::QString).toString(),
        sourceGroup);
    auto *baudSpin = new QSpinBox(sourceGroup);
    baudSpin->setRange(1200, 921600);
    baudSpin->setValue(m_configManager.getValue(QStringLiteral("serial.baudRate"), QMetaType::Int).toInt());
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
    sourceForm->addRow(tr("Serial Port"), serialPortEdit);
    sourceForm->addRow(tr("Baud Rate"), baudSpin);
    sourceForm->addRow(tr("TCP Host"), tcpHostEdit);
    sourceForm->addRow(tr("TCP Port"), tcpPortSpin);
    sourceForm->addRow(tr("Simulation Interval"), simulationIntervalSpin);

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
    alarmForm->addRow(tr("Temperature High"), temperatureLimitSpin);
    alarmForm->addRow(tr("Pressure High"), pressureLimitSpin);
    alarmForm->addRow(tr("Flow High"), flowLimitSpin);

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

    loggingForm->addRow(loggingEnabledCheck);
    loggingForm->addRow(tr("CSV Directory"), loggingDirectoryEdit);
    loggingForm->addRow(tr("History Rows"), historyRowsSpin);
    loggingForm->addRow(tr("Chart Points"), chartPointsSpin);

    dialogLayout->addWidget(sourceGroup);
    dialogLayout->addWidget(alarmGroup);
    dialogLayout->addWidget(loggingGroup);

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
    m_configManager.setValue(QStringLiteral("serial.portName"), serialPortEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("serial.baudRate"), baudSpin->value());
    m_configManager.setValue(QStringLiteral("tcp.host"), tcpHostEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("tcp.port"), tcpPortSpin->value());
    m_configManager.setValue(QStringLiteral("simulation.intervalMs"), simulationIntervalSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.temperatureHigh"), temperatureLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.pressureHigh"), pressureLimitSpin->value());
    m_configManager.setValue(QStringLiteral("alarm.flowHigh"), flowLimitSpin->value());
    m_configManager.setValue(QStringLiteral("logging.enabled"), loggingEnabledCheck->isChecked());
    m_configManager.setValue(QStringLiteral("logging.directory"), loggingDirectoryEdit->text().trimmed());
    m_configManager.setValue(QStringLiteral("ui.maxHistoryRows"), historyRowsSpin->value());
    m_configManager.setValue(QStringLiteral("ui.maxChartPoints"), chartPointsSpin->value());

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

void MainWindow::updateAlarmUi(const AlarmManager::AlarmState &alarmState)
{
    if (!m_ledIndicator) {
        return;
    }

    const QString color = alarmState.active ? QStringLiteral("#D64545")
                                            : QStringLiteral("#2FA84F");

    ui->alarmValueLabel->setText(alarmState.active ? tr("Alarm") : tr("Normal"));
    ui->alarmValueLabel->setToolTip(alarmState.message);
    ui->alarmValueLabel->setStyleSheet(alarmState.active
                                           ? QStringLiteral("color: #F5B7B1; font-weight: 700;")
                                           : QStringLiteral("color: #ABEBC6; font-weight: 700;"));
    m_ledIndicator->setStyleSheet(QStringLiteral(
        "border-radius: 9px;"
        "border: 1px solid #555;"
        "background-color: %1;").arg(color));
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
