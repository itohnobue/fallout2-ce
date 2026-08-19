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
> [Step 7](#step-7--et-tu-fallout-1-in-fallout-2s-engine).

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

## Step 6 — RPU (Fallout 2 Restoration Project)

**What it is:** the huge Fallout 2 overhaul (restored quests, new locations, hi-res
support). Requires a **separate Fallout 2 game folder** — RPU and Et Tu cannot share
one FO2 folder because both install into the game root (`data/`, `mods/`,
`f2_res.dat`). If you don't have a game folder yet, repeat Step 2 into a new folder
(e.g. `~/Fallout2-RPU`).

You need the RPU release zip: [https://github.com/BGforgeNet/Fallout2_Restoration_Project/releases/latest](https://github.com/BGforgeNet/Fallout2_Restoration_Project/releases/latest) (`rpu_v2.4.34.zip`).

### 6.1 Download and unpack RPU

```bash
mkdir -p tmp/downloads
curl -L -o tmp/downloads/rpu_v2.4.34.zip \
     "https://github.com/BGforgeNet/Fallout2_Restoration_Project/releases/download/v2.4.34/rpu_v2.4.34.zip"
unzip -q tmp/downloads/rpu_v2.4.34.zip -d tmp/downloads/rpu-release
```

### 6.2 Install RPU into the game folder

```bash
cp -R tmp/downloads/rpu-release/. ~/Fallout2-RPU/
cd ~/Fallout2-RPU
./rpu-install.sh
cd ..
```

The official installer requires:
- a **case-insensitive** filesystem (default APFS) — otherwise it aborts with
  "filesystem is case sensitive"
- `master.dat` present — otherwise "not Fallout 2 directory"
- **no** UP or UPU installation — start from a fresh Fallout 2 data copy

It backs up the vanilla files into `backup/rpu/`, fixes the music paths in
`fallout2.cfg`, moves `mods_order.txt` into `mods/`, and deletes itself.

### 6.3 Remove the Windows-only files

```bash
cd ~/Fallout2-RPU
rm -f d3dx9_31.dll d3dx9_43.dll f2_res_config.exe f2_res.dll fallout2.exe \
      ddraw.dll goggame.dll fallout2.ico gfw_high.ico MANUAL.PDF readme.txt \
      sfall.dat sfall-FAQ.txt sfall-readme.txt unins000.dat unins000.exe \
      unins000.ini Support.ico "Launch Fallout.lnk"
cd ..
```

Keep: `master.dat`, `critter.dat`, `data/`, `sound/`, `appearance/`,
`translations/`, `mods/` (rpu.dat + mods_order.txt + f2_res.dat + inis),
`backup/`, `f2_res.ini`, `ddraw.ini`, `fallout2.cfg`.

### 6.4 Configure

`fallout2.cfg` is RPU's (music paths already fixed by the installer). Add the hi-res
archive path — RPU ships `f2_res.dat` in `mods/`, the engine default is `f2_res.dat`:

```bash
cd ~/Fallout2-RPU
# add this line under [system]:
#   f2_res_dat=mods/f2_res.dat
cd ..
```

Example `[system]` block after the edit:

```ini
[system]
master_dat=master.dat
critter_dat=critter.dat
master_patches=data
critter_patches=data
f2_res_dat=mods/f2_res.dat
```

`ddraw.ini` is RPU's own (from the zip) — keep as shipped. RPU options live in
`mods/rpu.ini` and `mods/upu.ini`.

### 6.5 Deploy the app

Copy the app into the game folder, inject the configs into the bundle, and re-sign
(identical procedure to Step 3, with RPU's configs):

```bash
APP="out/build/macos-arm64-release/RelWithDebInfo/Fallout II Community Edition.app"
cp -R "$APP" ~/Fallout2-RPU/

MACOS_DIR=~/Fallout2-RPU/Fallout\ II\ Community\ Edition.app/Contents/MacOS
cp ~/Fallout2-RPU/fallout2.cfg "$MACOS_DIR/fallout2.cfg" && chmod 644 "$MACOS_DIR/fallout2.cfg"
cp ~/Fallout2-RPU/ddraw.ini "$MACOS_DIR/ddraw.ini" && chmod 644 "$MACOS_DIR/ddraw.ini"

IDENTITY=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/8P47J3K846/ {print $2}' | head -1)
APP_BUNDLE=~/Fallout2-RPU/Fallout\ II\ Community\ Edition.app
for f in "$MACOS_DIR/fallout2.cfg" "$MACOS_DIR/ddraw.ini"; do
    codesign --force --sign "$IDENTITY" "$f"
done
codesign --force --sign "$IDENTITY" "$APP_BUNDLE"
codesign --verify --deep --strict "$APP_BUNDLE"
```

### 6.6 Launch and verify

```bash
cd ~/Fallout2-RPU
open "Fallout II Community Edition.app"
```

| Check | Expected |
|---|---|
| Main menu | Fallout 2 main menu with the RPU splash |
| New game | Starts in Arroyo |
| Deep-run marker | `data/worldmap.dat` appears after first run |

**RPU requires a new game after installation** — do not load old saves.

### 6.7 RPU troubleshooting

| Symptom | Cause / fix |
|---|---|
| `rpu-install.sh`: "filesystem is case sensitive" | Use a case-insensitive volume (default APFS) or lowercase the game folder |
| `rpu-install.sh`: "not Fallout 2 directory" | `master.dat` missing — redo Step 2 |
| `rpu-install.sh`: "UP/UPU installation detected" | Start from a fresh Fallout 2 data copy |
| "Could not find the master datafile" | Bundle config missing or stale — redo 6.5 |

---

## Step 7 — Et Tu (Fallout 1 in Fallout 2's engine)

**What it is:** restores Fallout 1 content into the Fallout 2 engine — you play
Fallout 1 (Vault 13 and all) with this engine's modern features. Needs a
**separate Fallout 2 game folder** (same reason as RPU) **plus the Fallout 1 GOG
installer** (`fallout_2.0.0.7.dmg`) for the Fallout 1 content, and the `ce-dat-tool`
(FO1 archive extractor) built from this repo.

### 7.1 Build the FO1 extractor

```bash
cmake --build --preset macos-arm64-release --target ce-dat-tool
```

Output: `out/build/macos-arm64-release/RelWithDebInfo/ce-dat-tool`

### 7.2 Set up the game folder and the mod

If you don't have a Fallout 2 game folder yet, repeat Step 2 into a new folder
(e.g. `~/Fallout2`). Then get the Et Tu release zip:
[https://github.com/rotators/Fo1in2/releases/latest](https://github.com/rotators/Fo1in2/releases/latest) (`Fallout1in2.zip`).

```bash
mkdir -p tmp/downloads
curl -L -o tmp/downloads/Fallout1in2.zip \
     "https://github.com/rotators/Fo1in2/releases/download/v1.16.3771/Fallout1in2.zip"
unzip -q tmp/downloads/Fallout1in2.zip -d tmp/downloads/ettu-release
cp -R tmp/downloads/ettu-release/Fallout1in2 ~/Fallout2/
```

The mod folder `~/Fallout2/Fallout1in2` is a complete game directory: it reads the
Fallout 2 data (`master.dat`, `critter.dat`) from the **parent** folder via
`..\master.dat` paths in its own config.

### 7.3 Snapshot the mod's own files (before extraction)

The FO1 extraction adds files to the mod folder, and only the whitelist in
`undat_files.txt` may stay (ART + SOUND; the mod ships its own scripts/proto/maps/
text). Snapshot the mod's own files **before** extracting, so you can tell them apart
later:

```bash
cd ~/Fallout2/Fallout1in2
find . -type f | sed 's|^\./||' | tr '[:upper:]' '[:lower:]' | sort -u > /tmp/pristine.txt
```

### 7.4 Extract the Fallout 1 content

Mount the Fallout 1 installer and extract its `MASTER.DAT` into the mod folder with
`ce-dat-tool` (which handles the FO1 LZSS archive format):

```bash
hdiutil attach install/fallout_2.0.0.7.dmg     # mounts at /Volumes/Fallout
FO1_DAT="/Volumes/Fallout/Fallout.app/Contents/Resources/game/Fallout.app/Contents/Resources/drive_c/GOG Games/Fallout/MASTER.DAT"
DAT=out/build/macos-arm64-release/RelWithDebInfo/ce-dat-tool

cd ~/Fallout2/Fallout1in2
"$DAT" "$FO1_DAT" extract .
hdiutil detach /Volumes/Fallout
```

### 7.5 Whitelist cleanup and Windows-file removal

Delete every extracted file that is **not** the mod's own (pristine snapshot) and
**not** on the `undat_files.txt` whitelist, then remove empty directories:

```bash
cd ~/Fallout2/Fallout1in2
find . -type f | sed 's|^\./||' | tr '[:upper:]' '[:lower:]' | sort -u > /tmp/after.txt
tr -d '\r' < undat_files.txt | sed 's|\\|/|g' | tr '[:upper:]' '[:lower:]' | sort -u > /tmp/keep.txt
comm -23 /tmp/after.txt <(cat /tmp/pristine.txt /tmp/keep.txt | sort -u) | while read -r p; do rm -f "./$p"; done
find . -type d -empty -delete
```

Remove the Windows-only files from the mod folder:

```bash
cd ~/Fallout2/Fallout1in2
rm -f d3dx9_31.dll d3dx9_43.dll dat2.exe ddraw.dll Fallout2.exe f2_res_Config.exe \
      f2_res.dll f2_res_README.rtf sfall.dat sfall-FAQ.txt sfall-readme.txt README.txt
cd ../..
```

### 7.6 Configure

`Fallout2.cfg` is the mod's own — keep it as shipped. Its `[system]` section resolves
the Fallout 2 data in the parent folder:

```ini
[system]
master_dat=..\master.dat
critter_dat=..\critter.dat
master_patches=data
critter_patches=data
```

`ddraw.ini` is the mod's own (Fallout 1 behavior keys) — keep as shipped. Et Tu
options live in `Fallout1in2/config/fo1_settings.ini` — set them **before** starting
a new game (mid-game changes can corrupt saves).

### 7.7 Deploy the app

The app goes **inside the mod folder** (`Fallout1in2/` — that is the engine's working
directory; its config resolves `..\master.dat` against the parent). Same procedure as
Step 3, with the mod's configs:

```bash
APP="out/build/macos-arm64-release/RelWithDebInfo/Fallout II Community Edition.app"
cp -R "$APP" ~/Fallout2/Fallout1in2/

MACOS_DIR=~/Fallout2/Fallout1in2/Fallout\ II\ Community\ Edition.app/Contents/MacOS
cp ~/Fallout2/Fallout1in2/Fallout2.cfg "$MACOS_DIR/fallout2.cfg" && chmod 644 "$MACOS_DIR/fallout2.cfg"
cp ~/Fallout2/Fallout1in2/ddraw.ini "$MACOS_DIR/ddraw.ini" && chmod 644 "$MACOS_DIR/ddraw.ini"

IDENTITY=$(security find-identity -v -p codesigning 2>/dev/null | awk -F'"' '/8P47J3K846/ {print $2}' | head -1)
APP_BUNDLE=~/Fallout2/Fallout1in2/Fallout\ II\ Community\ Edition.app
for f in "$MACOS_DIR/fallout2.cfg" "$MACOS_DIR/ddraw.ini"; do
    codesign --force --sign "$IDENTITY" "$f"
done
codesign --force --sign "$IDENTITY" "$APP_BUNDLE"
codesign --verify --deep --strict "$APP_BUNDLE"
```

### 7.8 Launch and verify

```bash
cd ~/Fallout2/Fallout1in2
open "Fallout II Community Edition.app"
```

| Check | Expected |
|---|---|
| Main menu | Fallout 1 main menu (original FO1 title/music) |
| New game | Starts in Vault 13 |
| Deep-run marker | `worldmap.dat` appears inside `Fallout1in2/` after first run |

### 7.9 Et Tu troubleshooting

| Symptom | Cause / fix |
|---|---|
| FO1 content missing (FO2 menu shows) | FO1 extraction/whitelist cleanup wrong — redo 7.3–7.5 from a fresh `Fallout1in2` copy |
| Wrong FO2 content instead of FO1 | `Fallout2.cfg` not injected into the bundle — redo 7.7 |
| "Could not find the master datafile" | Bundle config missing or stale — redo 7.7 |
| Settings don't apply / saves corrupt | `fo1_settings.ini` changed mid-game — configure before starting a new game |

Et Tu settings must be configured **before** starting a new game.

---

## Code signing

The build script wants to sign the app with the project's default signing team ID
(`8P47J3K846`) — that identity lives in this project's keychain, not in yours. If the
build fails with "No codesigning identity found", do one of the following.

### Option A — build unsigned

You don't need a certificate at all for local play: macOS only checks signatures on
quarantined downloads, not on apps you build yourself.

```bash
./build_mac_arm_release.sh --no-sign      # or: SKIP_SIGNING=1 ./build_mac_arm_release.sh
```

The `.app` and `.dmg` are produced without signing.

When you later deploy (Step 3.3 / 6.5 / 7.7), the identity lookup finds nothing and
the re-sign step is skipped — or use ad-hoc signing, which needs no certificate:

```bash
codesign --force --sign - "$APP_BUNDLE"          # ad-hoc sign (no identity needed)
codesign --verify --deep --strict "$APP_BUNDLE"
```

### Option B — use your own signing identity

If you have an Apple Developer certificate (any kind — free Apple ID account
works), point the build at it:

```bash
security find-identity -v -p codesigning         # list what your keychain has
```

Then either:

```bash
./build_mac_arm_release.sh --sign-id "Apple Development: Your Name (TEAMID)"
```

or set your team ID (find it on
https://developer.apple.com/account → Membership Details, or in the identity name,
e.g. `(8P47J3K846)`):

```bash
SIGNING_TEAM_ID=YOURTEAMID ./build_mac_arm_release.sh
```

With your own identity the deploy steps (3.3 / 6.5 / 7.7) will pick it up only if
they match the `8P47J3K846` lookup — otherwise just run the deploy re-sign block
with your identity string in place of `$IDENTITY`, or keep it unsigned as in
Option A.

---

## Troubleshooting

| Symptom | Cause / fix |
|---|---|
| Build fails: "No codesigning identity found" | See [Code signing](#code-signing) above — build unsigned with `--no-sign` or provide your own identity |
| "Could not find the master datafile" | Bundle config missing or stale — redo Step 3.2–3.3 |
| Game boots but shows no/old content | `ddraw.ini` not injected — redo Step 3.2–3.3; check `debug.log` for `Loading mod …` lines |
| Signature invalid after editing files inside the bundle | Never edit inside the bundle — edit the game-folder configs and redo Step 3.2–3.3 |
| Mod installer reports "filesystem is case sensitive" | Use a case-insensitive volume (default APFS) or lowercase the game folder |
| Game runs fullscreen and you want a window | In `fallout2.cfg`: `[screen] windowed=1`, `resolution_x` / `resolution_y` |
| Game still running / hung | `pkill -f "Fallout II Community Edition"` |
