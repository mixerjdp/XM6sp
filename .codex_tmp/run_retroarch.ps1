param(
  [string]$Core,
  [string]$Log,
  [string]$Rom,
  [string]$RetroArch = 'D:\Emulation\Emulators\RetroArch\retroarch.exe'
)

Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;

public static class Win32Input {
  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool SetForegroundWindow(IntPtr hWnd);

  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

  [DllImport("user32.dll", SetLastError=true)]
  public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

  [StructLayout(LayoutKind.Sequential)]
  public struct INPUT {
    public uint type;
    public InputUnion U;
  }

  [StructLayout(LayoutKind.Explicit)]
  public struct InputUnion {
    [FieldOffset(0)] public KEYBDINPUT ki;
  }

  [StructLayout(LayoutKind.Sequential)]
  public struct KEYBDINPUT {
    public ushort wVk;
    public ushort wScan;
    public uint dwFlags;
    public uint time;
    public IntPtr dwExtraInfo;
  }

  public const uint INPUT_KEYBOARD = 1;
  public const uint KEYEVENTF_KEYUP = 0x0002;
  public const uint KEYEVENTF_SCANCODE = 0x0008;
  public const uint KEYEVENTF_EXTENDEDKEY = 0x0001;
  public const int SW_RESTORE = 9;

  public static void SendVirtualKey(ushort vk) {
    INPUT[] inputs = new INPUT[2];
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].U.ki.wVk = vk;
    inputs[0].U.ki.wScan = 0;
    inputs[0].U.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
    inputs[0].U.ki.time = 0;
    inputs[0].U.ki.dwExtraInfo = IntPtr.Zero;

    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].U.ki.wVk = vk;
    inputs[1].U.ki.wScan = 0;
    inputs[1].U.ki.dwFlags = KEYEVENTF_KEYUP | KEYEVENTF_EXTENDEDKEY;
    inputs[1].U.ki.time = 0;
    inputs[1].U.ki.dwExtraInfo = IntPtr.Zero;

    SendInput((uint)inputs.Length, inputs, Marshal.SizeOf(typeof(INPUT)));
  }
}
"@

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Log) | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue $Log

$args = @('--verbose', "--log-file=$Log", '-L', $Core, $Rom)
$p = Start-Process -FilePath $RetroArch -ArgumentList $args -PassThru

Start-Sleep -Seconds 10
if (-not $p.HasExited) {
  while ($p.MainWindowHandle -eq 0) {
    Start-Sleep -Milliseconds 200
    $p.Refresh()
  }

  [Win32Input]::ShowWindowAsync($p.MainWindowHandle, [Win32Input]::SW_RESTORE) | Out-Null
  [Win32Input]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 250
  Write-Host "[run_retroarch] sending F4"
  [Win32Input]::SendVirtualKey(0x73)
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
