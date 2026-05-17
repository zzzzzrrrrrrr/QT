#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
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
        ui->startButton->setEnabled(false);
        ui->stopButton->setEnabled(true);
        ui->statusbar->showMessage(tr("Data acquisition started"));
        appendLogMessage(tr("Data acquisition started"));
        qDebug().noquote() << "[Step2][MainWindow] signal chain test is running";
    }
}

void MainWindow::handleStopClicked()
{
    qDebug().noquote() << "[Step2][MainWindow] Stop clicked";

    m_serialManager.stop();

    if (m_workerThread.isRunning()) {
        m_workerThread.stop();
        m_workerThread.wait(1000);
    }

    if (ui) {
        ui->startButton->setEnabled(true);
        ui->stopButton->setEnabled(false);
        ui->statusbar->showMessage(tr("Data acquisition stopped"));
        appendLogMessage(tr("Data acquisition stopped"));
    }
}

void MainWindow::handleProcessedData(const QVector<double> &values)
{
    const QString valuesText = formatValues(values);

    ui->dataView->setPlainText(tr("Processed values:\n%1").arg(valuesText));
    appendHistoryRow(values);
    updateChart(values);
    updateLedIndicator(values);
    appendLogMessage(tr("Processed values: %1").arg(valuesText));

    qDebug().noquote() << "[Step2][MainWindow] UI updated with processed data =" << valuesText;
}

void MainWindow::handleStatusMessage(const QString &message)
{
    ui->statusbar->showMessage(message);
}

void MainWindow::initializeModules()
{
    m_configManager.setValue(QStringLiteral("io.mode"), QStringLiteral("tcp-sim"));
    m_configManager.setValue(QStringLiteral("serial.portName"), QStringLiteral("COM1"));
    m_configManager.setValue(QStringLiteral("serial.baudRate"), 9600);
    m_configManager.setValue(QStringLiteral("tcp.host"), QStringLiteral("127.0.0.1"));
    m_configManager.setValue(QStringLiteral("tcp.port"), 502);
    m_configManager.setValue(QStringLiteral("simulation.intervalMs"), 250);

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
    m_workerThread.setIntervalMs(200);
    ui->stopButton->setEnabled(false);
}

void MainWindow::connectSignals()
{
    connect(ui->startButton, &QPushButton::clicked,
            this, &MainWindow::handleStartClicked);
    connect(ui->stopButton, &QPushButton::clicked,
            this, &MainWindow::handleStopClicked);

    connect(&m_serialManager, &SerialManager::dataReceived,
            &m_dataProcessor, &DataProcessor::processRawData);
    connect(&m_serialManager, &SerialManager::errorOccurred,
            this, &MainWindow::handleStatusMessage);
    connect(&m_dataProcessor, &DataProcessor::dataUpdated,
            &m_workerThread, &WorkerThread::setLatestData);
    connect(&m_workerThread, &WorkerThread::dataProcessed,
            this, &MainWindow::handleProcessedData);
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
        return;
    }

    m_serialManager.setConnectionType(SerialManager::ConnectionType::TcpSocket);
    m_serialManager.setTcpSimulationEnabled(true);
}

void MainWindow::setupRuntimeUiExtensions()
{
    auto *extensionLayout = new QVBoxLayout(ui->extensionArea);
    extensionLayout->setContentsMargins(0, 8, 0, 0);
    extensionLayout->setSpacing(8);

    setupLedIndicator();
    setupChart();
    setupHistoryTable();
    setupLogView();
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
    auto *statusLayout = new QHBoxLayout();
    auto *statusText = new QLabel(tr("Status"), ui->extensionArea);

    m_ledIndicator = new QLabel(ui->extensionArea);
    m_ledIndicator->setFixedSize(18, 18);
    m_ledIndicator->setToolTip(tr("Green: normal, Red: alarm"));

    statusLayout->addWidget(statusText);
    statusLayout->addWidget(m_ledIndicator);
    statusLayout->addStretch();
    ui->extensionArea->layout()->addItem(statusLayout);

    updateLedIndicator({});
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

void MainWindow::updateLedIndicator(const QVector<double> &values)
{
    if (!m_ledIndicator) {
        return;
    }

    // Simple alarm rule: pressure channel after processing is above 108.
    const bool alarm = values.size() > 1 && values.at(1) > 108.0;
    const QString color = alarm ? QStringLiteral("#D64545") : QStringLiteral("#2FA84F");

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
