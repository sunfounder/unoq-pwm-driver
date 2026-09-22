<#
.SYNOPSIS
    Deploy UNOQ_PWMServoDriver to an Arduino UNO Q running App Lab.

.DESCRIPTION
    Pushes two things over SSH/SFTP:

      1. the library      -> ~/Arduino/libraries/UNOQ_PWMServoDriver/
      2. the demo app     -> ~/ArduinoApps/unoq-pwm-driver/

    The App Lab app must already exist on the board; create it once with
    `arduino-app-cli app new unoq-pwm-driver` (App Lab only recognises apps
    registered by its own CLI).

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

Write-Host "==> 目标设备: $Device"

Write-Host '==> 准备远程目录'
ssh @sshOpts $Device "mkdir -p $libRoot/src $appRoot/sketch $appRoot/python"

Write-Host '==> 上传库'
& scp @sshOpts -r src library.properties keywords.txt README.md LICENSE "${Device}:$libRoot/"

Write-Host '==> 上传 App Lab 应用'
& scp @sshOpts -r applab/app.yaml applab/README.md applab/.gitignore applab/python applab/sketch "${Device}:$appRoot/"

# An App Lab sketch folder ships a sketch.yaml, which puts arduino-cli into
# profile mode. Profile mode does NOT scan the user library directory, so the
# library would not be found there; removing it makes the build fall back to
# the default profile, which does discover ~/Arduino/libraries.
Write-Host '==> 移除远程 sketch.yaml（否则 profile 模式找不到用户库）'
ssh @sshOpts $Device "rm -f $appRoot/sketch/sketch.yaml"

Write-Host '==> 校验'
ssh @sshOpts $Device "find $libRoot $appRoot -type f | sort"

if ($Restart) {
    Write-Host '==> 重启 App（编译 sketch 并烧录 MCU）'
    ssh @sshOpts $Device "cd ~/ArduinoApps/unoq-pwm-driver && arduino-app-cli app restart ."
} else {
    Write-Host '==> 已跳过重启；需要生效时执行:'
    Write-Host "    ssh $Device `"cd ~/ArduinoApps/unoq-pwm-driver && arduino-app-cli app restart .`""
}

Write-Host '==> 完成'
