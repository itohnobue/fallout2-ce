# Fallout 2 CE Extended

Fallout 2 CE Extended is a fully working re-implementation of the Fallout 2 engine, optimized for a hassle-free experience on multiple platforms.  It provides high resolution support, quality-of-life improvements, and dozens of bug fixes.

This is a fork of the original Fallout2: CE project, which is no longer getting regular updates.

## Why you may need this

Currently there is no easy way to play both Fallout 1 and Fallout 2 on Mac with Apple Silicon and all the updates installed (like RPU and Et Tu) to get the best gaming experience. This project is aimed to fix that and deliver a native macOS ARM Fallout 1-2 engine which is also compatible with all improvements.

## Mod Compatibility

The following total conversion mods are tested:

| Mod | Status | Notes |
| --- | --- | --- |
| [Fallout 2 Restoration Project (RPU)](https://github.com/BGforgeNet/Fallout2_Restoration_Project) | Supported (Beta) | New hooks: CarTravel, SetGlobalVar, Sneak. SpeedMulti with `ddraw.ini [Speed]` parsing. Null-pointer guards in critter stat opcodes. |
| [Fallout Et Tu](https://github.com/rotators/Fo1in2) | Partial | All 3 critical hooks implemented (USEANIMOBJ, DESCRIPTIONOBJ, SETLIGHTING). Engine reports sfall 4.5.1. Remaining gaps: some sfall opcodes. |

## License

The source code is this repository is available under the [Sustainable Use License](LICENSE.md).
