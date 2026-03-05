param(
  [string]$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
)

$patterns = @(
  '^\s*#\s*include\s+"mfc',
  '^\s*#\s*include\s+<afx',
  '\bAfx[A-Za-z0-9_]*\b',
  '\bCString\b',
  '\bCCriticalSection\b',
  '\bCWnd\b'
)

$files = Get-ChildItem (Join-Path $RepoRoot 'vm') -Recurse -File -Include *.h,*.cpp
$hits = @()
foreach ($file in $files) {
  foreach ($pattern in $patterns) {
    $matches = Select-String -Path $file.FullName -Pattern $pattern
    foreach ($m in $matches) {
      $hits += [PSCustomObject]@{
        File = $file.FullName
        Line = $m.LineNumber
        Text = $m.Line.Trim()
      }
    }
  }
}

if ($hits.Count -gt 0) {
  Write-Host 'ERROR: se detectaron dependencias MFC dentro de vm/:' -ForegroundColor Red
  $hits | Sort-Object File, Line | ForEach-Object {
    Write-Host ("  {0}:{1}: {2}" -f $_.File, $_.Line, $_.Text)
  }
  exit 1
}

Write-Host 'OK: vm/ libre de dependencias MFC directas.' -ForegroundColor Green
exit 0
