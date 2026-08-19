# Known Issues

Current known issues for the Fallout 2 CE Extended engine with **RPU** and **Et Tu**.
Open issues carry their investigation state (root cause hypotheses, ruled-out work,
next steps) so nobody re-does work already done. Sources: in-game user reports and
debug-log analysis, 2026-08-18/19.

---

## Open issues

### Issue A — Striped buildings in Shady Sands (Et Tu) — root cause not found

**Symptom (user report):** In Shady Sands, buildings render as "stripes with proper
sprite and missing one". A text message ("something about group" — exact wording
uncertain) appears right on the sprites.

**Ruled out (with evidence):**
- Building FRMs are VALID: FO1 BOX04.FRM decodes perfectly with the engine's
  big-endian reads — ver=4, fps=10, af=0, fc=1, doff=[0×6], dsz=2456, frame 47×52
  size=2444, 62+12+2444=2518=file size. Pixel row stats show normal content, not
  alternation. Files byte-identical to the original FO1 MASTER.DAT extraction.
- FRM header layout is identical FO1/FO2 (both ver=4 big-endian; engine
  fileReadInt16/32 are big-endian — correct).
- `tile_hires_stencil`: identical to upstream (22 diff lines, cosmetic).
- tile.cc fork changes: M-142/M-143 bounds clamps only (defensive, correct).
- object.cc fork changes: M-76 gObjectFids guards only.
- art.cc fork changes: M-160 validation + art N-1 bounds guards only.
- Art cache size: standard (OverrideArtCacheSize=0, 261MB).
- map_edge.cc: M-141 edgeVersion2 reset (black-overlay-on-reload class) — user's
  issue is on FIRST load, not reload; unlikely.
- **Map data EXONERATED (2026-08-19, do NOT re-open):** with a fully corrected
  parser, SHADYW.MAP parses 100% clean to EOF — objects @44428 total=2000 (75 items,
  33 critters, 428 scenery, 937 walls, 597 misc) — the standard FO2 object table,
  not an FO1-divergent layout. ARVILLAG.MAP (FO2 known-good) parses identically
  clean. (Parser fixes along the way: elevation flags are 0x02/0x04/0x08, not
  0x1/0x2/0x4; script extent = 16 records + length + next per extent; critter object
  records have NO flags int; FO2 proto files are indexed by their INTERNAL pid; zlib
  wbits=15.)

**The "group" text IS explained (mod data, not engine):** ~55 et tu scripts
(ARADESH.int, SHADYWST.int, TANDI.int, OBJ_DUDE.int, gl_0.int, ...) ship a raw ANSI
fragment string `1;38;2;60;248;0` (15 chars) — a debug leftover from a printf where
the ESC byte was lost. The log shows it rendering as
`[1;38;2;60;248;0m�LVAR_reaction_level: 2` (ANSI codes + broken UTF-8 in the message
stream). This is et tu mod data — the fix would be mod-side (strip the debug
strings), not engine.

**Open hypotheses (next work — do NOT re-do the ruled-out work):**
1. Frame data RLE vs raw: the engine advances frames by `frm->size` raw bytes; if a
   Shady building FRM stores RLE-compressed data (size < w*h) and the engine expects
   raw, the FIRST frame may render fine but multi-frame/offset access misdraws.
   Check artGetFrame/artGetFrameData consumers for compression handling.
2. Object draw path: `_obj_render_object` — check whether the fork's
   `artGetFrameData` returns a frame buffer the renderer strides incorrectly for
   FO1's transparency (index 0 transparent — same as FO2, unlikely).
3. Reproduce decisively: load the Shady building FRMs through the engine's real
   artLoad/artGetFrameData and render to PGM/PNG; compare with a known-good tool,
   then bisect the render path (blitBufferToBufferTrans etc.).
4. EDG black overlay: the log shows a suspicious degenerate EDG zone
   `tileRect=(199,0,39800,39999)` with `pixelRect=(8000,0,32,3576)` — a 32px-wide
   strip. Verify what actually draws it (edge rendering vs scrolling only) and
   whether SHADYW.edg zones are misread.
5. Compare against vanilla: run batalov CE on the same et tu stand in Shady Sands
   and confirm buildings render correctly there.

**Next step:** go to the RENDER PATH — hypotheses 1-2 first. The data feeding the
renderer is provably clean, so the defect must be in the draw/blit path
(`artGetFrame`/`artGetFrameData` consumers, `_obj_render_object`, or the EDG
overlay — hypothesis 4). Hypothesis 5 (vanilla comparison in Shady) is the fastest
decisive discriminator.

