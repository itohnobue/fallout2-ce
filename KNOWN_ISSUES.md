# Known Issues

Current known issues for the Fallout 2 CE Extended engine with **RPU** and **Et Tu**.
Open issues carry their investigation state (root cause hypotheses, ruled-out work,
next steps) so nobody re-does work already done. Sources: in-game user reports and
debug-log analysis, 2026-08-18/19.

---

## Open issues

### Issue B — Hub Old Town: all NPCs are "Mike old town guard", broken dialogs, player skin swapped, Ian vanished (Et Tu)

In-game report, 2026-08-22 (session log: `smoke/fallout2-et-tu/Fallout1in2/debug.log`).

Symptoms:
- Hub Old Town (`HUBOLDTN.MAP`/`HUBOLDTN.SAV`): every NPC is described as
  "Mike old town guard"; weapon vendor dialog shows "Error in dialogs".
- Player character skin changed to a bald guy in combat armor (presumed that
  guard's appearance — likely Mike's proto data leaking into the player proto).
- Companion Ian disappeared.

Log evidence (this session):
- `Script Error: scripts\Mike.int: op_critter_add_trait: obj is NULL` — fires
  immediately before `MAP LOAD: HUBOLDTN.SAV` (Map load function #3 data size
  read: 167 bytes).
- Repeated ` ** Error in proto_dude_init()! **` + `Error: _proto_dude_init
  failed in protoReset()!` at map transitions/proto resets (multiple x, e.g.
  around lines 2687/3120).

Symptom pattern (collective NPC/proto reuse) suggests proto table corruption or
critter proto data being overwritten/recycled — NOT yet investigated (root
cause hypotheses not formed; next step: correlate Mike.int behavior with proto
load state around HUBOLDTN transitions, check proto_critter/pid reuse).

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
