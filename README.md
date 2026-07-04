# Fallout 2 CE Extended

Fallout 2 CE Extended is a fully working re-implementation of the Fallout 2 engine, optimized for a hassle-free experience on multiple platforms.  It provides high resolution support, quality-of-life improvements, and dozens of bug fixes.

This is a fork of the original Fallout2: CE project, which is no longer getting regular updates.

## Why you may need this

Currently there is no easy way to play both Fallout 1 and Fallout 2 on Mac with Apple Silicon and all the updates installed (like RPU and Et Tu) to get the best gaming experience. This project is aimed to fix that and deliver a native macOS ARM Fallout 1-2 engine which is also compatible with all improvements.

**Mod compatibility is actively improving.** The following total conversion mods are tested:

| Mod | Status | Notes |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | Supported (Beta) | Latest update adds 3 Et Tu-critical hooks, SpeedMulti, and additional hook fire points for better compatibility. |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | Partial | 3 critical hooks now implemented (USEANIMOBJ, DESCRIPTIONOBJ, SETLIGHTING). Some sfall 4.4.x scripting features may still be missing. |

Fallout2: CE has broad (though not total) compatibility with [Sfall](https://github.com/sfall-team/sfall) scripting extensions.  Many traditional Fallout mods work out of the box.

There is also [Fallout 1 Community Edition](https://github.com/alexbatalov/fallout1-ce) (not affiliated with this fork).

## Configuration

The main configuration file is `fallout2.cfg`. There are several important settings you might need to adjust for your installation. Depending on your Fallout distribution main game assets `master.dat`, `critter.dat`, `patch000.dat`, and `data` folder might be either all lowercased, or all uppercased. You can either update `master_dat`, `critter_dat`, `master_patches` and `critter_patches` settings to match your file names, or rename files to match entries in your `fallout2.cfg`.

The `sound` folder (with `music` folder inside) might be located either in `data` folder, or be in the Fallout folder. Update `music_path1` setting to match your hierarchy, usually it's `data/sound/music/` or `sound/music/`. Make sure it matches your path exactly (so it might be `SOUND/MUSIC/` if you've installed Fallout from CD). Music files themselves (with `ACM` extension) should be all uppercased, regardless of `sound` and `music` folders.

Additional settings for screen resolution, UI customization, and map options are now integrated into the main `fallout2.cfg` file (previously part of `f2_res.ini` from Mash's HRP). When Fallout 2 CE starts, if it detects an existing `f2_res.ini` file, it automatically migrates these settings into `fallout2.cfg`. After migration, `fallout2.cfg` becomes the single source of truth for this configuration.

Here are some important settings in `fallout2.cfg` under the `[screen]` and `[ui]` sections.  See [the example config](https://github.com/fallout2-ce/fallout2-ce/tree/refs/heads/main/files/fallout2.cfg) for a full list of settings.

```ini
[screen]
resolution_x=1600 ; actual game window size (screen resolution), in pixels
resolution_y=1080
windowed=1 ; 0 = fullscreen
scale=2 ; 1 = original scale, 2 = 2x scale, etc. (e.g. at scale 2 and screen resolution 1920x1080, in-game resolution will be 960x540, thus every pixel is twice as wide and tall)

[ui]
;Set to 1 to expand the barter/trade window vertically, adding a 4th item slot per side (requires ce.dat)
expand_barter_window=1
;Maximum number of columns shown in the main inventory and loot/steal windows (valid range: 1..2)
inventory_columns=2
splash_screen_size=1 ; Splash screen scaling (0=original size, 1=fit preserving aspect, 2=stretch to fill)
quick_toolbar_visible=0 ; Skills quick access toolbar visibility (iOS only) (0=hidden, 1=visible)
```

**Recommendations:**
- **Desktops**: Use any size you see fit.
- **Tablets**: Set these values to logical resolution of your device, for example iPad Pro 11 is 1668x2388 (pixels), but it's logical resolution is 834x1194 (points).
- **Mobile phones**: Set height to 480, calculate width according to your device screen (aspect) ratio, for example Samsung S21 is 20:9 device, so the width should be 480 * 20 / 9 = 1067.

In time this stuff will receive in-game interface, right now you have to do it manually. To see all currently working fallout2.cfg settings, just run the game once and quit. It will be automatically updated with defaults for every supported setting.
*Note*: Use of the IFACE_BAR settings requires the `f2_res.dat` file, which contains graphical assets. Various versions are available, but one compatible with the above settings can be found here: [f2_res.dat](https://github.com/fallout2-ce/fallout2-ce/raw/refs/heads/main/files/f2_res.dat)

## Quality of life benefits over vanilla Fallout

* High resolution support
* Expanded 2-column inventory
* Expanded 4-row barter screen
* Expanded AP bar
* Party members can loot and barter in place of PC
* Increased pathfinding nodes 5x for more accurate pathfinding, and use accurate path length for walk vs run
* Ctrl-click to quickly move items when bartering, looting, or stealing
* _a_ to select "all" when selecting item quantity, and for `Take All` when looting
* When bartering, caps default to the right amount to balance the trade
* Music continues playing between maps
* Auto open doors
* Integrated "HELP" menu
* 44.1 kHz stereo sound/music supported, in .ogg and .wav format as well as legacy .acm
* Last used save slot is remembered
* Item/Corpse/Container/Critter highlighting (configure using [mods/sfall-mods.ini](https://github.com/sfall-team/sfall/blob/master/artifacts/config_files/sfall-mods.ini))
* Global game speed multiplier (SpeedMulti) — control animation speed via `[Speed]` section of `ddraw.ini`; changeable at runtime by scripts

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
