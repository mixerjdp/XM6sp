$path = 'D:\Emulation\Emulators\RetroArch\retroarch.cfg'
$text = Get-Content -LiteralPath $path -Raw
$text = $text -replace '(?m)^network_cmd_enable\s*=\s*\"false\"$', 'network_cmd_enable = "true"'
Set-Content -LiteralPath $path -Value $text