### Issue C — VOODOO write_* errors (Et Tu, low priority)

`Error during execution: VOODOO write_int/byte(0x00499xxx, ...) — NOT SUPPORTED in
CE engine` — gl_classic_wm.int's VOODOO_WriteNop procs attempt direct memory
patching (sfall-era engine addresses). The fork correctly rejects them. The
classicWM art swap now works via fs_copy, so these are benign-but-noisy. Mod-side
issue; engine behavior is correct.

### Issue E — Combat miss: evade animation plays but HP is drained anyway (Et Tu) — root cause not found

**Symptom (user report, 2026-08-19):** In battle, when an enemy attack misses — the
player's evade/dodge animation plays — the player's HP is drained anyway, as if the
hit landed. Animation says evaded, damage says hit.

**Reproduction:** Observed in the et tu stand. NOT yet characterized: which
enemy/attack type, how often ("sometimes"), whether HP drain equals the full
would-be hit or a partial amount, and whether it also affects non-player critters.
Needs in-game testing to pin down.

**Investigation state (single-session, do NOT re-do):**
- **Theory 1 (script-started combat with damage modifiers) — EXCLUDED.** The only
  script path that starts combat with minDamage/damageBonus is the `attack` opcode
  (0x80D0/0x80DD → `_combat(&gScriptsCSD)` → `_combat_attack` does
  `defenderDamage += damageBonus` + minDamage clamp UNCONDITIONALLY — a MISS
  becomes minDamage damage, dodge anim plays, "missed" message shows, HP drains —
  the exact symptom). BUT: a full disassembler scan of ALL 1443 master.dat scripts
  + 1041 mods/fo1_base scripts found ZERO calls to attack (0x80D0/0x80DD),
  attack_setup (0x8143), critter_dmg (0x80EF), or register_hook (0x8207/0x8262/
  0x827d). et tu never uses script-driven combat or hooks.
- **Theory 2 (`_main_ctd` file-static race) — PARTIALLY EXCLUDED.** `_combat_attack`
  copies the stack-local to the file-static `_main_ctd` AFTER `_action_attack`;
  `_combat_anim_finished` (async) does `_combat_display(&_main_ctd)` +
  `_apply_damage(&_main_ctd, true)` when `_combat_turn_running` hits 0. Two attack
  sequences can overlap — but ONLY the PLAYER's queued attacks can overlap, and
  that direction produces "hit with no damage" (stale apply) — the REVERSE of the
  report. Enemy-overlap could not be constructed statically.
