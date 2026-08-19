# Fallout 2 CE Extended

**A fork of the Fallout 2 Community Engine, targeting native macOS ARM support for the modern total-conversion mods (RPU, Et Tu) — both fully working and currently in the testing stage.**

Upstream CE runs on macOS, but the big total conversions (RPU, Et Tu) require [sfall](https://github.com/sfall-team/sfall) — a Windows-only engine extension. This project reimplements the sfall scripting surface natively inside CE, so the mods can run on the fork without a Windows layer. The fork point is motivated by the lack of an easy way to play Fallout 1 and 2 on Apple Silicon with the modern mod scene installed.

## Current status

**Both mods work, run and are playable on this fork, and are currently going through testing to catch minor bugs.** Issues found during testing are tracked and fixed as they surface.

| Mod | Status | Notes |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | Supported | Works, runs and is playable end-to-end; currently going through testing to catch minor bugs. RPU's 4 hooks, ~25 opcodes/metarules, config keys and UPU extras implemented; all 25 requirement rows + 6 remaining-work items verified against the RPU source (2026-08-17). |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | Supported | Works, runs and is playable end-to-end; currently going through testing to catch minor bugs. FO1-mode engine behavior and Et Tu's sfall surface implemented; 30 of 33 requirement rows verified against source (2026-08-17). |

Requirement-by-requirement status for both mods, including the known gaps, lives in [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md).

## Docs

- [INSTALL.md](INSTALL.md) — build and install for all platforms
- [SFALL_COMPATIBILITY.md](SFALL_COMPATIBILITY.md) — sfall / RPU / Et Tu compatibility reference: what's implemented vs. upstream CE, remaining-work checklists
- [CHANGELOG.md](CHANGELOG.md) — release history

## License

The source code in this repository is available under the [Sustainable Use License](LICENSE.md).
