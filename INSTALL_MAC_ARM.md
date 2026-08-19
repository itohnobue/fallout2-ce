# Install Guide — Apple Silicon Macs (ARM64)

This guide explains how to install the **Fallout 2 CE Extended** engine (the built
`.app` bundle) on an Apple Silicon Mac, step by step. It covers:

1. Building the engine (produces the `.app` file)
2. Getting Fallout 2 from a GOG installer (the GOG Mac installers are Wine
   wrappers — you extract the game data from inside them)
3. Installing the `.app` into your game folder
4. Launching and verifying the game works

Every command below is the exact procedure used to build and verify the engine's
test stands — it is known to work end to end.

---

## Requirements

| Requirement | Notes |
|---|---|
| **Apple Silicon Mac** (M1/M2/M3/M4) | The build targets arm64 only |
| **macOS** with a **case-insensitive** filesystem | Default APFS is case-insensitive — fine |
| **Xcode Command Line Tools** | `xcode-select --install` if missing |
| **CMake ≥ 3.20** | `brew install cmake` if missing |
| **Codesigning identity** | Default team ID `8P47J3K846` looked up in the keychain; the build fails without it unless you use `--no-sign` |
| **Fallout 2 game files** | From the GOG installer (`fallout_2_2.0.0.4.dmg`) or any other copy of the game data |

---

## Step 1 — Build the engine

From the repository root:

```bash
./build_mac_arm_release.sh
```

This configures, builds, signs, and verifies. When it finishes you get:

```
App:  out/build/macos-arm64-release/RelWithDebInfo/Fallout II Community Edition.app
DMG:  out/build/macos-arm64-release/Fallout II Community Edition.dmg
```

Useful options (see `./build_mac_arm_release.sh --help`):

| Option | What it does |
|---|---|
| `--no-sign` | Skip code signing (use this if you have no signing identity; the game still runs locally) |
| `--clean` | Wipe the build directory before rebuilding |
| `--sign-id "<identity>"` | Use a specific signing identity |

The `.dmg` in the same folder contains the identical signed `.app` — pick either
artifact; the install steps below are the same.

---

## Step 2 — Get the Fallout 2 game data (GOG installer)

**How GOG Mac installers work:** the GOG `fallout_2_2.0.0.4.dmg` is not a normal
game — it is a Wine wrapper. The actual game files (the `.dat` archives) sit inside
a nested `drive_c` folder, the same layout the Windows version uses. Installing the
game for CE means **extracting that data folder**; the Wine wrapper itself is not used.

