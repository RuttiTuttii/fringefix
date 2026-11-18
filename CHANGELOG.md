# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow the `vX.Y.Z` tags. Betas carry `-beta.N`.

## v0.8.0-beta.7 (2026-11-19)

### Added

- after a crash the program starts again by itself, at most three times in a row (a manual start resets the count); the log has a line about it
- `fringefix.log` lists the names of all modules of the process at start and a minute after it: it shows the overlays and hooks that attached to the program (RTSS, the NVIDIA overlay, a `dxgi.dll` proxy)
- the "alive" line of a monitor thread is now written also while the thread retries the device start or sits on pause, and it shows whether the monitor is on
- a "Pause on all monitors when the app is open on any of them" checkbox in the Program window (key `pause_all_monitors`): a fullscreen or 3D app on one monitor pauses the filter on the other ones too; without it the pause stays as before, on the monitor with the app only
- `fringefix.ini` holds the output names (`mon0_device`, `mon1_device`): the ReShade add-on without a running program takes the settings of the monitor the game runs on, not the one selected in the program window
- a watchdog for hangs: a capture thread that stays 15 seconds in the start or the closing of a device (a driver or an overlay hook did not return from a call) is written to `fringefix.log` and `crash.log` with the place where each thread stands, `crash.dmp` is written, and the program starts again the same way as after a crash; before, a hang stayed until the process was killed by hand. The frame steps (acquire, copy, compare, draw, present) are not watched: they can wait for a good reason, for example while a monitor sleeps
- the start of a device marks its steps (device, output duplication, window, swap chain, shaders): after a hang the log shows the call it stood in
- when auto-pause starts on a fullscreen window, the log line carries the class, the styles and the rectangle of that window

### Changed

- ReShade add-on: a game window stretched over several monitors gets the settings of the monitor that holds most of it, and the add-on window shows a line about it; the add-on cannot apply different settings to parts of one picture
- the master switch of the old versions is gone from the program: no "master switch off" in the monitor list and under the filter switch, no command from the add-on slider to the program. What is left works per monitor: the switch at the bottom of the settings window, `F10` and the tray item, which now reads "Turn on the filter of the selected monitor" and acts like `F10`
- the tray icon and tip show whether the filter works on at least one monitor
- a monitor with all shifts and sharpness at zero is not processed: no device and no overlay window are created for it, its filter switch is inactive and the line under it says "not applied, all values are zero", the monitor list marks it "values are zero", `F10` and the tray item leave it alone. The switch keeps its position, so moving any slider brings the filter back as it was. On the first start, with default settings, the overlay therefore does not start until a value changes; the "Run" test of the diagnostics window still measures with zero values
- `F10` in a game with the ReShade add-on now belongs to the add-on alone: it turns the filter in that game on and off (the add-on reads the key itself, also while the program runs, and also with a hotkey changed in the ini), and the program leaves the desktop as it was. The add-on slider "Filter" does the same and is inactive at zero values. Install the add-on again from the program: the slider of the old add-on no longer reaches the program
- a pause that `NVIDIA Overlay.exe` (the overlay on Alt+Z) caused holds for 8 seconds after the last time its window was active
- a device start that failed is retried with growing pauses of 0.5, 1, 2 and 4 seconds instead of every 0.5 seconds; a settings change no longer cuts the pause short

### Fixed

- `F10` in a game with the add-on switched the filter off on all monitors including the desktop and left the monitor list with "master switch off"
- in the log of `v0.8.0-beta.6` the pause caused by `NVIDIA Overlay.exe` ended 1 to 4 seconds after it began, as soon as its window stopped being the active one, so the filter could come back over the overlay; the pause now holds for 8 seconds after the last activity (see the known issues)

### Known issues

