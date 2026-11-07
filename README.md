# Red Alert 2 on macOS

Native macOS / Apple Silicon builds of **Command & Conquer: Red Alert 2**
and **Yuri's Revenge**, based on
[sp00nznet/redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp).
The original x86 game code is statically recompiled to C and built as a native
host. A compatible, legally owned Windows game installation is required.

This fork adds display refresh settings, right-button map dragging and
Simplified Chinese support. The matching modified pcrecomp toolkit is included
in `third_party/pcrecomp`, so a separate toolkit clone is unnecessary.
Upstream installers/releases do not contain this fork's changes.

## Features

- Fullscreen: **F11**, **Option+Return**, or **Control+Command+F**.
- **F10 → Display refresh**: automatic, 60, 120, 144 or 240 Hz presentation
  ceilings. Fractional deadlines avoid the old 15 ms display limiter.
- The original simulation timing and game-speed slider are preserved.
  The highest original single-player speed can run uncapped. Raising the display
  ceiling does not interpolate unit animation or guarantee that many new frames.
- **F10 → Right mouse button → Original / Drag map**. Drag mode is the default.
  Terrain follows the pointer: drag left to move the picture left, and up to
  move it up. A stationary pointer or released button stops panning. Short right
  clicks retain their original action; menus and the sidebar retain normal input.
- Optional Simplified Chinese game text/fonts and a Chinese F10 menu.
  Original English audio and cinematic assets are retained.
- Steam executable icons stored as PNG resources are supported by the macOS packager.

## Build from your own game

Use the Windows files from the Steam or EA release, including `game.exe`
(RA2 1.006), `gamemd.exe` (YR 1.001), and their accompanying data. A game folder
copied from an existing Windows installation also works. This repository
contains no retail executables, MIX archives, keys or generated game C.

On macOS, install Xcode Command Line Tools and Homebrew, then:

```sh
git clone https://github.com/M1p0/redalert2-macos.git
cd redalert2-macos
./setup.sh --game-dir "/path/to/your/RedAlert2"
```

Setup checks dependencies, analyzes and recompiles both games, and builds native
launchers. It asks before installing missing tools. The initial analysis/build
is substantial; see [the build guide](docs/building.md) for the individual steps.
`PCRECOMP` can explicitly select another toolkit; the included version is the
one tested with this fork.

Package double-clickable apps after building:

```sh
.venv/bin/python tools/package_macos.py --build-dir build-macos-game --out dist/ra2
.venv/bin/python tools/package_macos.py --build-dir build-macos --out dist/yr
```

The apps bundle their native libraries and ask for your game folder on first
launch. Their minimum macOS version depends on the bundled libraries; the local
Apple Silicon packages were tested on macOS 27.0.1 and require macOS 27.0.
They are ad-hoc signed and not notarized. Other macOS versions and Intel Macs
have not been validated for this fork.

## Optional Simplified Chinese

The translation project is
[Translate-with-LOVE/Ra2-zh_hans-main](https://github.com/Translate-with-LOVE/Ra2-zh_hans-main),
pinned to `f09640b65b8540c2cf3fe779ee2cc0e0a4fc15f2`.
Translation assets are obtained separately and retain their original rights.
The helper verifies SHA-256 hashes, compiles CSF strings and backs up replaced
files before installing. Choose your own cache and a **new** backup directory:

```sh
python3 tools/install_zh_hans.py --game "/path/to/your/RedAlert2" \
  --cache-dir "/path/to/localization-cache" \
  --backup-dir "/path/to/new-localization-backup"
```

This downloads text and the game font. For the complete five-file overlay pack
(including the separately built Chinese cameo artwork), use
`--pack-dir "/path/to/pack"` instead of `--cache-dir`; hashes must match the
verified pack. The expected hashes are listed in the helper. No audio is replaced.
The packaged launcher detects the installer's provenance marker and uses Chinese
F10 labels with an available macOS system CJK font. Set `RA2_UI_LANGUAGE=en` to
keep that native menu in English.

Backups preserve each previous filename and contain `restore.json`. To undo,
restore entries marked `true` from the backup to the listed game path, and remove
only the newly installed entries marked `false`. Keep the backup until verified.

## Settings and validation

Packaged apps save display/input preferences outside the app bundle:
`~/Library/Application Support/Red Alert 2 Recomp/ra2-present.ini` or
`yr-present.ini`. Game INIs and saves remain in the game folder.
See [presenter details](docs/presenter.md) for command-line overrides and timing
measurements. Normal play does not require test flags such as `--headless`,
`--mute`, `--record` or `--watchdog`.

Validated locally with both Steam games on Apple Silicon:

- Entering a skirmish with Simplified Chinese text and fonts.
- Four drag directions, exact pixel camera deltas, stationary holds and releases.
- Switching and saving Original / Drag map in the Chinese F10 menu.
- Display-deadline checks at 30, 60, 120, 144 and 240 Hz, including stall recovery.
- Packaging, dynamic-library signing verification and a normal app launch.

`tools/playtest.py skirmish-mapdrag` exercises the actual game scroll routine.
The small source-only checks run in [GitHub Actions](.github/workflows/checks.yml);
they do not claim full game or campaign coverage.

## Credits and license

The game host and recompilation pipeline are by **sp00nz** and contributors:
[redalert2-recomp](https://github.com/sp00nznet/redalert2-recomp) and
[pcrecomp](https://github.com/sp00nznet/pcrecomp).
This fork retains their MIT license and notices. See [LICENSE](LICENSE),
[NOTICE](NOTICE), and [toolkit provenance](third_party/pcrecomp-source.json).
Command & Conquer / Red Alert 2 / Yuri's Revenge belong to EA / Westwood.
