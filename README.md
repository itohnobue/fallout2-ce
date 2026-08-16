# Fallout 2 CE Extended

**A native macOS ARM Fallout 2 engine that runs the big total-conversion mods — Fallout 2 Restoration Project (RPU) and Fallout Et Tu — out of the box.**

Fallout 2 CE Extended is a fork of [fallout2-ce](https://github.com/fallout2-ce/fallout2-ce) (the modern re-implementation of the Fallout 2 engine). On top of the upstream engine we add the sfall scripting layer those mods depend on, deep RPU / Et Tu compatibility work, and production hardening — all kept in sync with upstream as it evolves.

## Why this fork

There was no easy way to play Fallout 1 and Fallout 2 on Mac with Apple Silicon with the modern mod scene installed (RPU, Et Tu). Upstream CE runs on macOS, but the big total conversions require [sfall](https://github.com/sfall-team/sfall) — a Windows-only engine extension. This project reimplements the sfall scripting surface natively inside CE, so the mods run on the fork without any Windows layer.

## What you get vs. upstream CE

| Area | Upstream CE | This fork |
| --- | --- | --- |
| **sfall scripting** | partial | Full sfall 4.5.1 scripting surface: 121+ opcodes/metarules, 43 of 62 hook types (38 sfall hooks + 5 CE-specific hooks) |
| **Fallout 2 Restoration Project (RPU)** | not supported | ✅ Supported — all RPU hooks, opcodes, metarules and config keys implemented and verified against the RPU source |
| **Fallout Et Tu** | not supported | ✅ Supported — 28/33 requirement rows verified; the 5 remaining are optional sfall engine features no Et Tu script depends on |
| **Mod configuration** | partial | RPU/Et Tu ddraw.ini keys honored (WorldMapSlots, ElevatorsFile, ExtraSaveSlots, KarmaFRMs, SpeedMulti, OverrideArtCacheSize, FemaleDialogMsgs, …), bridged into CE's config system |
| **Save compatibility** | — | Backward compatible saves; sfall global-vars and override state serialized |
| **Hardening** | — | 18 production audit passes: ~800+ verified fixes across every subsystem (bounds checks, UAF/null-deref guards, save integrity, VFS sandboxing) |
| **Upstream sync** | — | Continuously merged (currently synced through upstream 1cce144, 2026-08-16) |
| **Tests** | — | 90 test executables, all passing |

## Mod compatibility

| Mod | Status | Notes |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | ✅ Supported | Hooks, opcodes, metarules, config keys all implemented. UPU extras included (Goris de-robing FPS, critter walk speed, extra save slots, hero appearance). |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | ✅ Supported | Full FO1-mode engine behavior (traits, combat, rest, encounters, worldmap, dialog), Et Tu config overlays, and its sfall surface. |

Detailed, requirement-by-requirement status for both mods lives in [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md).

## What's left

Everything remaining is polish-level — nothing blocks playing either mod:

- **Et Tu: `PerksFile` (P2)** — Et Tu ships `config/Perks.ini` FO1 perk tuning that the engine currently ignores (FO2 perk defaults apply).
- **Et Tu: optional sfall engine features (P3)** — NPC combat control, key-driven item highlighting, worldmap tweaks, and a few QoL settings. None of these break Et Tu scripts; they're parity niceties.
- **Et Tu: rotators-only metarules as safe no-ops (P3)** — for third-party scripts probing `metarule_exist("r_...")`.
- **Runtime verification (P3)** — a few in-game checks (reaction-threshold persistence, Fast Shot AP edge cases).
- **Upstream sync (ongoing)** — drift is currently 1 commit.

## Docs

- [INSTALL.md](INSTALL.md) — build and install for all platforms
- [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md) — sfall / RPU / Et Tu compatibility reference, with remaining-work checklists
- [CHANGELOG.md](CHANGELOG.md) — release history

## License

The source code in this repository is available under the [Sustainable Use License](LICENSE.md).
