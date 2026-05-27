# HMI Serial Dashboard

Qt6 Widgets based HMI dashboard prototype for serial/TCP data acquisition, realtime processing, visualization, logging, and status indication.

## Features

- Qt6 Widgets desktop application
- CMake build system
- TCP simulated data source enabled by default
- Optional Qt6 SerialPort support when the module is installed
- Optional Qt6 SerialBus support for Modbus RTU/TCP when the module is installed
- Abstract data-source layer for TCP simulation, TCP socket, and serial input
- Serial port auto-scan, hotplug refresh, and reconnect support
- Dedicated alarm manager with configurable high limits
- Alarm acknowledge, silence, and history workflow
- Runtime settings dialog for IO mode, serial/TCP parameters, alarm limits, logging, and UI limits
- CSV data logging with one file per acquisition session
- Realtime signal chain:
  - `SerialManager`
  - `DataSource`
  - `DataProcessor`
  - `WorkerThread`
  - `AlarmManager`
  - `DataLogger`
  - `MainWindow`
- Realtime UI:
  - Trend chart
  - History table
  - Runtime log
  - LED alarm indicator
  - Alarm operation/history panel
  - Live value cards
- Manual `uic` / `moc` helper scripts for restricted PowerShell environments
- Lightweight C++ unit test target for parser, alarms, config, and CSV logging

## Project Structure

```text
HMI_SerialDashboard/
  CMakeLists.txt
  main.cpp
  mainwindow.h
  mainwindow.cpp
  mainwindow.ui
  datasource.h
  datasource.cpp
  alarmmanager.h
  alarmmanager.cpp
  serialmanager.h
  serialmanager.cpp
  dataprocessor.h
  dataprocessor.cpp
  workerthread.h
  workerthread.cpp
  configmanager.h
  configmanager.cpp
  datalogger.h
  datalogger.cpp
  scripts/
    generate_autogen.ps1
    build_release.ps1
    deploy.ps1
```

## Important Note About Generated Files

Do not edit files under `build/` manually.

Examples:

```text
build/manual_autogen/include/ui_mainwindow.h
build/manual_autogen/moc/moc_*.cpp
build/Release/manual_autogen/include/ui_mainwindow.h
build/Release/manual_autogen/moc/moc_*.cpp
```

These files are generated from:

```text
mainwindow.ui
*.h files that contain Q_OBJECT
```

Edit the source files instead, then regenerate/build.

## Build Requirements

Tested with:

```text
Qt:      C:\Qt\6.11.0\mingw_64
MinGW:   C:\Qt\Tools\mingw1310_64
CMake:   C:\Qt\Tools\CMake_64\bin\cmake.exe
```

Required Qt modules:

```text
Qt6::Core
Qt6::Widgets
Qt6::Network
Qt6::Xml
Qt6::Charts
```

Optional:

```text
Qt6::SerialPort
Qt6::SerialBus
```

If `Qt6SerialPort` or `Qt6SerialBus` is not installed, the project still builds and runs with TCP simulation.

## Build Release

PowerShell may block local `.ps1` files depending on execution policy. Use this command from the project root:

```powershell
cd E:\HMI_SerialDashboard

& "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File .\scripts\build_release.ps1
```

Release executable:

```text
build/Release/HMI_SerialDashboard.exe
build/Release/hmi_unit_tests.exe
```

## Generate UI And MOC Files Only

```powershell
cd E:\HMI_SerialDashboard

& "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File .\scripts\generate_autogen.ps1
```

This generates:

```text
build/Release/manual_autogen/include/ui_mainwindow.h
build/Release/manual_autogen/moc/moc_mainwindow.cpp
build/Release/manual_autogen/moc/moc_datasource.cpp
build/Release/manual_autogen/moc/moc_alarmmanager.cpp
build/Release/manual_autogen/moc/moc_serialmanager.cpp
build/Release/manual_autogen/moc/moc_dataprocessor.cpp
build/Release/manual_autogen/moc/moc_workerthread.cpp
build/Release/manual_autogen/moc/moc_configmanager.cpp
build/Release/manual_autogen/moc/moc_datalogger.cpp
```

## Run Unit Tests

```powershell
cd E:\HMI_SerialDashboard

$env:PATH = "C:\Qt\6.11.0\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
.\build\Release\hmi_unit_tests.exe
```

Expected output:

```text
All HMI unit tests passed.
```

## Deploy

```powershell
cd E:\HMI_SerialDashboard

& "C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe" `
  -ExecutionPolicy Bypass `
  -File .\scripts\deploy.ps1
```

The deploy script builds Release and then runs `windeployqt` to create a runnable package.
If `windeployqt` is unavailable or fails, the script falls back to copying common Qt/MinGW
runtime DLLs, the `platforms/qwindows.dll` plugin, README, config files, and a `data_logs`
folder.

## Runtime Behavior

Default input mode is TCP simulation:

```cpp
m_configManager.setValue(QStringLiteral("io.mode"), QStringLiteral("tcp-sim"));
```

Click `Start` in the UI to run the signal chain:

```text
Start button
  -> SerialManager selects active DataSource
  -> TcpSimulationDataSource generates raw text
  -> dataReceived(QString)
  -> DataProcessor parses QVector<double>
  -> WorkerThread processes data in background
  -> AlarmManager evaluates limits
  -> MainWindow updates chart, table, log, value cards, alarm text, and LED
  -> DataLogger writes processed samples to CSV when logging is enabled
```

