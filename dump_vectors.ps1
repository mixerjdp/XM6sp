$bytes = [System.IO.File]::ReadAllBytes('c:\sw\XM62022\00proj.vc7\Debug\iplrom.dat')
$start = 0x10000
$end = 0x10040
$out = ""
for ($i = $start; $i -lt $end; $i += 4) {
    if (($i - $start) % 16 -eq 0) { $out += "`n$([Convert]::ToString($i, 16).PadLeft(6, '0')): " }
    $w1 = ($bytes[$i] -shl 8) -bor $bytes[$i + 1]
    $w2 = ($bytes[$i + 2] -shl 8) -bor $bytes[$i + 3]
    $dw = ($w1 -shl 16) -bor $w2
    $out += $([Convert]::ToString($dw, 16).PadLeft(8, '0')) + " "
}
Write-Host $out
