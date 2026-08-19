# Known Issues

Current known issues for the Fallout 2 CE Extended engine with **RPU** and **Et Tu**.
Open issues carry their investigation state (root cause hypotheses, ruled-out work,
next steps) so nobody re-does work already done. Sources: in-game user reports and
debug-log analysis, 2026-08-18/19.

---

## Open issues

### Issue A — VOODOO write_* errors (Et Tu, low priority)

`Error during execution: VOODOO write_int/byte(0x00499xxx, ...) — NOT SUPPORTED in
CE engine` — gl_classic_wm.int's VOODOO_WriteNop procs attempt direct memory
patching (sfall-era engine addresses). The fork correctly rejects them. The
classicWM art swap now works via fs_copy, so these are benign-but-noisy. Mod-side
issue; engine behavior is correct.

### Issue B — Pip Boy rest/wait timers misbehave (Et Tu, open)

User report (2026-08-20): Pip Boy alarm clock rest/wait durations don't behave as
selected. Selecting "until 12:00" (noon) actually waits until 6:00; other options
misbehave too. A 2-hour rest advances the clock only ~30 minutes or so.

No investigation done yet. Entry points for a future investigation:
- Rest option dispatch: `pipboyHandleAlarmClock` (src/pipboy.cc:2140) maps
  eventCode → duration; fixed durations call `pipboyRest` directly
  (src/pipboy.cc:2168-2201)
- "Until" options compute hours/minutes via `_ClacTime` (src/pipboy.cc:2606)
  with configurable wake hours (`pipboyRestOptionWakeHour`, src/pipboy.cc:2662;
  defaults table at src/pipboy.cc:2675 — FO1 mode morning wake hour is 6,
  FO2 is 8)
- Actual time advancement in `pipboyRest`; game clock is minutes-of-day
  encoded (hour*100 + minute)

---

## Debugging notes

- DBGTRACE instrumentation (program create/free/exit traces, UAF detectors, teardown
  markers, `[COMBAT]` lines) is silent unless `[debug] mode=log` is set in the cfg —
  then everything lands in `debug.log` in the game folder.
- `[debug] console_output_path=<file>` captures the in-game console.
