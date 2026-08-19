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

---

## Debugging notes

- DBGTRACE instrumentation (program create/free/exit traces, UAF detectors, teardown
  markers, `[COMBAT]` lines) is silent unless `[debug] mode=log` is set in the cfg —
  then everything lands in `debug.log` in the game folder.
- `[debug] console_output_path=<file>` captures the in-game console.
