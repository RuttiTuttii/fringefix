# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow the `vX.Y.Z` tags. Betas carry `-beta.N`.

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
