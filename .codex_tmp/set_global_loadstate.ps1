$path = 'D:\Emulation\Emulators\RetroArch\retroarch.cfg'
$text = Get-Content -LiteralPath $path -Raw
$text = $text -replace '(?m)^input_load_state\s*=\s*\"nul\"$', 'input_load_state = "f4"'
Set-Content -LiteralPath $path -Value $text