## Configuration

The application stores runtime settings next to the executable:

```text
config/hmi_config.json
```

When no config file exists, `MainWindow` creates default values for:

```text
io.mode
serial.portName
serial.baudRate
serial.autoReconnect
serial.reconnectIntervalMs
tcp.host
tcp.port
modbus.tcp.host
modbus.tcp.port
modbus.rtu.portName
modbus.rtu.baudRate
modbus.unitId
modbus.startAddress
modbus.registerCount
modbus.pollIntervalMs
modbus.timeoutMs
simulation.intervalMs
worker.intervalMs
alarm.temperatureHigh
alarm.pressureHigh
alarm.flowHigh
logging.enabled
logging.directory
ui.maxHistoryRows
ui.maxChartPoints
```

Use the `Settings` button in the left control panel to edit these values at runtime.
If acquisition is running, the app stops it before applying the new settings.

Supported `io.mode` values:

```text
tcp-sim
tcp
serial
modbus-tcp
modbus-rtu
```

## Data Logging

When CSV logging is enabled, each `Start` action creates one session file under:

```text
data_logs/
```

CSV columns:

```text
timestamp,temperature,pressure,flow,alarm_active,alarm_message
```

The log directory can be changed in the runtime settings dialog.

## Switch To SerialPort

Install Qt SerialPort first. For Qt Online Installer based setups, install the Qt6 SerialPort add-on that matches the current Qt kit.

Then select `serial` in the `Settings` dialog, or set:

```cpp
m_configManager.setValue(QStringLiteral("io.mode"), QStringLiteral("serial"));
m_configManager.setValue(QStringLiteral("serial.portName"), QStringLiteral("COM1"));
m_configManager.setValue(QStringLiteral("serial.baudRate"), 9600);
```

No UI signal-chain changes are needed. `SerialManager` keeps emitting:

```cpp
dataReceived(QString)
```

## Main Modules

### SerialManager

Selects and manages the active input source.

- Keeps the external `start()`, `stop()`, `readOnce()` API stable
- Delegates actual IO to `DataSource` implementations
- Emits `dataReceived(QString)`
- Forwards serial port list changes to the UI

### DataSource

Abstract input layer.

- `TcpSimulationDataSource`: deterministic simulated HMI values
- `TcpSocketDataSource`: raw TCP client data
- `SerialPortDataSource`: real serial data when `Qt6SerialPort` is installed
- `ModbusDataSource`: Modbus TCP/RTU holding-register polling when `Qt6SerialBus` is installed
- Future sources can be added without changing the processing/UI pipeline

### DataProcessor

Parses raw text into numeric vectors.

- Input: `QString`
- Output/cache: `QVector<double>`
- Emits `dataUpdated(QVector<double>)`

### WorkerThread

Background processing thread.

- Stores latest data with `QReadWriteLock`
- Uses `QWaitCondition` to reduce unnecessary polling
- Emits `dataProcessed(QVector<double>)`

### AlarmManager

Central alarm evaluator.

- Owns temperature, pressure, and flow high limits
- Evaluates processed values
- Emits `alarmStateChanged(bool, QString)` only when state/message changes
- Tracks acknowledge/silence state and alarm history
- Keeps LED, alarm text, and alarm logging out of parsing/threading code

### DataLogger

CSV session logger.

- Creates one CSV file per acquisition session
- Records processed values, alarm state, and alarm message
- Keeps file IO out of the UI update path

### MainWindow

HMI UI layer.

- Start/Stop controls
- Runtime settings dialog
- Realtime chart
- History table
- Runtime log
- LED alarm indicator
- Alarm acknowledge/silence buttons
- Alarm history table
- Live value cards
- CSV data logging coordination

## Alarm Defaults

Default alarm limits are configured by `MainWindow::ensureDefaultConfiguration()`:

```cpp
m_configManager.setValue(QStringLiteral("alarm.temperatureHigh"), 32.0);
m_configManager.setValue(QStringLiteral("alarm.pressureHigh"), 108.0);
m_configManager.setValue(QStringLiteral("alarm.flowHigh"), 70.0);
```

The UI shows:

```text
Green LED: normal
Red LED: active alarm
```

## Packaging Output

After running `scripts/deploy.ps1`, the default package folder is:

```text
package/HMI_SerialDashboard/
```

Expected contents include:

```text
HMI_SerialDashboard.exe
hmi_unit_tests.exe
Qt6*.dll
platforms/qwindows.dll
README.md
config/
data_logs/
```

`Qt6SerialPort.dll` and `Qt6SerialBus.dll` are copied only when those Qt modules are installed.

## Development Notes

- Keep source edits in root `.h/.cpp/.ui` files.
- Do not edit `build/` generated files.
- Keep `HMI_USE_MANUAL_AUTOGEN=ON` only for restricted shells.
- Qt Creator can use normal CMake automatic `uic/moc` with the default `HMI_USE_MANUAL_AUTOGEN=OFF`.

## Further Work

- Add CSV replay mode for offline debugging.
- Add live serial protocol framing presets.
- Add Modbus write/register-map editor.
- Add UI actions for exporting alarm history.
- Add an installer script with Inno Setup or NSIS.
