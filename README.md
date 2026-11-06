<p align="center"><img src="assets/readme/banner-en.svg" alt="fringefix" width="100%"></p>

<p align="center"><a href="https://github.com/RuttiTuttii/fringefix/releases/latest">Download</a> · <a href="#what-the-problem-looks-like">What the problem looks like</a> · <a href="#screenshots">Screenshots</a> · <a href="#support-the-project">Support</a> · <a href="README.ru.md">Русский</a></p>

<p align="center"><a href="https://github.com/RuttiTuttii/fringefix/actions/workflows/ci.yml"><img src="https://github.com/RuttiTuttii/fringefix/actions/workflows/ci.yml/badge.svg" alt="CI"></a></p>

A Windows overlay that shifts the colour channels of the finished frame by fractions of a pixel to cancel colour fringing on QD-OLED panels with the triangular subpixel layout (4th generation and earlier). Lives in the tray, tuned with sliders, updates from GitHub Releases.

> limitations: the project builds with mingw-w64 without warnings, the portable-module tests (12 suites) pass under ASan and UBSan, the report receiver was checked from outside on both addresses. Version 0.1.0 ran on one machine with a 4K QD-OLED in SDR: the tray, settings window and overlay worked. Everything added later has not run on Windows: the HDR path, the updater and version picker, the wizard, auto-pause and light mode, the dark theme, language and theme choice, statistics and report sending, the filter switch, the new test pattern, four auto-pause modes, the app lists window with the process picker, the beta channel, reopening windows after an update, start with Windows and the bundled profile. The frame comparison fix and the HDR surface format were checked on one machine by the tester. Requires Windows 10 2004 or newer.

## what the problem looks like

on first-to-fourth generation QD-OLED the subpixels form a triangle: green above red and blue. ClearType text assumes RGB stripes, so horizontal edges get a green and a pink fringe. Fringefix shifts every channel to where its subpixel really sits.

<p align="center"><img src="assets/readme/fringe-demo-en.svg" alt="Text on QD-OLED without correction with colour fringes and with fringefix correction" width="100%"></p>

## features

<table>
<tr><td width="50%" valign="top"><img src="assets/icons/shift.svg" width="44" height="44" alt=""><br><b>Channel shift</b><br>The shader moves red, green and blue by fractions of a pixel: 18 texture reads per pixel, redrawn only when the screen changes.</td><td width="50%" valign="top"><img src="assets/icons/wizard.svg" width="44" height="44" alt=""><br><b>Setup wizard</b><br>Three steps by panel type. Removed from the window until statistics from different panels arrive, the code stays in the build.</td></tr>
<tr><td width="50%" valign="top"><img src="assets/icons/games.svg" width="44" height="44" alt=""><br><b>Auto-pause in games</b><br>Four modes: fullscreen apps, 3D apps, both, off. A list of programs the filter is always applied to.</td><td width="50%" valign="top"><img src="assets/icons/profiles.svg" width="44" height="44" alt=""><br><b>JSON profiles</b><br>Shifts and sharpness are saved to a file and switched from the tray menu. A profile for QD-OLED gen 3 and 4 is built into the exe.</td></tr>
<tr><td width="50%" valign="top"><img src="assets/icons/versions.svg" width="44" height="44" alt=""><br><b>Versions and rollback</b><br>A release list, install of any version, SHA-256 check.</td><td width="50%" valign="top"><img src="assets/icons/privacy.svg" width="44" height="44" alt=""><br><b>Privacy</b><br>Statistics are local and off until you turn them on. The report is shown in full before sending.</td></tr>
<tr><td width="50%" valign="top"><img src="assets/icons/theme.svg" width="44" height="44" alt=""><br><b>Language and theme</b><br>Russian and English, light and dark theme, auto from the system or by hand.</td><td width="50%" valign="top"><img src="assets/icons/report.svg" width="44" height="44" alt=""><br><b>Reports</b><br>To the project server, through a GitHub issue or to a file.</td></tr>
</table>

## screenshots

<p align="center"><picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/screens/settings-en-dark.svg">
  <img src="assets/screens/settings-en-light.svg" alt="Settings window: channel shifts and sharpness" width="520">
</picture></p>

<p align="center"><picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/screens/program-en-dark.svg">
  <img src="assets/screens/program-en-light.svg" alt="Program settings window: language, theme, auto-pause, updates, start with Windows" width="520">
</picture></p>

