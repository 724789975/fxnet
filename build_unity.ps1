# ============================================================
# FxNet Unity UDP Test - automated build script (Windows PowerShell)
# ------------------------------------------------------------
# Invokes the Unity editor in batchmode via -executeMethod to run
# FxNetBuildScript, producing:
#   publish\win-client\FxNetUdpClient.exe    local interactive Windows client
#   publish\linux-server\FxNetUdpServer      Linux dedicated server (the same
#                                            binary also runs as a headless
#                                            client via -role client)
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File build_unity.ps1              # build all
#   powershell -ExecutionPolicy Bypass -File build_unity.ps1 -Target win    # Windows client only
#   powershell -ExecutionPolicy Bypass -File build_unity.ps1 -Target linux  # Linux server only
#   powershell -ExecutionPolicy Bypass -File build_unity.ps1 -UnityPath "C:\...\Unity.exe"
#
# NOTE: keep this file ASCII-only. Windows PowerShell 5.1 reads .ps1 as the
#       system ANSI codepage, so non-ASCII text can break parsing.
# ============================================================
param(
    [ValidateSet("all", "win", "linux")]
    [string]$Target = "all",
    [string]$UnityPath = ""
)

$ErrorActionPreference = "Stop"

# --- project paths (this script lives in the unity\ folder) ---
$ScriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Definition
$ProjectPath = Join-Path $ScriptDir "network"
$PublishDir  = Join-Path $ScriptDir "publish"

# --- locate Unity.exe ---
if ([string]::IsNullOrEmpty($UnityPath)) {
    # prefer the exact editor version pinned by the project
    $verFile = Join-Path $ProjectPath "ProjectSettings\ProjectVersion.txt"
    $version = ""
    if (Test-Path $verFile) {
        $line = Get-Content $verFile | Where-Object { $_ -match "^m_EditorVersion:" } | Select-Object -First 1
        if ($line) { $version = ($line -replace "m_EditorVersion:\s*", "").Trim() }
    }

    $candidates = @()
    if ($version) {
        $candidates += "C:\Program Files\Unity\Hub\Editor\$version\Editor\Unity.exe"
    }
    # fallback: any version under the Hub
    $hubRoot = "C:\Program Files\Unity\Hub\Editor"
    if (Test-Path $hubRoot) {
        Get-ChildItem $hubRoot -Directory | ForEach-Object {
            $candidates += (Join-Path $_.FullName "Editor\Unity.exe")
        }
    }

    $UnityPath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

if ([string]::IsNullOrEmpty($UnityPath) -or -not (Test-Path $UnityPath)) {
    Write-Error "Unity.exe not found; pass it explicitly with -UnityPath."
    exit 1
}

# --- pick the build method ---
switch ($Target) {
    "win"   { $method = "FxNet.UdpTest.EditorTools.FxNetBuildScript.BuildWindowsClient" }
    "linux" { $method = "FxNet.UdpTest.EditorTools.FxNetBuildScript.BuildLinuxServer" }
    default { $method = "FxNet.UdpTest.EditorTools.FxNetBuildScript.BuildAll" }
}

$logFile = Join-Path $ScriptDir "build_unity.log"

Write-Host "============================================" -ForegroundColor Cyan
Write-Host " FxNet Unity build" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "  Unity   : $UnityPath"
Write-Host "  Project : $ProjectPath"
Write-Host "  Target  : $Target  ->  $method"
Write-Host "  Log     : $logFile"
Write-Host ""

# --- run the batchmode build ---
$unityArgs = @(
    "-batchmode", "-quit", "-nographics",
    "-projectPath", $ProjectPath,
    "-executeMethod", $method,
    "-logFile", $logFile
)

$proc = Start-Process -FilePath $UnityPath -ArgumentList $unityArgs -NoNewWindow -PassThru -Wait
$code = $proc.ExitCode

Write-Host ""
if ($code -eq 0) {
    Write-Host "=== build succeeded (exit $code) ===" -ForegroundColor Green
    if (Test-Path $PublishDir) {
        Write-Host "output dir: $PublishDir"
        Get-ChildItem $PublishDir -Directory | ForEach-Object { Write-Host "  - $($_.Name)\" }
    }
} else {
    Write-Host "=== build FAILED (exit $code) ===" -ForegroundColor Red
    Write-Host "log tail:" -ForegroundColor Yellow
    if (Test-Path $logFile) { Get-Content $logFile -Tail 40 }
}

exit $code
