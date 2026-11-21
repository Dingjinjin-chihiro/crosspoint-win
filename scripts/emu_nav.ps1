# Drive the Crosspoint emulator: send keys via PostMessage, then dump the panel framebuffer.
# Requires the emulator to be started with CROSSPOINT_DUMP_FB=<path to fb.pbm>.
#
# Usage: powershell -File emu_nav.ps1 -Keys "ENTER" -Name step1 [-WaitMs 2500]
param(
  [Parameter(Mandatory = $true)][string]$Keys,
  [Parameter(Mandatory = $true)][string]$Name,
  [int]$WaitMs = 2500
)

$root = "C:\Users\DINGJINJIN\Downloads\cross_point"
$fb = Join-Path $root "fb.pbm"

Add-Type @"
using System;
using System.Runtime.InteropServices;
using System.Text;
public class NavWin {
  public delegate bool EnumWindowsProc(IntPtr hWnd, IntPtr lParam);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc cb, IntPtr lParam);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr hWnd, out uint pid);
  [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h, uint msg, IntPtr w, IntPtr l);
  [DllImport("user32.dll")] public static extern uint MapVirtualKey(uint code, uint mapType);
  [DllImport("user32.dll", CharSet=CharSet.Unicode)] public static extern int GetWindowText(IntPtr hWnd, StringBuilder s, int n);
}
"@

$script:vkMap = @{}
$script:vkMap["LEFT"] = 0x25
$script:vkMap["UP"] = 0x26
$script:vkMap["RIGHT"] = 0x27
$script:vkMap["DOWN"] = 0x28
$script:vkMap["ENTER"] = 0x0D
$script:vkMap["BACK"] = 0x08
$script:vkMap["ESC"] = 0x1B
$script:vkMap["SPACE"] = 0x20
$script:vkMap["P"] = 0x50

$proc = Get-Process crosspoint_emulator -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc) { Write-Host "EMULATOR NOT RUNNING"; exit 2 }
$targetPid = $proc.Id

$script:hwnd = [IntPtr]::Zero
$cb = [NavWin+EnumWindowsProc]{
  param($h, $l)
  $wpid = 0
  [NavWin]::GetWindowThreadProcessId($h, [ref]$wpid) | Out-Null
  if ($wpid -eq $targetPid) {
    $sb = New-Object System.Text.StringBuilder 256
    [NavWin]::GetWindowText($h, $sb, 256) | Out-Null
    if ($sb.ToString() -like "*Crosspoint*" -and $script:hwnd -eq [IntPtr]::Zero) { $script:hwnd = $h }
  }
  return $true
}
[NavWin]::EnumWindows($cb, [IntPtr]::Zero) | Out-Null
if ($script:hwnd -eq [IntPtr]::Zero) { Write-Host "WINDOW NOT FOUND"; exit 3 }

foreach ($raw in ($Keys -split ',')) {
  $keyName = $raw.Trim().ToUpperInvariant()
  if (-not $script:vkMap.ContainsKey($keyName)) { throw "unknown key '$raw'" }
  $vk = $script:vkMap[$keyName]
  $scan = [NavWin]::MapVirtualKey([uint32]$vk, 0)
  # Arrow/navigation keys are "extended" keys: bit 24 must be set or SDL maps them wrongly.
  $extended = @("LEFT", "UP", "RIGHT", "DOWN") -contains $keyName
  $lp = 1 -bor ($scan -shl 16)
  if ($extended) { $lp = $lp -bor (1 -shl 24) }
  $down = [IntPtr]$lp
  $up = [IntPtr]($lp -bor (1 -shl 30) -bor (1 -shl 31))
  [NavWin]::PostMessage($script:hwnd, 0x0100, [IntPtr]$vk, $down) | Out-Null
  Start-Sleep -Milliseconds 70
  [NavWin]::PostMessage($script:hwnd, 0x0101, [IntPtr]$vk, $up) | Out-Null
  Start-Sleep -Milliseconds 450
}

Start-Sleep -Milliseconds $WaitMs
$alive = $null -ne (Get-Process -Id $targetPid -ErrorAction SilentlyContinue)
Write-Host "alive=$alive"
if (Test-Path $fb) {
  $png = Join-Path $root "$Name.png"
  python (Join-Path $root "scripts\pbm_info.py") $fb $png | Select-Object -First 2
  Write-Host "shot: $png"
} else {
  Write-Host "no framebuffer dump (was CROSSPOINT_DUMP_FB set?)"
}