<p align="center"><picture>
  <source media="(prefers-color-scheme: dark)" srcset="assets/screens/report-en-dark.svg">
  <img src="assets/screens/report-en-light.svg" alt="Report window with a JSON preview" width="640">
</picture></p>

## support the project


I make fringefix alone and it is free. If it was useful to you:

- star the repository on GitHub <img src="assets/star.svg" width="16" height="16" alt=""> it really helps: the project becomes more visible
- send any amount in crypto. Life in Russia is hard right now and even a small coin is very welcome and supports me a lot

| network | address |
| --- | --- |
| USDT on TON | `UQBHrA4BaeTkLgJ-R7KBFUJ6hoXHqCfvcmysTHboh3kxRSh4` |
| Bitcoin | `bc1qlsehpmp77xnfqmasr4tzhs3fsmtfjcwq75r2t6` |
| USDT on TRON (TRC-20) | `TB49TjDG5F9pDMiVEHAuVxdGtqmEnfEfyv` |

check the network before sending: a transfer to the wrong network cannot be recovered.

## running


- download `fringefix.exe` from the latest release and put it in any folder
- start it, a tray icon of three balls in a triangle appears (black and white means off or paused)
- double click opens settings, right click opens the menu with profiles
- the "Filter" switch at the bottom left of the settings window does the same as the toggle hotkey; with the filter off the picture is left unchanged
- `F10` toggles the overlay, `F11` quits. A global hotkey takes the key away from every program, `F11` is fullscreen in browsers: change both in the "Hotkeys" card of the program settings window or with `hotkey_toggle` and `hotkey_quit` in the ini. The format is `F1`-`F24` alone, or a letter or digit with `Ctrl`, `Alt`, `Shift`, `Win` joined by `+` (`Ctrl+Shift+F`), `none` turns a hotkey off. The Win key cannot be typed into a hotkey field of Windows, so here it is typed as text
- settings live in `%APPDATA%\fringefix\fringefix.ini`, the Save button writes them
- the "Start with Windows" checkbox in the Startup card (the "Settings" button) writes `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`, no administrator rights needed. The state is read from the registry: if the exe moved to another folder the checkbox is off, turning it on again updates the path

## tuning


1. press the test pattern button: the pattern window opens to the right of the settings window, keep the settings window on the monitor the overlay corrects. The button and a click on the pattern close it
2. move the vertical green slider until horizontal edges turn grey
3. move the horizontal red and blue sliders in opposite directions until vertical edges turn grey
4. the other shifts are usually unneeded on this panel: red and blue subpixels sit at the same height and green sits midway between them horizontally
5. press save

The window is split into cards by colour: red, green and blue each have a horizontal and a vertical shift, the Image card holds sharpness and the monitor choice. Everything is at zero until you change it.

The test pattern is as tall as the settings window and half as wide. The top half is light (black on white), the bottom half is dark (white on black). Each half has three bands: thin lines (1 pixel, 14 pixel pitch), thick lines (5 pixels, 10 pixel pitch) and the alphabet, lowercase letters first, then capitals. In the first two bands horizontal lines are on the left and vertical lines on the right. Line thickness is in physical pixels and does not follow Windows scaling: a one pixel line is the subpixel probe. The pitch is given for 100% and grows with the scaling of the settings window (at 150% a pitch of 14 becomes 21). The pattern moves with the settings window.

- a positive value moves the channel content right and down, range is -2 to 2 pixels, step 0.01
- a value can be typed into the field right of the slider, dot and comma both work, the change applies at once
- a double click on a slider resets it to zero, the mouse wheel moves it by 0.01 (0.1 with Shift), arrows by 0.01, PageUp and PageDown by 0.1, Home and End to the ends
- sharpness is from -1 to 1: plus strengthens contours, minus softens them

A sub-pixel shift goes through bilinear interpolation, which blurs. The shader restores sharpness up front with a laplacian of strength `min(1.72 * f^2, 0.35)`, where `f` is the fractional part of the shift. So with non-zero shifts a small contour boost exists even at sharpness 0, and with zero shifts there is none. Sharpness -1 cancels the maximum of that boost completely.

## setup wizard


> there is no wizard button in the window now: it was removed until statistics from different panels arrive. The `ui_wizard.c` file builds but is not called from the interface.

three steps:

1. panel type: QD-OLED with triangular subpixels (gen 1-4) or RGB stripes where no correction is needed
2. vertical strength with the test pattern open: look at the horizontal lines
3. horizontal strength: look at the vertical lines in the left half

