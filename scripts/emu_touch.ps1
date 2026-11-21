<#
Drive the emulator's touch panel and capture the window to a PNG.

Input goes through the real cursor (SetCursorPos + mouse_event): the emulator's finger is
the operating system's pointer, exactly like a person clicking the window. Posting
synthetic WM_*BUTTON messages was tried first and rejected -- SDL calls SetCapture() on
button-down, and Windows then emits a WM_MOUSEMOVE carrying the *real* cursor position,
which a synthetic-only run turns into a spurious drag. With real input that extra sample
lands on the same point, so the gesture is what the mouse actually did.

The window is brought to the foreground first (clicks go to whatever is under the
cursor) and the caller's cursor position is restored afterwards.

Actions are ';'-separated, in client pixels of the 480x800 emulator window:
  down:X,Y              press at (X,Y)
  move:X,Y              move the pointer (still pressed if it was)
  up:X,Y                release at (X,Y)
  tap:X,Y               down+up at (X,Y)
  long:X,Y,MS           down, hold MS, up
  swipe:X1,Y1,X2,Y2,MS  down, interpolated moves over MS, up
  fkey:N                press F<N> in the window (1-12); F1..F7 are the gesture
                        hotkeys: move the pointer first, then the emulator
                        delivers that gesture at the pointer with its own timing
  wait:MS               pause

Example:
  powershell -NoProfile -ExecutionPolicy Bypass -File scripts\emu_touch.ps1 ^
    -Actions "tap:232,421" -Out shot.png -WaitMs 1500
#>
param(
  [Parameter(Mandatory = $true)][string]$Actions,
  [Parameter(Mandatory = $true)][string]$Out,
  [int]$ProcessId = 0,
  [int]$WaitMs = 1200
)

Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName Microsoft.VisualBasic
Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class EmuTouch {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool GetClientRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
  [DllImport("user32.dll")] public static extern bool SetCursorPos(int x, int y);
  [DllImport("user32.dll")] public static extern bool GetCursorPos(out POINT p);
  [DllImport("user32.dll")] public static extern void mouse_event(uint flags, uint dx, uint dy, uint data, UIntPtr extra);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int Left, Top, Right, Bottom; }
  [StructLayout(LayoutKind.Sequential)] public struct POINT { public int X, Y; }
}
"@

$LEFTDOWN = 0x0002
$LEFTUP = 0x0004

$proc = if ($ProcessId -ne 0) { Get-Process -Id $ProcessId -ErrorAction SilentlyContinue } else {
  Get-Process crosspoint_emulator -ErrorAction SilentlyContinue | Select-Object -First 1
}
if (-not $proc) { Write-Host "EMULATOR NOT RUNNING"; exit 2 }
$targetPid = $proc.Id

$script:hwnd = [IntPtr]::Zero
$cb = [EmuTouch+EnumWindowsProc]{
  param($h, $l)
  $wpid = 0
  [EmuTouch]::GetWindowThreadProcessId($h, [ref]$wpid) | Out-Null
  if ($wpid -eq $targetPid) {
    $sb = New-Object System.Text.StringBuilder 256
    [EmuTouch]::GetWindowText($h, $sb, 256) | Out-Null
    if ($sb.ToString() -like "*Crosspoint*") { $script:hwnd = $h }
  }
  return $true
}
[EmuTouch]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:hwnd -eq [IntPtr]::Zero) { Write-Host "WINDOW NOT FOUND"; exit 3 }

$cr = New-Object EmuTouch+RECT
[EmuTouch]::GetClientRect($script:hwnd, [ref]$cr) | Out-Null
Write-Host ("client=" + ($cr.Right - $cr.Left) + "x" + ($cr.Bottom - $cr.Top))

[Microsoft.VisualBasic.Interaction]::AppActivate($targetPid) | Out-Null
Start-Sleep -Milliseconds 250

$saved = New-Object EmuTouch+POINT
[EmuTouch]::GetCursorPos([ref]$saved) | Out-Null