- the NVIDIA overlay: the hold above is built from the log and has not been checked with the real overlay. In the same log both capture threads lost the capture at the same instant after Alt+Z, the device start then failed several times with `E_ACCESSDENIED` (`0x80070005`) and once did not return at all; the call that hangs is not found. The watchdog restarts the program in that case, `crash.log` will show the place. Send `fringefix.log`, `crash.log` and `crash.dmp` if it happens again
- RivaTuner Statistics Server hooks the process and crashes it in `dxgi.dll` with two monitors filtered; adding `fringefix.exe` to the RTSS exclusions removes the crash (confirmed for `v0.8.0-beta.6`)

## v0.8.0-beta.6 (2026-11-17)

### Added

- on a crash `crash.log` now has the exception name (for example `ACCESS_VIOLATION`, `STACK_OVERFLOW`, `HEAP_CORRUPTION`), `GetLastError`, the registers, the address and the kind of access, and where the other capture threads are at that moment (address in a module and return addresses from the exe); a `crash.dmp` with the stacks of all threads is written next to it
- `fringefix.log` gets the exceptions that a driver or a library swallows by itself (the first 64: code, module with offset, thread and its step); a stack overflow is always written, and threads keep a stack reserve for the write
- return codes that used to be lost: `Present` not `S_OK`, a failed read of the frame compare texture, a frame format change, a failed rebuild; each with the device removal reason (`GetDeviceRemovedReason`); the "alive" line of a thread now carries its step

## v0.8.0-beta.5 (2026-11-17)

### Added

- `fringefix.log` in `%APPDATA%\fringefix`: a work log, one line per event with a millisecond time, the thread number and the monitor; it holds the start and the Windows version, every output (name, position, rotation, HDR, video card), the start and stop of each monitor thread, device creation and closing with the reason, the autopause and the game that caused it, a change of the monitor layout, a capture loss with the error code and the device removal reason, and an "alive" line from each thread once a minute; a log over 1 MB goes to `fringefix.log.1` at start
- if the previous run ended without a clean exit (a killed process, a crash without an exception, a power loss), the log has a line about it at the next start
- a crash through `abort()` now reaches the log too, and on an exception the log keeps a line with the address and the last step of each thread; the details stay in `crash.log`

### Known issues

- the cause of the crash on two monitors is not found: each monitor already runs its own thread, and the shared places (device creation, the shader cache, the 3D app check cache) have been under a lock since `v0.8.0-beta.3`; this build is meant to locate the failure through `fringefix.log` and `crash.log`

## v0.8.0-beta.4 (2026-11-14)

### Changed

- the monitor list and the state line under the settings window show what really happens to the filter of each monitor: off, did not start, master switch off, or paused because a game or a 3D app is on that monitor
- the monitor switch and `F10` write `fringefix.ini` at once (except while a trial of the diagnostics window is running), so the choice of the filtered monitors survives a restart; before, it was kept only by the next "Save"

### Fixed

- the game crashed when the ReShade menu was opened with the add-on of v0.8.0-beta.1 to v0.8.0-beta.3 loaded: the add-on read the cursor position from ReShade with a calling convention different from the one ReShade is built with (an `ImVec2` result goes through a hidden pointer there), and ReShade wrote the result into a random address; the add-on is rebuilt with the matching call
- the filter switch of a monitor turned in mode B of the rotation did not work: it wrote into the settings set of the orientation, which does not hold the switch

## v0.8.0-beta.3 (2026-11-13)

### Added

- screen rotation: a monitor turned in Windows (portrait, flipped landscape) is filtered instead of stopping with an error; the "Screen rotation" list of the diagnostics window chooses mode A (the landscape settings, shift vectors rotate together with the panel) or mode B (a separate set of settings for each orientation, `rotation_mode` and `mon0_r1_red_x` keys); the monitor list marks a rotated monitor with the angle
- the "?" button next to the version line opens the "About" window
- `crash.log` in `%APPDATA%\fringefix`: after a crash it holds the exception, the address in the exe and the last step of each capture thread

### Changed

- the filter switch moved to the very bottom of the settings window and looks like a Windows switch; it acts only on the monitor chosen in the list
- `F10` toggles the filter of the selected monitor only; while the ReShade add-on works in a game it still acts on all monitors
- the "Image" card went under the "Blue" card as in the old versions
- the description in the "About" window is rewritten