- **Verified identical to upstream:** `_action_melee` (dodge gated on
  `attackerFlags & 0x0300` == 0), `_action_ranged` (dodge on crit-fail without
  hit), `attackCompute` miss path (melee miss → no damage calls),
  `_combat_display`/`_apply_damage` (damage only if > 0), the `_gcsd` block
  (upstream has the same miss-clamp), `randomRoll`. Fork's combat diffs are sfall
  feature additions (knockback globals, hooks, hit-chance caps) + M-55
  interposed-hit fix (`_check_ranged_miss` — enabled the "miss hits interposed
  target" path; ranged-only, rats are melee).
- **debug.log evidence:** the cave-rat battle (V13ENT) + pig-rat battle
  (VAULTBUR) are engine-AI driven ("Cave Rat is using Rats packet" =
  `_combatai_msg`; "computing/sequencing/running attack" = `_combat_attack`).
  Attacks serialize in the log.
- **INSTRUMENTATION DEPLOYED (et tu stand, 2026-08-19):** `[COMBAT]` debug lines in
  `_combat_attack` (post-copy snapshot), `attackCompute` (post-switch state),
  `_action_melee` (branch chosen), `_combat_anim_finished` (what gets
  displayed/applied + turn_running), `_apply_damage` (who takes how much), `_gcsd`
  (active marker). Gated on `[debug] mode=log`. Next: reproduce the fight → grep
  `[COMBAT]` in the et tu stand's debug.log.
- **Not yet investigated:** the RPU-vs-et tu discriminator (reproduce the same
  fight in RPU).

---

## Recently fixed (do not re-open)

### Issue B — Save game failed in both stands — FIXED & USER-VERIFIED (2026-08-19)

**Symptom:** save always failed (`Error renaming temp save file to SAVE.DAT!`) in
BOTH RPU and et tu — even after the first fix.

**Root cause (two-layer):**
1. First fix (b9a290c) was incomplete: it prefixed the rename operands with
   `master_patches`, but the DESTINATION was built from the **global** `_gmpath`,
   which the save handlers clobber mid-save — `_GameMap2Slot` rewrites it to the
   slot-dir/AUTOMAP.DB.SAV paths while copying map files. By the post-loop rename the
   destination was garbage → ENOENT every time. The failure-path `compat_remove`
   (built from the LOCAL `_saveDatTmp` snapshot) succeeded, so no `.tmp` remained —
   which masked the real cause.
2. Same CWD-relative-rename class in the SAVE.DAT.BAK crash-recovery path
   (`lsgLoadGameInSlot`).

**Fix (1ef6861):** snapshot the relative save path into a LOCAL (`_saveDatRel`)
before the handler loop; build the rename destination from it. Crash-recovery rename
also `_patches`-prefixed.

**Verified by user:** save + load + quick-load work in BOTH RPU and et tu;
`SAVE.DAT` + `sfallgv.sav` land in `data/SAVEGAME/SLOT01/`; logs clean.

**Gotcha for future work:** `_gmpath` in loadsave.cc is a global shared across the
whole save/load flow — never read it after the handler loop runs.

### Issue D — Two-finger tap on macOS should act as right-click — FIXED & USER-VERIFIED (2026-08-19)

**Symptom:** two-finger tap does not right-click in-game. Corner-click worked;
two-finger tap produced nothing.

**Root cause:** the engine's mouse-button state comes from per-frame polling —
`mouseDeviceGetData()` calls SDL_GetMouseState() once per frame. macOS delivers a
two-finger tap as a fast right-button down+up pair; when both land between two polls
(sub-frame tap), the poll sees nothing and no right-button-down event is generated.

**Fix:** synthetic button-down latch in the input layer (dinput.cc
`gSyntheticDownButtons`): `mouseDeviceNoteButtonDown()` from
`_GNW95_process_message` on SDL_MOUSEBUTTONDOWN; `mouseDeviceGetData()` ORs the
latch into the polled state then clears it; reset on focus loss/gain. Left + right
both covered.

**Verified by user:** two-finger tap cycles the cursor mode in-game in BOTH stands;
single-finger tap-to-click also more reliable.

### Fixed session issues (2026-08-18, all committed, deployed, user-verified)

| # | Bug | Root cause | Fix |
|---|-----|-----------|-----|
| 1 | Dead world: empty inventory/log, static NPCs + crashes | **F-034 (fork 36414b6)**: `opExitProgram` set `program->exited=true` on O_EXIT_PROGRAM — the NORMAL script-termination opcode; `_updatePrograms` then freed every script's Program right after its start proc ran; `script->program` dangled → dead world or crash | Reverted F-034 in `opExitProgram` (interpreter.cc) — only sets the flag, never `exited=true`. Added `programListContains()` + DBGTRACE UAF detectors |
| 2 | Message log showed "Error" fallback everywhere | **BoostScriptDialogLimit** (ddraw.ini [Misc]) is a BOOLEAN in sfall, but fork commit 5222087 assigned the raw value as absolute capacity → capacity=1 → every message lookup failed | scripts.cc: nonzero → max capacity (10000) |
| 3 | `fs_copy: cannot open source` errors (RPU goris-derobing FRM patch inert; et tu classicWM art swap inert) | fs_copy used raw `compat_fopen` — cannot see .dat archive members or directory mods | `sfallVfsReadFile()` reads via engine VFS; same-path copies MATERIALIZE into `master_patches_path` (data/) |
| 4 | Worldmap screen black (music plays, travel works) | Fork kept stray `wmFadeOut()` at `wmWorldMapFunc` entry (worldmap.cc:3905) while upstream removed BOTH fades — incomplete merge left palette black | Removed the stray `wmFadeOut()` |
| 5 | `Undefined opcode 81a3` (et tu GLZFTERM.int, gl_pipboychk.int) | sfall's deprecated `eax_available()` deliberately unimplemented, but mods call it | 0-stub + `set_eax_environment` no-op |
| 6 | 48K "heap corruption detected" spam | Fork's heap-walk diagnostics used `ptr >= heapEnd` but the 0x8000 sentinel sits exactly AT heapEnd → false positive | `>=` → `>` in programMarkHeap/programPushString/interpreterPrintStats |
| 7 | Save game always failed (BOTH stands) | See Issue B above | See Issue B above |

---

## Debugging notes

- DBGTRACE instrumentation (program create/free/exit traces, UAF detectors, teardown
  markers, `[COMBAT]` lines) is silent unless `[debug] mode=log` is set in the cfg —
  then everything lands in `debug.log` in the game folder.
- `[debug] console_output_path=<file>` captures the in-game console.
