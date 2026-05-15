param(
  [string]$Core,
  [string]$Log,
  [string]$Rom,
  [int]$EntrySlot = -1,
  [string]$CommandName = $null,
  [string]$RetroArch = 'D:\Emulation\Emulators\RetroArch\retroarch.exe'
)

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Log) | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue $Log

$args = @('--verbose', "--log-file=$Log")
if ($EntrySlot -ge 0) {
  $args += @('-e', "$EntrySlot")
}
$args += @('-L', $Core, $Rom)
$p = Start-Process -FilePath $RetroArch -ArgumentList $args -PassThru

Start-Sleep -Seconds 10
if ($CommandName) {
  Start-Process -FilePath $RetroArch -ArgumentList @('--command', $CommandName) | Out-Null
  Start-Sleep -Seconds 2
}
Start-Sleep -Seconds 8
if (-not $p.HasExited) {
  Stop-Process -Id $p.Id -Force
}
Start-Sleep -Seconds 2
if (Test-Path $Log) {
  Get-Item $Log | Select-Object FullName,Length,LastWriteTime
} else {
  Write-Host 'LOG_NOT_FOUND'
}
