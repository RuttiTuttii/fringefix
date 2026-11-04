# fringefix

[Русский](README.ru.md)

A Windows overlay that shifts the colour channels of the finished frame by fractions of a pixel to cancel colour fringing on QD-OLED panels with the triangular subpixel layout (4th generation and earlier). Lives in the tray, tuned with sliders, updates from GitHub Releases.

> limitations: the project builds with mingw-w64 without warnings and the portable-module tests pass (`make test`). Version 0.1.0 ran on one machine with a 4K QD-OLED in SDR: the tray, settings window and overlay worked. The HDR path, updater, version picker, dark theme, typed values, profiles, the wizard, auto-pause and light mode are unverified. Requires Windows 10 2004 or newer. Updates work only with a public repository: for a private one the GitHub API answers 404 without a token.

## running

- download `fringefix.exe` from the latest release and put it in any folder
- start it, a tray icon of three coloured bars appears (grey means off)
- double click opens settings, right click opens the menu with profiles
- `Ctrl+Alt+F` toggles the overlay, `Ctrl+Alt+Q` quits
- settings live in `%APPDATA%\fringefix\fringefix.ini`, the Save button writes them

## tuning

1. press the test pattern button: lines on the left and text in the middle open on the chosen monitor
2. move the vertical green slider until horizontal edges turn grey
3. move the horizontal red and blue sliders in opposite directions until vertical edges turn grey
4. the other shifts are usually unneeded on this panel: red and blue subpixels sit at the same height and green sits midway between them horizontally
5. press save

The window is split into cards by colour: red, green and blue each have a horizontal and a vertical shift, the Image card holds sharpness and the monitor choice. Everything is at zero until you change it.

- a positive value moves the channel content right and down, range is -2 to 2 pixels, step 0.01
- a value can be typed into the field right of the slider, dot and comma both work, the change applies at once
- a double click on a slider resets it to zero, the mouse wheel moves it by 0.01 (0.1 with Shift), arrows by 0.01, PageUp and PageDown by 0.1, Home and End to the ends
- sharpness is from -1 to 1: plus strengthens contours, minus softens them

A sub-pixel shift goes through bilinear interpolation, which blurs. The shader restores sharpness up front with a laplacian of strength `min(1.72 * f^2, 0.35)`, where `f` is the fractional part of the shift. So with non-zero shifts a small contour boost exists even at sharpness 0, and with zero shifts there is none. Sharpness -1 cancels the maximum of that boost completely.

## setup wizard

the "Setup wizard..." button in the window. Three steps:

1. panel type: QD-OLED with triangular subpixels (gen 1-4) or RGB stripes where no correction is needed
2. vertical strength with the test pattern open: look at the horizontal lines in the right half
3. horizontal strength: look at the vertical lines in the left half

for the triangular layout the shifts are computed from the geometry: red and blue sit side by side at the bottom, green above them in the middle, offsets from the pixel centre are 0.25 each. A channel must show the picture where it physically sits, so the shift is minus the offset: green vertical +0.25, red and blue vertical -0.25, red horizontal +0.25, blue -0.25. The vertical signs match the values the original's author found by hand (green plus, red and blue minus).

both strengths go from -3 to 3 and scale the offsets along their axis, a minus mirrors the layout: useful if the panel has green at the bottom or red and blue the other way round. Finish keeps the values and saves the settings, Cancel restores the previous shifts. The horizontal strength often has to be reduced: ClearType already shifts text horizontally for RGB stripes.

the calculation follows a panel model, not a measurement: there is no automatic fringing measurement, for example from a phone photo.

## auto-pause

the mode is chosen in the Image card, the `auto_pause` key in the ini:

- 0 off
- 1 in fullscreen apps (default)
- 2 in fullscreen and 3D apps

a fullscreen app is an active window with no title bar or frame that covers the whole overlay monitor (2 pixel slack): a borderless game, exclusive fullscreen, F11 video in a browser. A 3D app is the active window's process that loaded d3d9, d3d11, d3d12, vulkan-1 or opengl32 and is not in the built-in list of known non-games (browsers, editors, messengers, office, the shell). A process protected from reading (anti-cheat) is detected by fullscreen only.

- the check runs every 0.5 seconds, the pause is lifted after two consecutive checks without a game so alt-tab back and forth does not recreate the device
- while paused the D3D device and the overlay window are released and the tray icon is grey
- why: an overlay over a fullscreen game takes away its direct output that bypasses DWM composition (independent flip, per the documentation), adds a frame of latency and loads the GPU itself

## latency

- the device frame queue is limited to one frame instead of the default three
- the rest of the latency is built into the scheme: the desktop is captured after DWM composition, the overlay is drawn and composed again, at least one or two frames. For games the answer is auto-pause
- I did not measure values in milliseconds

## games

the Image card of the settings window has two rows, both work with auto-pause:

- "Auto-pause": when to consider a game running (off, fullscreen, fullscreen and 3D apps)
- "Action in games": "Pause" releases the device and the overlay window, "Light mode" keeps the overlay but the shader does one shift per channel without the laplacian and sharpness and costs almost nothing. Light mode does not remove DWM composition, so only a pause lifts the game latency

more in `fringefix.ini`:

- `pause_apps = game.exe;other.exe` always treat as a game even when the window is not fullscreen; a name without `.exe` also fits, case is ignored
- `ignore_apps = player.exe` never treat as a game, for example a fullscreen video player
- `gpu_priority = -3` GPU priority of the overlay thread from -7 to 7; below zero the overlay yields the GPU to the game, which reduces the FPS cost in light mode

## versions and rollback

the "Versions..." button in the settings window opens a list of the repository releases (up to the latest 30 that have `fringefix.exe` and `fringefix.exe.sha256`). Picking a version newer than the current one installs an update, older installs a rollback. Versions are not compared during installation: the hash check and the exe swap are the same as in a normal update, settings in `%APPDATA%` stay. Old releases are not overwritten: to go back to a version it must stay in Releases.

## profiles

- "Save profile..." and "Load profile..." work with JSON files, the default folder is `%APPDATA%\fringefix\profiles`
- the tray menu item "Profiles" lists the files from that folder, picking one applies it at once
- a profile holds only the look of the picture: the three channel shifts and sharpness, the monitor and the update check stay as they were

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

## language and theme

- the interface is Russian if Windows display language is Russian, English otherwise
- the theme is light or dark by the Windows "app mode" setting, a change applies to an open window without restart
- the slider accent colour comes from the system one

## how it works

```
main.c ─ ui_tray.c ─ ui_settings.c ─ ui_controls.c, ui_paint.c, slider.c, theme.c
  │         │            └─ ui_profile.c, ui_wizard.c ─ layout.c, pattern.c
  │         │            └─ ui_versions.c
  │         └─ ui_update.c ─ update.c, update_install.c ─ http.c, sha256.c, release.c
  └─ app.c ─ config.c, profile.c, profile_io.c, i18n.c
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
- `release`: on a `vX.Y.Z` tag, only if all three previous jobs are green

the application runs on Windows only: capture through DXGI Desktop Duplication and output through Direct3D 11 do not port to Linux. On Linux the CI checks the code and builds the exe but does not run it.

## not done

- automatic fringing measurement, for example from a photo of the screen: the wizard follows a panel model
- layouts other than the QD-OLED triangle: WOLED, for example, arranges subpixels differently
- text-only mask for sharpening: sharpening applies to the whole screen
- rotated monitors
- start with Windows
- code signing and release signing
- manual choice of language and theme: they come from the system only
- the overlay adds one frame of latency over the desktop
