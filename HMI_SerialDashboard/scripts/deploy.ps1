param(
    [string]$QtRoot = "C:\Qt\6.11.0\mingw_64",
    [string]$MingwRoot = "C:\Qt\Tools\mingw1310_64",
    [string]$BuildDir = "",
    [string]$PackageDir = "",
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot "build\Release"
} elseif (-not [System.IO.Path]::IsPathRooted($BuildDir)) {
    $BuildDir = Join-Path $RepoRoot $BuildDir
}

if ([string]::IsNullOrWhiteSpace($PackageDir)) {
    $PackageDir = Join-Path $RepoRoot "package\HMI_SerialDashboard"
} elseif (-not [System.IO.Path]::IsPathRooted($PackageDir)) {
    $PackageDir = Join-Path $RepoRoot $PackageDir
}

if (-not $SkipBuild) {
    & (Join-Path $PSScriptRoot "build_release.ps1") `
        -QtRoot $QtRoot `
        -MingwRoot $MingwRoot `
        -BuildDir $BuildDir
}

$ExePath = Join-Path $BuildDir "HMI_SerialDashboard.exe"
if (-not (Test-Path $ExePath)) {
    throw "Release executable was not found: $ExePath"
}

New-Item -ItemType Directory -Force $PackageDir | Out-Null
Copy-Item -LiteralPath $ExePath -Destination $PackageDir -Force

$TestExePath = Join-Path $BuildDir "hmi_unit_tests.exe"
if (Test-Path $TestExePath) {
    Copy-Item -LiteralPath $TestExePath -Destination $PackageDir -Force
}

$Windeployqt = Join-Path $QtRoot "bin\windeployqt.exe"
$DeployOk = $false
if (Test-Path $Windeployqt) {
    & $Windeployqt --release (Join-Path $PackageDir "HMI_SerialDashboard.exe")
    $DeployOk = ($LASTEXITCODE -eq 0)
} else {
    Write-Warning "windeployqt.exe was not found. Copy Qt runtime DLLs manually."
}

if (-not $DeployOk) {
    Write-Warning "windeployqt did not complete successfully. Copying common runtime files manually."

    $QtBin = Join-Path $QtRoot "bin"
    $MingwBin = Join-Path $MingwRoot "bin"
    $QtDlls = @(
        "Qt6Charts.dll",
        "Qt6Core.dll",
        "Qt6Gui.dll",
        "Qt6Network.dll",
        "Qt6OpenGL.dll",
        "Qt6OpenGLWidgets.dll",
        "Qt6SerialBus.dll",
        "Qt6SerialPort.dll",
        "Qt6Widgets.dll",
        "Qt6Xml.dll"
    )
    $MingwDlls = @(
        "libgcc_s_seh-1.dll",
        "libstdc++-6.dll",
        "libwinpthread-1.dll"
    )

    foreach ($Dll in $QtDlls) {
        $Source = Join-Path $QtBin $Dll
        if (Test-Path $Source) {
            Copy-Item -LiteralPath $Source -Destination $PackageDir -Force
        }
    }

    foreach ($Dll in $MingwDlls) {
        $Source = Join-Path $MingwBin $Dll
        if (Test-Path $Source) {
            Copy-Item -LiteralPath $Source -Destination $PackageDir -Force
        }
    }

    $PlatformDir = Join-Path $PackageDir "platforms"
    New-Item -ItemType Directory -Force $PlatformDir | Out-Null
    $QWindows = Join-Path $QtRoot "plugins\platforms\qwindows.dll"
    if (Test-Path $QWindows) {
        Copy-Item -LiteralPath $QWindows -Destination $PlatformDir -Force
    }
}

$ReadmePath = Join-Path $RepoRoot "README.md"
if (Test-Path $ReadmePath) {
    Copy-Item -LiteralPath $ReadmePath -Destination $PackageDir -Force
}

$ArchitecturePath = Join-Path $RepoRoot "ARCHITECTURE.md"
if (Test-Path $ArchitecturePath) {
    Copy-Item -LiteralPath $ArchitecturePath -Destination $PackageDir -Force
}

$BuildConfigDir = Join-Path $BuildDir "config"
if (Test-Path $BuildConfigDir) {
    Copy-Item -LiteralPath $BuildConfigDir -Destination $PackageDir -Recurse -Force
}

New-Item -ItemType Directory -Force (Join-Path $PackageDir "data_logs") | Out-Null

Write-Host "[deploy] Package directory:"
Write-Host "         $PackageDir"
