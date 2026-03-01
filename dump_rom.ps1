$bytes = [System.IO.File]::ReadAllBytes('c:\sw\XM62022\00proj.vc7\Debug\iplrom.dat')
$start = 0x10510
$end = 0x10530
$out = ""
for ($i = $start; $i -lt $end; $i += 2) {
    if (($i - $start) % 16 -eq 0) { $out += "`n$([Convert]::ToString($i, 16).PadLeft(6, '0')): " }
    $w = ($bytes[$i] -shl 8) -bor $bytes[$i + 1]
    $out += $([Convert]::ToString($w, 16).PadLeft(4, '0')) + " "
}
Write-Host $out