### Removed

- the menu bar of the settings window; "What's new..." and the settings folder stay in the "About" window

### Fixed

- capture threads of different monitors no longer build shaders and create devices at the same time, and the shader cache and the 3D-app cache are guarded by a lock; the crash on dragging a window between two filtered monitors was not reproduced, `crash.log` will show its place if it remains

## v0.8.0-beta.2 (2026-11-11)

### Added

- a "Panel position" list in the diagnostics window: the measurement panel goes to any of the four corners of the monitor (the `hud_corner` key) so that it does not collide with, for example, the MSI Afterburner counter

### Fixed

- when the program, app filter, About and What's new windows are moved to a monitor with another Windows scale they are recreated in the new scale

## v0.8.0-beta.1 (2026-11-11)

### Added

- every monitor has its own settings: shifts, sharpness and the filter switch; pick a monitor in the list and edit only that one
- the program drives all monitors at once, one capture thread each; auto-pause is evaluated per monitor, `F10` and the tray switch all of them together
- a menu in the settings window: File, Edit, Tools, Help; Help has "What's new" and "About"
- the "ReShade integration" tab is highlighted with colour; the "Download the add-on to this PC" button saves the add-on and the `reshade-shaders` folder into a chosen folder
- the three ReShade buttons remember the last folder (the `reshade_dir` ini key)
- the add-on window inside a game has two sliders: filter and monitoring; the filter slider works together with `F10` and the tray

### Changed

- the settings file writes `mon0_`, `mon1_` and so on blocks; an old file is read as the settings of the monitor the overlay ran on
- a profile holds the shifts and sharpness of the selected monitor; reset clears only that monitor
- engine 2 (compute and 3x3 kernel) is the default engine; where channel shifts differ by more than a pixel it falls back to engine 1 by itself, because the kernel reads three times more pixels there
- auto-pause defaults to "In fullscreen and 3D apps", the items have a new order
- the "App lists" button is now "App filter"
- the add-on takes the settings of the monitor the game runs on
- the blocks of the program settings window are reordered; "Diagnostics and measurements" moved into "Updates and data"
- the diagnostics window is redrawn with a card and even margins

### Removed

- the light mode and the "Action in games" row; in games there is only a pause now

## v0.7.0-beta.2 (2026-11-07)

### Changed

- unsigned beta builds until a signing certificate exists

## v0.7.0-beta.1 (2026-11-07)

### Added

- ReShade add-on with live settings from the program, one-button install, the overlay pauses while the add-on works in a game

## v0.6.0-beta.1 (2026-11-06)

### Added

- three processing engines: classic, compute frame comparison, compute and 3x3 kernel
- the "Diagnostics and measurements" window: GPU stage times, latencies, power, a run over all engines, CSV

### Removed

- the setup wizard

## v0.5.3 (2026-11-05)

### Added

- app lists window with a pick from running processes
- betas are hidden until the checkbox is on; turning it off rolls back to the stable version
- windows return to their places after an update

## v0.5.2-beta.1 (2026-11-05)

### Changed

- frames are compared by every pixel instead of a sparse hash
- the frame format comes from the captured surface; rotated monitors are refused with a clear error

## v0.5.0 and v0.5.1 (2026-11-05)

### Added

- a beta channel and configurable hotkeys

## v0.4.0 (2026-11-05)

### Fixed

- atomic file writes, a BOM in the ini, `Win+Alt+F` and `Win+Alt+Q` hotkeys
- autostart, updates, statistics and game detection

## v0.3.0 (2026-11-04)

### Added

- the program window, report and local statistics, versions with rollback, GPU priority and app lists

## v0.2.0 (2026-11-04)

### Added

- auto-pause in fullscreen and 3D apps, a grey tray icon while paused
- JSON profiles, Russian and English, theme

## v0.1.0 (2026-11-04)

- first release: overlay with channel shift, tray, settings window, updates with a sha256 check
