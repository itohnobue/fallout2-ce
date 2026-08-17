# Fallout 2 CE Extended

**A beta-stage fork of the Fallout 2 Community Engine, targeting native macOS ARM support for the modern total-conversion mods (RPU, Et Tu).**

Fallout 2 CE Extended is a fork of [fallout2-ce](https://github.com/fallout2-ce/fallout2-ce), the modern re-implementation of the Fallout 2 engine. On top of the upstream engine we are building the sfall scripting layer those mods depend on, RPU / Et Tu compatibility work, and production hardening — kept in sync with upstream as it evolves.

**Status: beta.** The compatibility surface is implemented and unit-tested, but large-scale in-game testing against real mod installs is still underway. Do not treat the mod support claims below as a finished product.

## Why this fork

Upstream CE runs on macOS, but the big total conversions (RPU, Et Tu) require [sfall](https://github.com/sfall-team/sfall) — a Windows-only engine extension. This project reimplements the sfall scripting surface natively inside CE, so the mods can run on the fork without a Windows layer. The fork point is motivated by the lack of an easy way to play Fallout 1 and 2 on Apple Silicon with the modern mod scene installed.

## What we've implemented (vs. upstream CE)

| Area | Upstream CE | This fork |
| --- | --- | --- |
| **sfall scripting** | partial | sfall 4.5.1 scripting surface reimplemented: 121+ opcodes/metarules registered, 43 of 62 hook types (38 sfall hooks + 5 CE-specific hooks) |
| **RPU (Fallout 2 Restoration Project)** | not supported | RPU's hooks, opcodes, metarules and ddraw.ini config keys implemented — all 25 requirement rows and all 6 remaining-work items verified against the RPU source (2026-08-17 audit). Script-level behavior still needs large-scale in-game verification. |
| **Et Tu (Fallout 1 in FO2)** | not supported | Et Tu's config overlays and sfall surface implemented; FO1-mode engine behavior (traits, combat, rest, encounters, worldmap, dialog) built in. 30 of 33 requirement rows verified against source (2026-08-17 audit); the 3 remaining rows are out-of-project-scope optional sfall engine features no mod script depends on. |
| **Config keys** | partial | RPU/Et Tu ddraw.ini keys bridged into CE's config system (WorldMapSlots, ElevatorsFile, ExtraSaveSlots, KarmaFRMs, SpeedMulti, OverrideArtCacheSize, FemaleDialogMsgs, …) |
| **Save compatibility** | — | Backward-compatible saves; sfall global-vars and override state serialized |
| **Hardening** | — | 18 production audit passes — hundreds of verified fixes (bounds checks, UAF/null-deref guards, save integrity, VFS sandboxing) |
| **Upstream sync** | — | Continuously merged (currently synced through upstream 1cce144, 2026-08-16) |
| **Tests** | — | 92 test executables, all passing (unit/mirror tests; not a substitute for in-game testing) |

## Mod compatibility status

**Both mods are in beta testing — do not assume a finished experience.**

| Mod | Status | What's implemented |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | Beta | RPU's 4 hooks, ~25 opcodes/metarules, and config keys implemented; UPU extras (Goris de-robing FPS, critter walk speed, extra save slots, hero appearance). All 25 requirement rows + 6 remaining-work items verified against the RPU source (2026-08-17); long-session in-game testing is still in progress. |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | Beta | FO1-mode engine behavior and Et Tu's sfall surface implemented; 30 of 33 requirement rows verified against source (2026-08-17). The remaining 3 rows are out-of-project-scope optional sfall engine features no mod script depends on (owner decision 2026-08-17). |

Requirement-by-requirement status for both mods, including the known gaps, lives in [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md).

## What's left

- **In-game verification (the big one):** long-session playtesting of both mods on real installs. The unit-test suite covers the code paths, but it does not prove the mods play correctly end-to-end. This is the main outstanding work.
- **Et Tu: optional sfall engine features (P3, out of scope)** — NPC combat control, key-driven item highlighting, `UseScrollingQuestsList`, `ItemCounterAutoCaps`, `DeathScreenFontPatch`, `EnableMusicInDialogue`. None of these break Et Tu scripts; they're parity niceties (owner decision 2026-08-17).
- **Runtime checks (P3)** — a few in-game behavior checks remain (reaction-threshold persistence across game reset). The Fast Shot AP edge cases (double reduction) were fixed and unit-tested 2026-08-17.
- **Upstream sync (ongoing)** — drift is currently 1 commit.

## Docs

- [INSTALL.md](INSTALL.md) — build and install for all platforms
- [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md) — sfall / RPU / Et Tu compatibility reference, with remaining-work checklists
- [CHANGELOG.md](CHANGELOG.md) — release history

## License

The source code in this repository is available under the [Sustainable Use License](LICENSE.md).