for the triangular layout the shifts are computed from the geometry: red and blue sit side by side at the bottom, green above them in the middle, offsets from the pixel centre are 0.25 each. A channel must show the picture where it physically sits, so the shift is minus the offset: green vertical +0.25, red and blue vertical -0.25, red horizontal +0.25, blue -0.25. The vertical signs match the values the original's author found by hand (green plus, red and blue minus).

both strengths go from -3 to 3 and scale the offsets along their axis, a minus mirrors the layout: useful if the panel has green at the bottom or red and blue the other way round. Finish keeps the values and saves the settings, Cancel restores the previous shifts. The horizontal strength often has to be reduced: ClearType already shifts text horizontally for RGB stripes.

the calculation follows a panel model, not a measurement: there is no automatic fringing measurement, for example from a phone photo.

## auto-pause


the mode is chosen in the Auto-pause rules card of the program settings window (the "Settings" button), the `auto_pause` key in the ini:

- 0 off
- 1 in fullscreen apps (default)
- 2 in fullscreen and 3D apps
- 3 in 3D apps only

the list shows the modes in this order: fullscreen, 3D, both, off.

a fullscreen app is an active window with no title bar or frame that covers the whole overlay monitor (2 pixel slack): a borderless game, exclusive fullscreen, F11 video in a browser. A 3D app is the active window's process that loaded d3d9, d3d11, d3d12, vulkan-1 or opengl32 and is not in the built-in list of known non-games (browsers, editors, messengers, office, the shell). Windows built on the Chromium engine (Electron, CEF) are never counted as 3D apps: they load d3d11 for compositing but are not games. A process protected from reading (anti-cheat) is detected by fullscreen only, so in mode 3 it is not detected at all.

- the check runs every 0.5 seconds, the pause is lifted after two consecutive checks without a game so alt-tab back and forth does not recreate the device
- while paused the D3D device and the overlay window are released and the tray icon is grey
- why: an overlay over a fullscreen game takes away its direct output that bypasses DWM composition (independent flip, per the documentation), adds a frame of latency and loads the GPU itself

## latency


- the device frame queue is limited to one frame instead of the default three
- the rest of the latency is built into the scheme: the desktop is captured after DWM composition, the overlay is drawn and composed again, at least one or two frames. For games the answer is auto-pause
- I did not measure values in milliseconds

## auto-pause rules


the Auto-pause rules card of the program settings window has three items:

- "Auto-pause": when to consider a game running (fullscreen, 3D, fullscreen and 3D, off)
- "Action in games": it can be changed only while auto-pause is off, in any other mode it shows a locked "Pause". "Pause" releases the device and the overlay window, "Light mode" keeps the overlay but the shader does one shift per channel without the laplacian and sharpness and costs almost nothing. Light mode does not remove DWM composition, so only a pause lifts the game latency. With auto-pause off and light mode, games are looked for as in the "fullscreen and 3D apps" mode and light mode is used instead of a pause; with auto-pause off and "Pause" nothing is looked for
- "App lists...": a window with two lists, "Always apply the filter" (`ignore_apps`: photo viewers, players, whatever auto-pause is set to) and "Always pause" (`pause_apps`). The "+ Add..." button opens a picker with the running programs that have a visible window; the search box filters by process name or by window title. An app that is not running can be typed in by hand: when no row matches, the typed name is added. "− Remove" deletes the selected row. An app is in one list at a time: adding it to one list removes it from the other. The name must be Latin, each list takes up to 1023 bytes

more in `fringefix.ini`:

- `pause_apps = game.exe;other.exe` always treat as a game even when the window is not fullscreen; a name without `.exe` also fits, case is ignored
- `ignore_apps = player.exe` never treat as a game, for example a fullscreen video player
- `gpu_priority = -3` GPU priority of the overlay thread from -7 to 7; below zero the overlay yields the GPU to the game, which reduces the FPS cost in light mode

## versions and rollback


the "Versions..." button in the program settings window opens a list of the repository releases (up to the latest 30 that have `fringefix.exe` and `fringefix.exe.sha256`). Picking a version newer than the current one installs an update, older installs a rollback. Versions are not compared during installation: the hash check and the exe swap are the same as in a normal update, settings in `%APPDATA%` stay. Betas are hidden in the list until the "Get beta versions" checkbox is on. Old releases are not overwritten: to go back to a version it must stay in Releases.

