param(
    [string]$QtRoot = "C:\Qt\6.11.0\mingw_64",
    [string]$MingwRoot = "C:\Qt\Tools\mingw1310_64",
    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\Release"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

$CMake = "C:\Qt\Tools\CMake_64\bin\cmake.exe"
$Make = Join-Path $MingwRoot "bin\mingw32-make.exe"
$Compiler = Join-Path $MingwRoot "bin\g++.exe"

if (-not (Test-Path $CMake)) {
    throw "cmake.exe was not found: $CMake"
}
if (-not (Test-Path $Make)) {
    throw "mingw32-make.exe was not found: $Make"
}
if (-not (Test-Path $Compiler)) {
    throw "g++.exe was not found: $Compiler"
}

# The manual autogen path keeps builds working in restricted shells while
# normal Qt Creator builds can still use CMake AUTOMOC/AUTOUIC.
& (Join-Path $PSScriptRoot "generate_autogen.ps1") `
    -QtRoot $QtRoot `
    -BuildDir $BuildDir

& $CMake `
    -S $RepoRoot `
    -B $BuildDir `
    -G "MinGW Makefiles" `
    "-DCMAKE_PREFIX_PATH=$QtRoot" `
    -DCMAKE_BUILD_TYPE=Release `
    "-DCMAKE_MAKE_PROGRAM=$Make" `
    "-DCMAKE_CXX_COMPILER=$Compiler" `
    -DHMI_USE_MANUAL_AUTOGEN=ON

& $CMake --build $BuildDir --config Release

Write-Host "[build] Release executable:"
Write-Host "        $(Join-Path $BuildDir 'HMI_SerialDashboard.exe')"
