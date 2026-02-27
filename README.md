# tpkb

<p align="center">
  <img src="art/icon-run.png" width="128">
</p>

[![Build](https://github.com/li-ruijie/tpkb/actions/workflows/build.yml/badge.svg)](https://github.com/li-ruijie/tpkb/actions/workflows/build.yml)

TrackPoint keyboard helper for Windows. Converts TrackPoint mouse button presses or drags into scroll wheel events, and provides fine-grained control over keyboard repeat rates beyond what Windows settings allow. Runs as a system tray application.

Also available as an [F# (.NET) version](https://github.com/li-ruijie/tpkb-fs).

No runtime dependencies. Single statically linked executable.

## Features

- **Scroll emulation** — Convert mouse button or TrackPoint button gestures into scroll wheel events
- **Multiple trigger modes** — Single button, button combinations, drag gestures, or keyboard hotkey
- **Scroll acceleration** — Speed-dependent multipliers with presets or custom curves
- **Real wheel mode** — Generate actual `WM_MOUSEWHEEL` events for applications that require them
- **VH adjuster** — Lock scrolling to vertical or horizontal axis
- **Keyboard repeat tuning** — Set repeat delay and rate via Windows Filter Keys, allowing faster repeat rates than the standard Windows control panel permits (e.g. 16 ms = ~60 keys/sec)
- **Configuration profiles** — Multiple named INI profiles with hot-switching
- **Hook health monitoring** — Automatic detection and recovery from silently dropped hooks

## Requirements

- Windows 10/11

## Installation

Download the latest release from [Releases](https://github.com/li-ruijie/tpkb/releases), or build from source.

## Usage

Run `tpkb.exe`. The application appears in the system tray.

- **Right-click** the tray icon to access settings and options
- **Double-click** the tray icon to toggle Pass Mode (disable/enable scrolling)

### Command line

| Parameter          | Description                                             |
|--------------------|---------------------------------------------------------|
| `--sendExit`       | Send exit signal to running instance                    |
| `--sendPassMode`   | Toggle pass mode (add `true`/`false` to set explicitly) |
| `--sendReloadProp` | Reload properties file in running instance              |
| `--sendInitState`  | Reset internal state of running instance                |
| `<name>`           | Load named properties file on startup                   |

**Examples:**

```
tpkb.exe --sendExit           # Close running instance
tpkb.exe --sendPassMode true  # Enable pass mode
tpkb.exe --sendPassMode false # Disable pass mode
tpkb.exe --sendReloadProp     # Reload current properties
tpkb.exe MyProfile            # Start with "MyProfile" properties
```

## Settings

Right-click the tray icon and select **Settings...** to open the settings dialog. Settings are organized into seven tabbed pages described below.

Settings are stored in `%USERPROFILE%\.config\tpkb\tpkb.ini`. Named profiles are stored as `tpkb.<name>.ini` in the same directory.

### General

#### Trigger

Determines how scroll mode is activated.

| Trigger    | Description                                |
|------------|--------------------------------------------|
| LR         | Press Left+Right simultaneously            |
| Left       | Press Left then Right button               |
| Right      | Press Right then Left button               |
| Middle     | Press Middle button                        |
| X1         | Press X1 (Back) button                     |
| X2         | Press X2 (Forward) button                  |
| LeftDrag   | Hold Left button and drag                  |
| RightDrag  | Hold Right button and drag                 |
| MiddleDrag | Hold Middle button and drag                |
| X1Drag     | Hold X1 button and drag                    |
| X2Drag     | Hold X2 button and drag                    |
| None       | Disable mouse triggers (hotkey only)       |

- **Send MiddleClick** — Send a middle click if trigger buttons are pressed and released without scrolling. Property: `sendMiddleClick`
- **Dragged Lock** — For drag triggers: keep scroll mode active after releasing; click again to exit. Property: `draggedLock`

#### Hotkey

- **Enable** — Hold a keyboard key to enter scroll mode. Property: `keyboardHook`
- **VK Code** — The trigger key. Property: `targetVKCode` (default: `VK_NONCONVERT`)

#### System

- **Priority** — Process priority level. Property: `processPriority` (default: `AboveNormal`)
- **Health check interval** — Seconds between hook health checks. Detects and reinstalls silently dropped hooks. Set to 0 to disable. Property: `hookHealthCheck` (default: 0, range: 0–300)

### Scroll

- **Cursor Change** — Show a scroll cursor while in scroll mode. Property: `cursorChange`
- **Horizontal Scroll** — Enable horizontal scrolling. Property: `horizontalScroll`
- **Reverse Scroll** — Invert scroll direction (natural scrolling). Property: `reverseScroll`
- **Swap Scroll (V/H)** — Swap vertical and horizontal axes. Property: `swapScroll`
- **Button press timeout** — Milliseconds for second button in LR/Left/Right modes. Property: `pollTimeout` (default: 200, range: 50–500)
- **Scroll lock time** — Minimum ms scroll mode stays active. Property: `scrollLocktime` (default: 200, range: 150–500)
- **Vertical threshold** — Minimum vertical movement in pixels. Property: `verticalThreshold` (default: 0)
- **Horizontal threshold** — Minimum horizontal movement in pixels. Property: `horizontalThreshold` (default: 75)
- **Drag threshold** — Minimum movement before drag triggers activate. Property: `dragThreshold` (default: 0)

### Acceleration

- **Enable acceleration** — Speed-dependent scroll multipliers. Property: `accelTable`
- **Preset** — Predefined curves M5–M9. Property: `accelMultiplier` (default: `M5`)
- **Custom table** — User-defined threshold/multiplier arrays. Properties: `customAccelThreshold`, `customAccelMultiplier`

### Real Wheel

- **Enable real wheel mode** — Generate `WM_MOUSEWHEEL`/`WM_MOUSEHWHEEL` instead of `WM_VSCROLL`/`WM_HSCROLL`. Property: `realWheelMode`
- **Wheel delta** — Delta per scroll event (standard: 120). Property: `wheelDelta` (default: 120)
- **Vertical/Horizontal speed** — Pixels per wheel event. Properties: `vWheelMove`, `hWheelMove` (default: 60)
- **Quick first scroll** — Send first event immediately. Property: `quickFirst`
- **Quick direction change** — Reset accumulator on direction reversal. Property: `quickTurn`

### VH Adjuster

Constrains scrolling to one axis based on dominant movement direction.

- **Fixed** — Locks direction from initial movement until scroll mode exits.
- **Switching** — Dynamically switches direction during scroll mode.

Properties: `vhAdjusterMode`, `vhAdjusterMethod` (default: `Switching`), `firstPreferVertical`, `firstMinThreshold`, `switchingThreshold`

### Keyboard

Controls keyboard repeat behaviour using the Windows Filter Keys accessibility feature. This allows setting repeat rates faster than the standard Windows keyboard settings permit — for example, a repeat rate of 16 ms (~60 keys/sec) compared to the Windows maximum of ~30 keys/sec.

- **Character repeat delay** — How long to hold a key before auto-repeat starts (0=shortest, 3=longest). Property: `kbRepeatDelay` (default: 0, range: 0–3)
- **Character repeat speed** — Auto-repeat rate (31=fastest at ~30 char/sec, 0=slowest). Property: `kbRepeatSpeed` (default: 31, range: 0–31)
- **Enable Filter Keys** — Override keyboard repeat via the Filter Keys feature. When off, Windows reverts to standard repeat behaviour. Property: `filterKeys`
- **Lock** — Reapply keyboard settings on startup if the system state has changed (e.g. after reboot). Property: `fkLock`
- **Acceptance delay** — Time in ms a key must be held before it registers. Set to 0 for immediate. Property: `fkAcceptanceDelay` (default: 0, range: 0–10000)
- **Repeat delay** — Time in ms a key must be held before auto-repeat begins. Property: `fkRepeatDelay` (default: 0, range: 0–10000)
- **Repeat rate** — Interval in ms between repeated keystrokes. Lower = faster. Property: `fkRepeatRate` (default: 16, range: 0–10000)
- **Bounce time** — Time in ms to ignore duplicate presses after release. Cannot be used with the other timing fields. Property: `fkBounceTime` (default: 0, range: 0–10000)

### Profiles

- **Reload** — Reload current profile from disk.
- **Save** — Save current settings to the active profile.
- **Open Dir** — Open the configuration directory in Explorer.
- **Add** — Create a new named profile.
- **Delete** — Delete the selected profile.

## Building

Requires VS 2026 Build Tools (C/C++ workload) and CMake 3.20+.

**MSVC:**
```
build.bat
```

**MinGW:**
```
build-mingw.bat
```

## License

GPL-3.0

## Credits

- **Original Author:** Yuki Ono (2016-2021) - [W10Wheel.NET](https://github.com/ykon/w10wheel.net)
- **Fork & C Port:** Li Ruijie (2026-)
