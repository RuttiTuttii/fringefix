# Changelog

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), versions follow the `vX.Y.Z` tags. Betas carry `-beta.N`.

## v0.8.0 (upcoming)

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
