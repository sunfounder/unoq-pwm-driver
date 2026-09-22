<#
.SYNOPSIS
    Deploy UNOQ_PWMServoDriver to an Arduino UNO Q running App Lab.

.DESCRIPTION
    Pushes two things over SSH/SFTP:

      1. the library   -> ~/Arduino/libraries/UNOQ_PWMServoDriver/
      2. the demo app  -> ~/ArduinoApps/unoq-pwm-driver/

    The library sources are also copied into the App's sketch folder, because an
    App Lab sketch keeps its sketch.yaml and that puts arduino-cli into profile
    mode, where the user library directory is not searched.

    The App Lab app must already exist on the board; create it once with
    `arduino-app-cli app new unoq-pwm-driver`, because App Lab only recognises
    apps registered by its own CLI.

    This script is deliberately ASCII-only: Windows PowerShell 5.1 reads a
    BOM-less file using the ANSI code page, and non-ASCII text would corrupt
    the parser.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File .\deploy.ps1
    powershell -ExecutionPolicy Bypass -File .\deploy.ps1 -Restart
#>
[CmdletBinding()]
param(
    [string]$Device = 'arduino@192.168.18.119',
    [switch]$Restart
)

$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot

$sshOpts = @('-o', 'BatchMode=yes', '-o', 'ConnectTimeout=10')
$libRoot = '~/Arduino/libraries/UNOQ_PWMServoDriver'
$appRoot = '~/ArduinoApps/unoq-pwm-driver'

Write-Host "==> target device: $Device"

Write-Host '==> creating remote directories'
ssh @sshOpts $Device "mkdir -p $libRoot/src $appRoot/sketch $appRoot/python"

Write-Host '==> uploading library'
& scp @sshOpts -r src library.properties keywords.txt README.md LICENSE "${Device}:$libRoot/"

Write-Host '==> bundling library sources into the App sketch'
Copy-Item -Force (Join-Path $PSScriptRoot 'src\*') (Join-Path $PSScriptRoot 'applab\sketch')

Write-Host '==> uploading App Lab app'
& scp @sshOpts -r applab/app.yaml applab/README.md applab/.gitignore applab/python applab/sketch "${Device}:$appRoot/"

Write-Host '==> verifying'
ssh @sshOpts $Device "find $libRoot $appRoot -type f | sort"

if ($Restart) {
    Write-Host '==> restarting the App (compiles the sketch and flashes the MCU)'
    ssh @sshOpts $Device "cd ~/ArduinoApps/unoq-pwm-driver && arduino-app-cli app restart ."
}
else {
    Write-Host '==> restart skipped. To make the changes take effect run:'
    Write-Host ('    ssh ' + $Device + ' "cd ~/ArduinoApps/unoq-pwm-driver && arduino-app-cli app restart ."')
}

Write-Host '==> done'