## profiles


- "Save profile..." and "Load profile..." work with JSON files, the default folder is `%APPDATA%\fringefix\profiles`
- the tray menu item "Profiles" lists the files from that folder, picking one applies it at once
- a profile holds only the look of the picture: the three channel shifts and sharpness, the monitor and the update check stay as they were
- the bundled profile "QD-OLED gen 3 4" is built into the exe and put into the profiles folder at start if that file is not there: an edited file is not overwritten, a deleted one comes back at the next start. The files live in `profiles/`, a new profile is a line in `src/res/fringefix.rc`, a number in `include/resource.h` and an entry in the `BUNDLED` table in `src/profile_io.c`

```json
{
  "format": "fringefix-profile",
  "version": 1,
  "red": { "x": 0.000, "y": 0.000 },
  "green": { "x": 0.000, "y": 0.250 },
  "blue": { "x": -0.150, "y": 0.000 },
  "sharpen": 0.000
}
```

Out-of-range values are clamped, broken ones are skipped, a file without the `format` field is rejected whole.

## language, theme and icons


- "Settings" in the settings window opens the program settings window: language, theme, auto-pause, updates, start with Windows
- language: "Auto" follows the Windows display language: Russian only on a Russian Windows, English on any other, European languages included, "Русский" and "English" are picked by hand; every list item has a flag
- theme: "Auto" follows the Windows "app mode" setting, "Light" and "Dark" are set by hand; a change applies to open windows at once
- a language change recreates the windows, settings are kept
- the slider accent colour comes from the system one
- the application icon: three balls in a triangle, red and blue on top, green below, as in a macro photo of QD-OLED subpixels. The drawings live in `assets/` as SVG: `icon.svg` with a tile that turns light in the light theme, `mark.svg` without a tile for small sizes, and the flags in `assets/flags/`. The drawing is flat, with no gradients or glow
- the icon of a disabled overlay is the same drawing under a monochrome filter
- the exe ICO files are built from the SVG with `make icons` (needs `rsvg-convert`), the ready ones are in `src/res/`

## statistics and reports


everything below is off until you turn it on yourself. Nothing is sent on its own.

- in the program settings window the "Report and statistics..." button opens the report window
- the "Keep statistics on this computer" checkbox turns on local counters: frames, skipped echo frames, device recreations, pauses, overlay errors and the games that caused a pause. The `stats.txt` file sits next to the settings in `%APPDATA%\fringefix` and goes nowhere
- the window shows exactly the JSON text that would go over the network; you can untick the System, Settings and Statistics sections and add a comment
- there are three ways to send: to the project server with the Send button (with a confirmation), through a new GitHub issue (the report is copied to the clipboard and you paste it into the issue yourself), or just save it to a file
- "Clear statistics" resets the counters and deletes the file

what the report holds: the program version and language; the Windows build number, the GPU name, the monitor resolution and an HDR flag; channel shifts, sharpness, auto-pause mode, the action in games and GPU priority; counters; overlay error texts; for games only the exe name without a path, how it was detected, what the overlay did, the number of sessions and minutes; your comment.

what the report does not hold: user name, computer name, file paths, window titles, monitor serial numbers, a permanent identifier and the send time.

the receiver on the server checks the report against a whitelist of fields, limits the size and the rate (5 per hour per address), stores each report as its own file with mode 0600 and records the client address nowhere: the rate limit keeps only a salt and a hash in memory. The receiver code and instructions are in `server/`. The receiving address goes into the exe at build time from the `REPORT_URL` variable and is not in the sources; if it is not set, the Send button is disabled and the other ways keep working. The server still sees the network address at the moment of the request, which is a property of any network connection.

## how it works

<p align="center"><img src="assets/readme/pipeline-en.svg" alt="Diagram: desktop, frame capture, frame signature, shader, overlay window" width="100%"></p>


```
main.c ─ ui_tray.c ─ ui_settings.c ─ ui_controls.c, ui_paint.c, slider.c, theme.c
  │         │            └─ ui_profile.c, ui_wizard.c ─ layout.c, pattern.c
  │         │            └─ ui_versions.c, ui_app.c, ui_prefs.c, autostart.c
  │         │            └─ ui_report.c ─ report_actions.c, report_send.c, report.c
  │         └─ ui_update.c ─ update.c, update_install.c ─ http.c, sha256.c, release.c
  └─ app.c ─ config.c, profile.c, profile_io.c, i18n.c, stats.c, stats_store.c
       └─ capture.c (own thread) ─ gamedetect.c ─ applist.c
            └─ gfx.c, gfx_frame.c, gfx_sig.c ─ shader.c, output.c
```

