param(
  [ValidateSet('Debug','Release')]
  [string]$Configuration = 'Debug'
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$vmDir = Join-Path $repoRoot 'vm'
$outDir = Join-Path $repoRoot (Join-Path 'build_vm_only' $Configuration)

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
  Write-Host 'ERROR: cl.exe no esta disponible. Ejecuta este script desde build_vm_only.bat.' -ForegroundColor Red
  exit 1
}

$srcFiles = @()
$srcFiles += Get-ChildItem $vmDir -File -Filter *.cpp
$srcFiles += Get-ChildItem $vmDir -File -Filter *.c
$srcFiles = $srcFiles | Sort-Object Name
if ($srcFiles.Count -eq 0) {
  Write-Host 'ERROR: no se encontraron fuentes en vm/.' -ForegroundColor Red
  exit 1
}

New-Item -ItemType Directory -Force -Path $outDir | Out-Null

$includeFlags = @(
  "/I`"$($repoRoot)\\vm`"",
  "/I`"$($repoRoot)\\cpu`"",
  "/I`"$($repoRoot)\\mfc`""
)

$defineFlags = @(
  '/DWIN32',
  '/D_WINDOWS',
  '/D_MBCS',
  '/D_CRT_SECURE_NO_WARNINGS'
)

if ($Configuration -eq 'Debug') {
  $defineFlags += '/D_DEBUG'
  $cfgFlags = @('/Zi', '/MDd')
} else {
  $defineFlags += '/DNDEBUG'
  $cfgFlags = @('/O2', '/MD')
}

$commonFlags = @('/nologo', '/c', '/W4') + $cfgFlags + $includeFlags + $defineFlags

$errors = 0
foreach ($src in $srcFiles) {
  $objPath = Join-Path $outDir ($src.BaseName + '.obj')
  Write-Host ("Compilando {0}" -f $src.Name)

  $flags = @()
  if ($src.Extension -ieq '.cpp') {
    $flags += '/EHsc'
  }

  & cl.exe @commonFlags @flags "/Fo$objPath" "$($src.FullName)"
  if ($LASTEXITCODE -ne 0) {
    $errors++
  }
}

if ($errors -gt 0) {
  Write-Host ("ERROR: build VM-only fallo en {0} archivo(s)." -f $errors) -ForegroundColor Red
  exit 1
}

Write-Host "OK: VM-only compilado correctamente ($Configuration)." -ForegroundColor Green
exit 0
