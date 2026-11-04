# fringefix

[Русский](README.ru.md)

A Windows overlay that shifts the colour channels of the finished frame by fractions of a pixel to cancel colour fringing on QD-OLED panels with the triangular subpixel layout (4th generation and earlier). Lives in the tray, tuned with sliders, updates from GitHub Releases.

> limitations: the project builds with mingw-w64 without warnings and the portable-module tests pass (`make test`), but it has never been run on Windows. The tray, settings window, HDR path and updater follow the documentation and are unverified. Requires Windows 10 2004 or newer. Updates work only with a public repository: for a private one the GitHub API answers 404 without a token.

## running

- download `fringefix.exe` from the latest release and put it in any folder
- start it, a tray icon of three coloured bars appears (grey means off)
- double click opens settings, right click opens the menu
- `Ctrl+Alt+F` toggles the overlay, `Ctrl+Alt+Q` quits
- settings live in `%APPDATA%\fringefix\fringefix.ini`, the Save button writes them

## tuning

1. press the pattern button: lines on the left and text in the middle open on the chosen monitor
2. move the vertical green slider until horizontal edges turn grey
3. move the horizontal red and blue sliders in opposite directions until vertical edges turn grey
4. the additional sliders are usually unneeded on this panel: red and blue subpixels sit at the same height and green sits midway between them horizontally
5. press save

A positive value moves the channel content right and down, slider step is 0.01 pixel, range is -2 to 2.

## how it works

```
main.c ─ ui_tray.c ─ ui_settings.c ─ pattern.c
  │         └─ ui_update.c ─ update.c ─ http.c, sha256.c, release.c
  └─ app.c ─ config.c
       └─ capture.c (own thread)
            └─ gfx.c, gfx_frame.c ─ shader.c, output.c
```

- the frame comes from `IDXGIOutput6::DuplicateOutput1`, format follows the desktop mode: 8-bit in SDR, FP16 scRGB in HDR
- the window uses `WDA_EXCLUDEFROMCAPTURE`, so capture never sees the overlay and no feedback loop forms
- capture and overlay run in their own thread: menus, dialogs and window dragging do not freeze the picture
- the shader runs only when the frame has a non-zero `LastPresentTime`, so cursor-only movement leaves the GPU idle
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

## not done

- auto-suspend in games: switch it off by hand, a full-screen shader loads the GPU noticeably
- text-only mask for sharpening: sharpening applies to the whole screen
- rotated monitors
- start with Windows
- code signing and release signing
- the interface is Russian only
- the overlay adds one frame of latency over the desktop
