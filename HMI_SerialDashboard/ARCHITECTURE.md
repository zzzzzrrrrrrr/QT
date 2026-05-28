# HMI Architecture

This project is organized as a small but extensible Qt6 Widgets HMI. The main
rule is that device IO, data parsing, background processing, alarm logic,
logging, and UI rendering stay in separate modules.

## Runtime Data Flow

```text
DataSource
  -> ProtocolFramer for TCP/serial byte stream boundaries
  -> SerialManager::dataReceived(QString)
  -> DataProcessor::processRawData(QString)
  -> DataProcessor::dataUpdated(QVector<double>)
  -> WorkerThread::setLatestData(QVector<double>)
  -> WorkerThread::dataProcessed(QVector<double>)
  -> MainWindow
      -> AlarmManager::evaluate(QVector<double>)
      -> DataLogger::logSample(...)
      -> throttled UI refresh
```

## Module Boundaries

### DataSource

Owns concrete acquisition details.

- `TcpSimulationDataSource`: deterministic local test data.
- `TcpSocketDataSource`: raw TCP client input.
- `SerialPortDataSource`: Qt SerialPort input, auto-scan, hotplug refresh,
  and reconnect.
- `ModbusDataSource`: optional Qt SerialBus Modbus TCP/RTU polling.

All sources emit the same raw text signal, so downstream modules do not care
where the data came from.

TCP and serial byte streams pass through `ProtocolFramer` before they emit raw
text. The framer owns packet boundaries and supports raw chunks, line-delimited
frames, custom delimiters, and fixed-length frames. This keeps half-packet and
sticky-packet handling out of `DataProcessor`.

### SerialManager

Selects the active source and keeps the stable public API:

```cpp
start()
stop()
readOnce()
dataReceived(QString)
```

It is the correct place to add new input modes, not `MainWindow`.

### DataProcessor

Converts raw device text into numeric vectors and keeps the latest parsed
snapshot. Parsing is intentionally independent from UI and IO.

### WorkerThread

Runs background numeric work and emits processed vectors. It keeps latest input
data behind a read-write lock and wakes on demand instead of polling constantly.
Processing is configurable as pass-through, scale/offset calibration, or a
simple low-pass filter.

### AlarmManager

Owns alarm thresholds, current alarm state, acknowledgement, silence state,
optional latching, hysteresis, and alarm history. UI widgets only display and
trigger actions.

### DataLogger

Writes processed samples to CSV. It buffers normal writes and flushes on a timer
plus session stop, which avoids one disk flush per sample.

### MainWindow

Owns the Widgets UI. It should stay a consumer/coordinator:

- Starts/stops acquisition.
- Applies runtime config.
- Receives processed data.
- Delegates alarm evaluation and CSV logging.
- Refreshes chart/table/log/LED through a throttled UI timer.

## Threading Model

```text
GUI thread:
  MainWindow
  SerialManager and DataSource objects
  DataProcessor slots
  AlarmManager
  DataLogger

Worker thread:
  WorkerThread::run()
```

`WorkerThread::setLatestData()` is thread-safe because it writes through
`QReadWriteLock`. The heavy UI widgets are updated only in the GUI thread.

## Performance Rules

- Do not update charts, tables, or text logs per raw sample.
- Store the latest processed values and refresh UI at `ui.refreshIntervalMs`.
- Batch table/chart updates with at most `ui.maxSamplesPerRefresh` samples per
  frame.
- Keep CSV logging separate from UI refresh; the logger records every processed
  sample even when the UI coalesces samples.
- Do not flush CSV on every write. Use `logging.flushIntervalMs` and flush on
  stop.
- Keep per-sample `qDebug()` disabled by default.
- Keep bounded UI history through `ui.maxHistoryRows` and `ui.maxChartPoints`.

## Adding A New Protocol

1. Add a new `DataSource` subclass.
2. Register it in `SerialManager`.
3. Add config keys in `MainWindow::ensureDefaultConfiguration()`.
4. Add settings controls in `MainWindow::openSettingsDialog()`.
5. Keep emitted data as raw text through `dataReceived(QString)`.

The parser and UI should not need protocol-specific changes unless the data
format itself changes.

## Adding More Processing

Put numeric transformations in `WorkerThread` or a dedicated processing class.
Keep the result as a `QVector<double>` until there is a real need for a richer
sample type.

If the project grows beyond three or four channels, introduce a typed sample
model:

```cpp
struct HmiSample {
    QDateTime timestamp;
    double temperature;
    double pressure;
    double flow;
};
```

For now, `QVector<double>` keeps the prototype flexible and small.
