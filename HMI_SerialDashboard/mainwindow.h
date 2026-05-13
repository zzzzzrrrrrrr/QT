#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "configmanager.h"
#include "dataprocessor.h"
#include "serialmanager.h"
#include "workerthread.h"

#ifndef HMI_HAS_QT_CHARTS
#define HMI_HAS_QT_CHARTS 0
#endif

#include <QLabel>
#include <QMainWindow>
#include <QStandardItemModel>
#include <QTextEdit>
#include <QVector>

#if HMI_HAS_QT_CHARTS
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#endif

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void handleStartClicked();
    void handleStopClicked();
    void handleProcessedData(const QVector<double> &values);
    void handleStatusMessage(const QString &message);

private:
    void initializeModules();
    void connectSignals();
    void configureInputSource();
    void setupRuntimeUiExtensions();
    void setupChart();
    void setupHistoryTable();
    void setupLogView();
    void setupLedIndicator();
    void appendHistoryRow(const QVector<double> &values);
    void appendLogMessage(const QString &message);
    void updateChart(const QVector<double> &values);
    void updateLedIndicator(const QVector<double> &values);
    QString formatValues(const QVector<double> &values) const;

    Ui::MainWindow *ui;
    SerialManager m_serialManager;
    DataProcessor m_dataProcessor;
    WorkerThread m_workerThread;
    ConfigManager m_configManager;
    QStandardItemModel *m_historyModel = nullptr;
    QTextEdit *m_logView = nullptr;
    QLabel *m_ledIndicator = nullptr;
    int m_sampleIndex = 0;
    int m_maxHistoryRows = 200;

#if HMI_HAS_QT_CHARTS
    QChart *m_chart = nullptr;
    QChartView *m_chartView = nullptr;
    QLineSeries *m_temperatureSeries = nullptr;
    QLineSeries *m_pressureSeries = nullptr;
    QLineSeries *m_flowSeries = nullptr;
    QValueAxis *m_axisX = nullptr;
    QValueAxis *m_axisY = nullptr;
    int m_maxChartPoints = 120;
#endif
};
#endif // MAINWINDOW_H
