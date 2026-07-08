#!/usr/bin/env bash
set -euo pipefail

# Fallout 2 CE — Linux Build Script
# Builds the native Linux executable with CMake.
#
# Options:
#   --debug    Build Debug configuration (default)
#   --release  Build RelWithDebInfo configuration
#   --x86      Build 32-bit x86 (requires multilib toolchain)
#   --help     Show this help

ARCH="x64"
BUILD_TYPE="Debug"

usage() {
    cat <<EOF
Usage: $0 [--debug|--release] [--x86] [--help]

Options:
  --debug     Build Debug configuration (default)
  --release   Build RelWithDebInfo configuration
  --x86       Build 32-bit x86 (requires g++-multilib)
  --help      Show this help

Environment:
  Requires: cmake >= 3.20 (3.21 recommended), C++17 compiler, SDL2, zlib

EOF
    exit 0
}

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="RelWithDebInfo"
            shift
            ;;
        --x86)
            ARCH="x86"
            shift
            ;;
        --help|-h)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

PRESET="linux-${ARCH}-debug"

if [[ "$BUILD_TYPE" == "RelWithDebInfo" ]]; then
    PRESET="linux-${ARCH}-release"
fi

# Install system dependencies.  SDL2 and zlib are required.  On
# Ubuntu / Debian packaging conventions are stable enough to
# detect with apt-get; other distributions need manual setup.
install_deps() {
    if command -v apt-get &>/dev/null; then
        echo "[build-linux] Installing dependencies via apt-get..."
        sudo apt-get update -qq

        if [[ "$ARCH" == "x86" ]]; then
            # 32-bit cross-compile toolchain + SDL2 / zlib :i386
            sudo dpkg --add-architecture i386
            sudo apt-get update -qq
            sudo apt-get install -y --no-install-recommends \
                g++-multilib \
                libsdl2-dev:i386 \
                zlib1g-dev:i386
        else
            sudo apt-get install -y --no-install-recommends \
                libsdl2-dev \
                zlib1g-dev
        fi
    elif command -v dnf &>/dev/null; then
        echo "[build-linux] Installing dependencies via dnf..."
        if [[ "$ARCH" == "x86" ]]; then
            sudo dnf install -y glibc-devel.i686 libstdc++-devel.i686 SDL2-devel.i686 zlib-devel.i686
        else
            sudo dnf install -y SDL2-devel zlib-devel
        fi
    elif command -v pacman &>/dev/null; then
        echo "[build-linux] Installing dependencies via pacman..."
        if [[ "$ARCH" == "x86" ]]; then
            sudo pacman -S --needed --noconfirm lib32-sdl2 lib32-zlib
        else
            sudo pacman -S --needed --noconfirm sdl2 zlib
        fi
    else
        echo "[build-linux] WARNING: could not detect package manager."
        echo "  Please install SDL2 and zlib development headers manually."
    fi
}

# ── main ──────────────────────────────────────────────────────────

echo "[build-linux] Preset: $PRESET"
echo "[build-linux] Build type: $BUILD_TYPE | Arch: $ARCH"

# Optionally install dependencies (skip if already present)
if [[ "${SKIP_DEPS:-}" != "1" ]]; then
    install_deps
else
    echo "[build-linux] SKIP_DEPS=1 — skipping dependency installation"
fi

if ! command -v cmake &>/dev/null; then
    echo "[build-linux] ERROR: cmake not found. Install CMake >= 3.20 (3.21 recommended)."
    exit 1
fi

# Check cmake version >= 3.20 (cmake --build --preset requires 3.20; 3.21 for conservative floor)
cmake_ver=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)
if [[ -z "$cmake_ver" ]]; then
    echo "[build-linux] ERROR: Could not parse cmake version."
    exit 1
fi
major=${cmake_ver%%.*}
minor=${cmake_ver#*.}
minor=${minor%%.*}
if [[ "$major" -lt 3 ]] || { [[ "$major" -eq 3 ]] && [[ "$minor" -lt 20 ]]; }; then
    echo "[build-linux] ERROR: CMake >= 3.20 required (3.21 recommended). Found: $cmake_ver"
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

echo "[build-linux] Configuring..."
cmake --preset "$PRESET" -S "$PROJECT_DIR"

echo "[build-linux] Building..."
cmake --build --preset "$PRESET"

echo "[build-linux] Done — binary at out/build/${PRESET}/fallout2-ce"
