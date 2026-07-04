# Build Instructions

Fallout 2 CE Extended uses CMake as its build system. Whether you're targeting macOS, Linux, Windows, or WebAssembly, the build configuration is primarily driven through CMake presets.

## Prerequisites

All platforms require:

- **CMake** 3.19 or later
- **Ninja** (recommended for Linux and macOS)
- **SDL2** development libraries
- **zlib** development libraries

---

## macOS

### 1. Install build dependencies

```console
$ xcode-select --install
$ brew install cmake ninja
```

### 2. Configure and build

```console
$ cmake --preset macos-debug
$ cmake --build out/build/macos-debug --config Debug --target fallout2-ce
```

You should see:

```
** BUILD SUCCEEDED **
```

The build output is a universal binary (arm64 + x86_64) at `out/build/macos-debug/Debug/fallout2-ce.app`.

For a release build:

```console
$ cmake --preset macos-release
$ cmake --build out/build/macos-release --config RelWithDebInfo --target fallout2-ce
```

Alternative manual configuration without presets:

```console
$ cmake -S . -B out/build/macos -G Ninja -DCMAKE_BUILD_TYPE=Debug
$ cmake --build out/build/macos --target fallout2-ce
```

---

## Linux

### 1. Install build dependencies

**Debian / Ubuntu:**

```console
$ sudo apt install build-essential cmake ninja-build libsdl2-dev zlib1g-dev
```

**Fedora:**

```console
$ sudo dnf install gcc-c++ cmake ninja-build SDL2-devel zlib-devel
```

**Arch Linux:**

```console
$ sudo pacman -S base-devel cmake ninja sdl2 zlib
```

### 2. Configure and build

```console
$ cmake --preset linux-x64-debug
$ cmake --build out/build/linux-x64-debug --target fallout2-ce
```

You should see:

```
** BUILD SUCCEEDED **
```

The executable is at `out/build/linux-x64-debug/fallout2-ce`.

For a release build:

```console
$ cmake --preset linux-x64-release
$ cmake --build out/build/linux-x64-release --target fallout2-ce
```

For 32-bit x86 builds, use the `linux-x86-debug` or `linux-x86-release` presets (requires 32-bit multilib libraries).

---

## Windows

### Option A: Visual Studio 2022 (MSVC)

1. Install [Visual Studio 2022](https://visualstudio.microsoft.com/vs/) with the "Desktop development with C++" workload.
2. Install [CMake](https://cmake.org/download/).

Use the CMake presets:

```console
> cmake --preset windows-x64-debug
> cmake --build out/build/windows-x64-debug --config Debug
```

Available presets:
- `windows-x64-debug` / `windows-x64-release` — x86_64 MSVC
- `windows-x86-debug` / `windows-x86-release` — 32-bit MSVC (Win32)

### Option B: MinGW-w64 (cross-compile from Linux or macOS)

```console
$ cmake --preset Mingw-x64-Debug
$ cmake --build out/build/Mingw-x64-Debug
```

Available presets:
- `Mingw-x64-Debug` / `Mingw-x64-Release` — x86_64 MinGW
- `Mingw-x86-Debug` / `Mingw-x86-Release` — 32-bit MinGW

---

## WebAssembly (Emscripten)

### Option A: CMake presets (if Emscripten SDK is installed locally)

```console
$ cmake --preset emscripten-debug
$ cmake --build out/build/emscripten-debug
```

Available presets:
- `emscripten-debug`
- `emscripten-release`

The presets use the toolchain file at `cmake/toolchain/Emscripten.cmake`.

### Option B: Docker (no local Emscripten SDK needed)

```console
$ docker run --rm -v $(pwd):/src emscripten/emsdk:3.1.74 \
    sh -c 'mkdir -p build && cd build && \
    emcmake cmake ../ -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain/Emscripten.cmake && \
    emmake make'
```

The WebAssembly output is written to the `build/` directory.

A demo of the WebAssembly build is available at: https://github.com/ololoken/fallout2-ce-ems

---

## iOS

```console
$ cmake --preset ios-debug
$ cmake --build out/build/ios-debug --config Debug
```

Requires Xcode and a valid signing certificate for device deployment.

---

## Android

See `os/android/` for the Android NDK-based project. Open the `os/android/` directory in Android Studio and build from there.

---

## CLI Tools

The project includes two additional build targets:

- **ce-dat-tool** — Inspect and extract Fallout .dat archives:

  ```console
  $ cmake --build out/build/<preset-name> --target ce-dat-tool
  ```

- **ce-frm2png** — Convert Fallout FRM art to/from PNG:

  ```console
  $ cmake --build out/build/<preset-name> --target ce-frm2png
  ```

See [CONTRIBUTING.md](CONTRIBUTING.md) for detailed usage.

---

## Testing

The project uses in-game `.ssl` script tests in `sfall_testing/`. These require the Fallout 2 game runtime and the sfall Modders Pack `compile.exe` to execute. There is no headless C++ test runner.

To run tests interactively: load the game with the `gl_*.ssl` scripts compiled into your mod scripts directory. Each test script logs results to the in-game console.

See `sfall_testing/` for the full list of test scripts covering opcodes, hooks, and engine integration.
