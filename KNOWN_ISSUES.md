# Known Issues

Current known issues for the Fallout 2 CE Extended engine with **RPU** and **Et Tu**.
Open issues carry their investigation state (root cause hypotheses, ruled-out work,
next steps) so nobody re-does work already done. Sources: in-game user reports and
debug-log analysis, 2026-08-18/19.

---

## Open issues

### ~~Issue B — Hub Old Town: all NPCs are "Mike old town guard", broken dialogs, player skin swapped, Ian vanished (Et Tu)~~ — **FIXED (engine commit, 2026-08-23)**

Root cause (found via debug.log + save-file forensics):

- The fork's `set_scr_name` metarule (RPU/Et-Tu compat layer, `src/sfall_metarules.cc`)
  wrote a **global script-file-name override** (`gScriptNameOverride`) on top of its
  per-sid critter rename. `scriptsGetFileName()` (`src/scripts.cc:1917`) honored that
  override **before** `scripts.lst` — so **one** call of `set_scr_name("Mike")`
  (et-tu's `Mike.int`, which calls it legitimately to rename its Old Town guard
  critters) made **every** script in the game resolve to `Mike.int`: map script,
  doors, all NPCs, party members — exactly the all-Mike `programCreate` flood in the
  log after leaving HUBOLDTN, the "You see: Mike, the Old Town guard." for every NPC
  (critter name = msg `101 + scriptIndex`, index 820 = Mike), the broken dialogs
  (Mike.int running as everyone, `op_critter_add_trait: obj is NULL`), and Ian's
  "vanishing" (Ian.int was never loaded).
- **Persistence made it permanent:** the override is serialized into `sfallgv.sav`
  (metarule stream) and restored on every load. SLOT01's `sfallgv.sav` contained the
  literal string `Mike` — any save taken after the pollution is burned; SLOT02 was
  clean (usable for testing).

Fix (3 files):

1. `src/scripts.cc` — `scriptsGetFileName()` no longer applies any script-name
   override; file names always come from `scripts.lst`.
2. `src/sfall_metarules.cc` — `mf_set_scr_name` keeps only the per-sid critter-name
   override (the actual et-tu feature); never sets the global.
3. `src/sfall_metarules.cc` — metarule save/load: format slot kept for compatibility
   (written empty); on load a stale override is **read and discarded** (logged) so
   poisoned saves load cleanly.

Regression tests added: `tests/test_fixes_metarules.cc` (Issue B cases). Build green,
92/92 tests pass; both stands deployed (2026-08-23). Verify in game: load **SLOT02**
(or an older save), walk Hub Old Town → all NPCs should show their real names/scripts.
`proto_dude_init` boot/reset noise remains (known, benign — see notes in AGENTS.md).

---

## Debugging notes

`Error during execution: VOODOO write_int/byte(0x00499xxx, ...) — NOT SUPPORTED in
CE engine` — gl_classic_wm.int's VOODOO_WriteNop procs attempt direct memory
patching (sfall-era engine addresses). The fork correctly rejects them. The
classicWM art swap now works via fs_copy, so these are benign-but-noisy. Mod-side
issue; engine behavior is correct.

---

## Debugging notes

- DBGTRACE instrumentation (program create/free/exit traces, UAF detectors, teardown
  markers, `[COMBAT]` lines) is silent unless `[debug] mode=log` is set in the cfg —
  then everything lands in `debug.log` in the game folder.
- `[debug] console_output_path=<file>` captures the in-game console.
