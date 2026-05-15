param(
  [string]$Core,
  [string]$Log,
  [string]$Rom,
  [string]$BeforeShot,
  [string]$AfterShot,
  [string]$RetroArch = 'D:\Emulation\Emulators\RetroArch\retroarch.exe'
)

Add-Type -TypeDefinition @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Runtime.InteropServices;

public static class Win32Capture {
  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool SetForegroundWindow(IntPtr hWnd);

  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool ShowWindowAsync(IntPtr hWnd, int nCmdShow);

  [DllImport("user32.dll", SetLastError=true)]
  public static extern uint SendInput(uint nInputs, INPUT[] pInputs, int cbSize);

  [DllImport("user32.dll", SetLastError=true)]
  public static extern bool GetWindowRect(IntPtr hWnd, out RECT lpRect);

  [StructLayout(LayoutKind.Sequential)]
  public struct RECT {
    public int Left;
    public int Top;
    public int Right;
    public int Bottom;
  }

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

  public static void CaptureRect(string path, RECT r) {
    int width = Math.Max(1, r.Right - r.Left);
    int height = Math.Max(1, r.Bottom - r.Top);
    using (var bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb))
    using (var g = Graphics.FromImage(bmp)) {
      g.CopyFromScreen(r.Left, r.Top, 0, 0, new Size(width, height), CopyPixelOperation.SourceCopy);
      bmp.Save(path, ImageFormat.Png);
    }
  }
}
"@

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $Log) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $BeforeShot) | Out-Null
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AfterShot) | Out-Null
Remove-Item -Force -ErrorAction SilentlyContinue $Log, $BeforeShot, $AfterShot

$args = @('--verbose', "--log-file=$Log", '-L', $Core, $Rom)
$p = Start-Process -FilePath $RetroArch -ArgumentList $args -PassThru

Start-Sleep -Seconds 10
if (-not $p.HasExited) {
  while ($p.MainWindowHandle -eq 0) {
    Start-Sleep -Milliseconds 200
    $p.Refresh()
  }

  [Win32Capture]::ShowWindowAsync($p.MainWindowHandle, [Win32Capture]::SW_RESTORE) | Out-Null
  [Win32Capture]::SetForegroundWindow($p.MainWindowHandle) | Out-Null
  Start-Sleep -Milliseconds 250

  $rect = New-Object Win32Capture+RECT
  if ([Win32Capture]::GetWindowRect($p.MainWindowHandle, [ref]$rect)) {
    [Win32Capture]::CaptureRect($BeforeShot, $rect)
  }

  Write-Host "[run_retroarch_capture] sending F4"
  [Win32Capture]::SendVirtualKey(0x73)

  Start-Sleep -Seconds 2
  if ([Win32Capture]::GetWindowRect($p.MainWindowHandle, [ref]$rect)) {
    [Win32Capture]::CaptureRect($AfterShot, $rect)
  }
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