function Set-ClientPoint([int]$x, [int]$y) {
  $p = New-Object EmuTouch+POINT
  $p.X = $x
  $p.Y = $y
  [EmuTouch]::ClientToScreen($script:hwnd, [ref]$p) | Out-Null
  [EmuTouch]::SetCursorPos($p.X, $p.Y) | Out-Null
}

foreach ($raw in ($Actions -split ';')) {
  $action = $raw.Trim()
  if ($action -eq '') { continue }
  $parts = $action -split ':'
  $op = $parts[0].Trim().ToLowerInvariant()
  $args = @()
  if ($parts.Count -gt 1) { $args = $parts[1] -split ',' | ForEach-Object { [int]$_.Trim() } }

  switch ($op) {
    'wait' { Start-Sleep -Milliseconds $args[0] }
    'move' { Set-ClientPoint $args[0] $args[1] }
    'down' { Set-ClientPoint $args[0] $args[1]; Start-Sleep -Milliseconds 30; [EmuTouch]::mouse_event($LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero) }
    'up' { Set-ClientPoint $args[0] $args[1]; Start-Sleep -Milliseconds 30; [EmuTouch]::mouse_event($LEFTUP, 0, 0, 0, [UIntPtr]::Zero) }
    'tap' {
      Set-ClientPoint $args[0] $args[1]
      Start-Sleep -Milliseconds 40
      [EmuTouch]::mouse_event($LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
      Start-Sleep -Milliseconds 70
      [EmuTouch]::mouse_event($LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    }
    'long' {
      Set-ClientPoint $args[0] $args[1]
      Start-Sleep -Milliseconds 40
      [EmuTouch]::mouse_event($LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
      Start-Sleep -Milliseconds $args[2]
      [EmuTouch]::mouse_event($LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    }
    'swipe' {
      $x1 = $args[0]; $y1 = $args[1]; $x2 = $args[2]; $y2 = $args[3]; $ms = $args[4]
      Set-ClientPoint $x1 $y1
      Start-Sleep -Milliseconds 40
      [EmuTouch]::mouse_event($LEFTDOWN, 0, 0, 0, [UIntPtr]::Zero)
      $steps = [Math]::Max(6, [int]($ms / 12))
      for ($i = 1; $i -le $steps; $i++) {
        $t = $i / $steps
        Set-ClientPoint ([int]($x1 + ($x2 - $x1) * $t)) ([int]($y1 + ($y2 - $y1) * $t))
        Start-Sleep -Milliseconds ([Math]::Max(1, [int]($ms / $steps)))
      }
      [EmuTouch]::mouse_event($LEFTUP, 0, 0, 0, [UIntPtr]::Zero)
    }
    'fkey' {
      # Posted, not sent: the emulator reads gesture hotkeys from its SDL event queue,
      # so a window message is enough and the caller keeps its own keyboard focus.
      $vk = 111 + $args[0]
      $scan = [EmuTouch]::MapVirtualKey([uint32]$vk, 0)
      $lp = 1 -bor ($scan -shl 16)
      [EmuTouch]::PostMessage($script:hwnd, 0x0100, [IntPtr]$vk, [IntPtr]$lp) | Out-Null
      Start-Sleep -Milliseconds 60
      [EmuTouch]::PostMessage($script:hwnd, 0x0101, [IntPtr]$vk, [IntPtr]($lp -bor (1 -shl 30) -bor (1 -shl 31))) | Out-Null
    }
    default { throw "unknown action '$op'" }
  }
  Start-Sleep -Milliseconds 40
}

Start-Sleep -Milliseconds $WaitMs
[EmuTouch]::SetCursorPos($saved.X, $saved.Y) | Out-Null

$alive = $null -ne (Get-Process -Id $targetPid -ErrorAction SilentlyContinue)
Write-Host "alive=$alive"

$r = New-Object EmuTouch+RECT
[EmuTouch]::GetWindowRect($script:hwnd, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap ($r.Right - $r.Left), ($r.Bottom - $r.Top)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$hdc = $g.GetHdc()
[EmuTouch]::PrintWindow($script:hwnd, $hdc, 2) | Out-Null
$g.ReleaseHdc($hdc)
$g.Dispose()
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "saved $Out"
