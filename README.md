# W10Wheel

Pure C port of [W10Wheel.NET](https://github.com/li-ruijie/w10wheel-net), a mouse wheel simulator for Windows. Converts mouse button presses or drags into scroll wheel events. Runs as a system tray application.

No runtime dependencies. Single statically linked executable.

## Requirements

- Windows 10/11
- Enable "Scroll inactive windows when I hover over them" in Settings > Devices > Mouse

## Installation

Download the latest release from [Releases](https://github.com/li-ruijie/w10wheel/releases), or build from source.

## Usage

Run `W10Wheel.exe`. The application appears in the system tray.

- **Right-click** the tray icon to access settings
- **Double-click** the tray icon to toggle Pass Mode

### Command Line

| Parameter          | Description                                             |
|--------------------|---------------------------------------------------------|
| `--sendExit`       | Send exit signal to running instance                    |
| `--sendPassMode`   | Toggle pass mode (add `true`/`false` to set explicitly) |
| `--sendReloadProp` | Reload properties file in running instance              |
| `--sendInitState`  | Reset internal state of running instance                |
| `<name>`           | Load named properties file on startup                   |

## Settings

Settings are stored in `%USERPROFILE%\.config\w10wheel\w10wheel.conf`.

Named profiles are stored as `w10wheel.<name>.conf` in the same directory.

Compatible with the F# version's config format.

### Triggers

| Trigger    | Description                                       |
|------------|---------------------------------------------------|
| LR         | Press Left then Right (or Right then Left) button |
| Left       | Press Left then Right button                      |
| Right      | Press Right then Left button                      |
| Middle     | Press Middle button                               |
| X1         | Press X1 (Back) button                            |
| X2         | Press X2 (Forward) button                         |
| LeftDrag   | Hold Left button and drag                         |
| RightDrag  | Hold Right button and drag                        |
| MiddleDrag | Hold Middle button and drag                       |
| X1Drag     | Hold X1 button and drag                           |
| X2Drag     | Hold X2 button and drag                           |
| None       | Disable mouse triggers (keyboard trigger only)    |

### Properties Reference

| Property              | Default       | Range   | Description                            |
|-----------------------|---------------|---------|----------------------------------------|
| `firstTrigger`        | `LR`          | -       | Trigger mode                           |
| `keyboardHook`        | `false`       | -       | Enable keyboard trigger                |
| `targetVKCode`        | `VK_NONCONVERT` | -    | Keyboard trigger key                   |
| `pollTimeout`         | `200`         | 50-500  | LR simultaneous press timeout (ms)     |
| `scrollLocktime`      | `200`         | 150-500 | Scroll mode lock time (ms)             |
| `verticalThreshold`   | `0`           | 0-500   | Minimum vertical movement to scroll    |
| `horizontalThreshold` | `75`          | 0-500   | Minimum horizontal movement to scroll  |
| `dragThreshold`       | `0`           | 0-500   | Minimum movement before drag activates |
| `cursorChange`        | `true`        | -       | Change cursor during scroll mode       |
| `horizontalScroll`    | `true`        | -       | Enable horizontal scrolling            |
| `reverseScroll`       | `false`       | -       | Reverse scroll direction               |
| `swapScroll`          | `false`       | -       | Swap vertical/horizontal axes          |
| `sendMiddleClick`     | `false`       | -       | Send middle click on quick release     |
| `draggedLock`         | `false`       | -       | Lock scroll mode after drag            |
| `accelTable`          | `true`        | -       | Enable acceleration table              |
| `accelMultiplier`     | `M5`          | -       | Acceleration preset (M5-M9)            |
| `customAccelTable`    | `false`       | -       | Use custom acceleration values         |
| `realWheelMode`       | `false`       | -       | Simulate real wheel events             |
| `wheelDelta`          | `120`         | 10-500  | Wheel delta per event                  |
| `vWheelMove`          | `60`          | 10-500  | Vertical movement per wheel event      |
| `hWheelMove`          | `60`          | 10-500  | Horizontal movement per wheel event    |
| `quickFirst`          | `false`       | -       | Send first wheel event immediately     |
| `quickTurn`           | `false`       | -       | Faster response to direction changes   |
| `vhAdjusterMode`      | `false`       | -       | Enable VH adjuster                     |
| `vhAdjusterMethod`    | `Switching`   | -       | VH method: `Fixed` or `Switching`      |
| `firstPreferVertical` | `true`        | -       | Prefer vertical on initial move        |
| `firstMinThreshold`   | `5`           | 1-10    | Minimum movement for direction detect  |
| `switchingThreshold`  | `50`          | 10-500  | Movement to switch direction           |
| `processPriority`     | `AboveNormal` | -       | `Normal`, `AboveNormal`, or `High`     |
| `uiLanguage`          | `en`          | -       | `en` or `ja`                           |

## Building

Requires VS 2026 Build Tools (C/C++ workload) and CMake 3.20+.

```
build.bat
```

Output: `build\W10Wheel.exe`

MinGW GCC is also supported via CMakeLists.txt.

## License

GPL-3.0

## Credits

- **Original Author:** Yuki Ono (2016-2021) - [W10Wheel.NET](https://github.com/ykon/w10wheel.net)
- **Fork & C Port:** Li Ruijie (2026-)