> Note: the Fallout 1 GOG installer (`fallout_2.0.0.7.dmg`) is **only** needed if you
> install the Et Tu mod (Fallout 1 in Fallout 2's engine) — see
> [Step 6](#step-6--optional-mods).

### 2.1 Mount the installer

```bash
hdiutil attach install/fallout_2_2.0.0.4.dmg
```

If your copy is elsewhere (e.g. `~/Downloads/fallout_2_2.0.0.4.dmg`), use that path
instead. The DMG mounts at `/Volumes/Fallout 2`.

### 2.2 Find and copy the game data

The game data lives inside the wrapper at:

```
/Volumes/Fallout 2/Fallout 2.app/Contents/Resources/game/Fallout 2.app/Contents/Resources/drive_c/Program Files/GOG.com/Fallout 2
```

Copy it into a new game folder (this example uses `~/Fallout2`; any folder works —
**case-insensitive** filesystem recommended):

```bash
FO2_SRC="/Volumes/Fallout 2/Fallout 2.app/Contents/Resources/game/Fallout 2.app/Contents/Resources/drive_c/Program Files/GOG.com/Fallout 2"
mkdir -p ~/Fallout2
cp -R "$FO2_SRC/." ~/Fallout2/
hdiutil detach "/Volumes/Fallout 2"
```

### 2.3 Remove the Windows-only files

The copied folder contains Windows executables, DLLs, icons and uninstaller files
that CE does not need. Delete them — keep only the game data:

```bash
cd ~/Fallout2
rm -f ddraw.dll fallout2.exe fallout2.ico goggame.dll MANUAL.PDF readme.txt \
      unins000.dat unins000.exe unins000.ini Support.ico gfw_high.ico
cd ..
```

Keep: `master.dat`, `critter.dat`, `patch000.dat`, `data/`, `sound/`.

---

## Step 3 — Install the app into the game folder

The engine looks up `fallout2.cfg` and `ddraw.ini` **next to the executable** (inside
the app bundle, `Contents/MacOS/`) — and all data paths in those configs resolve
against the game folder. This means:

- **The app must live in the game folder** (so the data paths resolve correctly).
- **Your config files must be inside the bundle** so every launch method (Finder,
  `open`, terminal) reads them.

### 3.1 Copy the app into the game folder

```bash
APP="out/build/macos-arm64-release/RelWithDebInfo/Fallout II Community Edition.app"
cp -R "$APP" ~/Fallout2/
```

### 3.2 Inject the config files

Copy `fallout2.cfg` and `ddraw.ini` from the game folder into the bundle (mode 644):

```bash
MACOS_DIR=~/Fallout2/Fallout\ II\ Community\ Edition.app/Contents/MacOS
cp ~/Fallout2/fallout2.cfg "$MACOS_DIR/fallout2.cfg"
chmod 644 "$MACOS_DIR/fallout2.cfg"
if [ -f ~/Fallout2/ddraw.ini ]; then
    cp ~/Fallout2/ddraw.ini "$MACOS_DIR/ddraw.ini"
    chmod 644 "$MACOS_DIR/ddraw.ini"
fi
```

The GOG Fallout 2 `fallout2.cfg` (in the copied game folder) already points at the
right data files and works as-is:

```ini
[system]
master_dat=master.dat
critter_dat=critter.dat
master_patches=data
critter_patches=data
```

`ddraw.ini` is optional for vanilla Fallout 2 — the engine falls back to built-in
defaults when it is absent.

### 3.3 Re-sign the bundle

macOS treats every file in `Contents/MacOS/` as a code object: an unsigned config
file there breaks the bundle signature. Sign each injected config **individually
first**, then the bundle itself, then verify. Use the signing identity from the
keychain (default team ID `8P47J3K846`):

```bash
IDENTITY=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/8P47J3K846/ {print $2}' | head -1)
APP_BUNDLE=~/Fallout2/Fallout\ II\ Community\ Edition.app

if [ -n "$IDENTITY" ]; then
    for f in "$MACOS_DIR/fallout2.cfg" "$MACOS_DIR/ddraw.ini"; do
        [ -f "$f" ] && codesign --force --sign "$IDENTITY" "$f"
    done
    codesign --force --sign "$IDENTITY" "$APP_BUNDLE"
    codesign --verify --deep --strict "$APP_BUNDLE"
fi
```

If no identity is available, re-sign ad-hoc instead (`codesign --force --sign -` on
each file and the bundle) — local execution works fine either way.

> **Never edit files inside the bundle after signing** — it invalidates the seal.
> Edit the configs in the game folder and repeat steps 3.2–3.3.

---

## Step 4 — Launch

```bash
cd ~/Fallout2
open "Fallout II Community Edition.app"
```

The first launch takes a few seconds. The engine writes defaults into
`fallout2.cfg` on first run (harmless — it also migrates `f2_res.ini` settings if
present).

---

## Step 5 — Verify it works

| Check | Expected |
|---|---|
| Game window opens | Fallout 2 main menu appears |
| Deep-run marker | `data/worldmap.dat` appears in the game folder after first run — proves the data chain loaded |
| Debug log (optional) | Add `[debug] mode=log` to `fallout2.cfg`, relaunch, then read `debug.log` in the game folder — first line shows which config the engine read |
| Console capture (optional) | `[debug] console_output_path=<file>` in the cfg writes the in-game console to that file |

---

## Step 6 — Optional: mods

The engine fully supports the two major overhaul mods — **RPU** (Fallout 2
Restoration Project) and **Et Tu** (Fallout 1 in Fallout 2's engine, needs the
Fallout 1 GOG installer too). Both follow the same procedure as above: extract the
game data, run the mod's own installer in the game folder, then apply Step 3–5 with
the mod's `fallout2.cfg` / `ddraw.ini` instead of the vanilla ones. RPU additionally
requires `f2_res_dat=mods/f2_res.dat` in `[system]`.

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Build fails: "No codesigning identity found" | No identity for team ID `8P47J3K846` in the keychain — rebuild with `./build_mac_arm_release.sh --no-sign` |
| "Could not find the master datafile" | Bundle config missing or stale — redo Step 3.2–3.3 |
| Game boots but shows no/old content | `ddraw.ini` not injected — redo Step 3.2–3.3; check `debug.log` for `Loading mod …` lines |
| Signature invalid after editing files inside the bundle | Never edit inside the bundle — edit the game-folder configs and redo Step 3.2–3.3 |
| Mod installer reports "filesystem is case sensitive" | Use a case-insensitive volume (default APFS) or lowercase the game folder |
| Game runs fullscreen and you want a window | In `fallout2.cfg`: `[screen] windowed=1`, `resolution_x` / `resolution_y` |
| Game still running / hung | `pkill -f "Fallout II Community Edition"` |
