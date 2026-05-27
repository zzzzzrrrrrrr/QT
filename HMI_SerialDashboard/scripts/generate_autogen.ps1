param(
    [string]$QtRoot = "C:\Qt\6.11.0\mingw_64",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\Release"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

$Uic = Join-Path $QtRoot "bin\uic.exe"
$Moc = Join-Path $QtRoot "bin\moc.exe"

if (-not (Test-Path $Uic)) {
    throw "uic.exe was not found: $Uic"
}
if (-not (Test-Path $Moc)) {
    throw "moc.exe was not found: $Moc"
}

$ManualAutogenDir = Join-Path $BuildDir "manual_autogen"
$UiOutputDir = Join-Path $ManualAutogenDir "include"
$MocOutputDir = Join-Path $ManualAutogenDir "moc"

New-Item -ItemType Directory -Force $UiOutputDir | Out-Null
New-Item -ItemType Directory -Force $MocOutputDir | Out-Null

$ChartsEnabled = 0
if (Test-Path (Join-Path $QtRoot "lib\cmake\Qt6Charts\Qt6ChartsConfig.cmake")) {
    $ChartsEnabled = 1
}

$SerialPortEnabled = 0
if (Test-Path (Join-Path $QtRoot "lib\cmake\Qt6SerialPort\Qt6SerialPortConfig.cmake")) {
    $SerialPortEnabled = 1
}

$SerialBusEnabled = 0
if (Test-Path (Join-Path $QtRoot "lib\cmake\Qt6SerialBus\Qt6SerialBusConfig.cmake")) {
    $SerialBusEnabled = 1
}

Write-Host "[autogen] Qt root: $QtRoot"
Write-Host "[autogen] Build dir: $BuildDir"
Write-Host "[autogen] Qt Charts: $ChartsEnabled"
Write-Host "[autogen] Qt SerialPort: $SerialPortEnabled"
Write-Host "[autogen] Qt SerialBus: $SerialBusEnabled"

# Step 1: convert the Designer file into the C++ UI header used by mainwindow.cpp.
& $Uic `
    -o (Join-Path $UiOutputDir "ui_mainwindow.h") `
    (Join-Path $RepoRoot "mainwindow.ui")

$CommonMocArgs = @(
    "-DHMI_HAS_QT_SERIALPORT=$SerialPortEnabled",
    "-DHMI_HAS_QT_SERIALBUS=$SerialBusEnabled",
    "-DHMI_HAS_QT_CHARTS=$ChartsEnabled",
    "-DQT_CORE_LIB",
    "-DQT_WIDGETS_LIB",
    "-DQT_NETWORK_LIB",
    "-DQT_CHARTS_LIB",
    "-DQT_XML_LIB",
    "-DQT_SERIALBUS_LIB",
    "-I$RepoRoot",
    "-I$QtRoot\include",
    "-I$QtRoot\include\QtCore",
    "-I$QtRoot\include\QtWidgets",
    "-I$QtRoot\include\QtGui",
    "-I$QtRoot\include\QtNetwork",
    "-I$QtRoot\include\QtCharts",
    "-I$QtRoot\include\QtSerialPort",
    "-I$QtRoot\include\QtSerialBus",
    "-I$QtRoot\include\QtXml"
)

$Headers = @(
    "mainwindow.h",
    "datasource.h",
    "alarmmanager.h",
    "datalogger.h",
    "serialmanager.h",
    "dataprocessor.h",
    "workerthread.h",
    "configmanager.h"
)

# Step 2: generate Qt meta-object code for every QObject-derived header.
foreach ($Header in $Headers) {
    $Name = [System.IO.Path]::GetFileNameWithoutExtension($Header)
    $InputPath = Join-Path $RepoRoot $Header
    $OutputPath = Join-Path $MocOutputDir "moc_$Name.cpp"

    & $Moc @CommonMocArgs $InputPath -o $OutputPath
    Write-Host "[autogen] Generated $OutputPath"
}

Write-Host "[autogen] Generated $(Join-Path $UiOutputDir 'ui_mainwindow.h')"