- the frame comes from `IDXGIOutput6::DuplicateOutput1`, format follows the desktop mode: 8-bit in SDR, FP16 scRGB in HDR
- the window uses `WDA_EXCLUDEFROMCAPTURE`, so it does not appear in the desktop snapshot
- capture and overlay run in their own thread: menus, dialogs and window dragging do not freeze the picture
- the overlay still wakes capture: per comments in the original's source, DWM recomposes the desktop after every `Present`. Without a guard each draw would trigger the next one at full refresh rate
- so each frame is classified first: cursor-only is skipped, scrolls and small areas (under 1/50 of the screen) are drawn at once, anything else is compared by a signature (a hash over a 32 by 24 point grid computed on the GPU, 4 bytes read on the CPU) and skipped if unchanged
- every 2 seconds the window is put back on top of the z-order
- each channel costs one hardware bilinear read plus 5 laplacian reads, 18 reads per pixel; at 4K 240 Hz that is about 36 billion reads per second (calculated, not measured)

## updates


- on start the program queries `api.github.com/repos/<repo>/releases/latest` and compares the `vX.Y.Z` tag with the build version
- if newer, it asks for permission, downloads `fringefix.exe` and `fringefix.exe.sha256` from the same release and checks SHA-256
- only links of the form `https://github.com/<repo>/releases/download/` are accepted, others are dropped
- the old exe is renamed to `fringefix.exe.old`, the new one takes its place, the program restarts
- the startup check is turned off by the checkbox in settings or `check_updates = 0` in the ini
- the "Get beta versions" checkbox (`beta = 1` in the ini) makes the check take the newest release by number from the whole list, betas included. A beta is a release with a tag `vX.Y.Z-beta.N` marked as a pre-release on GitHub: `releases/latest` never returns it, so users without the checkbox are not offered betas. `v0.5.0-beta.2` is newer than `v0.5.0-beta.1` and older than `v0.5.0`. The version list shows betas only with the checkbox on. If a beta is installed and the checkbox is turned off, the check offers the newest stable release instead, even if it is older by number (a rollback). The windows that were open (settings, program, app lists) reopen at the same places after an update or rollback restart
- a non-tag build (`dev`) does not check for updates

Protection is limited to TLS to github.com and a hash match against a file from the same release. If the account or the release is compromised, the hash is forged together with the exe. There is no code signing.

## build and release


```bash
# tests for config, versions, release links and SHA-256, run on any system
make test
```

```bash
# the same tests under ASan and UBSan, warnings as errors; CC=clang works too
make test SAN=1 WERROR=1
```

```bash
# Windows build with the mingw-w64 cross compiler, REPO enables update checks
make win REPO=owner/fringefix
```

```bash
# native build in msys2 UCRT64
make win CC_WIN=gcc WINDRES=windres REPO=owner/fringefix
```

```bash
# release: a vX.Y.Z tag runs the GitHub Actions build and publishes the exe with its sha256 file
git tag v0.1.0 && git push origin v0.1.0
```

## CI on GitHub Actions


- `linux`: tests of the portable modules on Ubuntu with two compilers (gcc and clang) under ASan and UBSan, warnings as errors
- `cross`: a full exe build with the mingw-w64 cross compiler on Ubuntu, warnings as errors, the imported DLL list in the log, the exe in artifacts
- `build`: the release build in msys2 UCRT64 on a Windows runner
- `server`: tests and ruff for the report receiver in Python
- `release`: on a `vX.Y.Z` tag, only if all four previous jobs are green

the application runs on Windows only: capture through DXGI Desktop Duplication and output through Direct3D 11 do not port to Linux. On Linux the CI checks the code and builds the exe but does not run it.

## not done


- automatic fringing measurement, for example from a photo of the screen: the wizard follows a panel model
- layouts other than the QD-OLED triangle: WOLED, for example, arranges subpixels differently
- text-only mask for sharpening: sharpening applies to the whole screen
- rotated monitors
- code signing and release signing
- a Linux version: planned (a GNOME Shell extension, a KWin effect or a Hyprland shader), capture through DXGI does not port to Linux
- languages other than Russian and English
- the overlay adds one frame of latency over the desktop

[GPL-3.0](LICENSE)
