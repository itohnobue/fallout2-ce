#!/usr/bin/env bash
set -euo pipefail

# Fallout 2 CE — macOS ARM64 Release Build Script
# Production-ready build for Apple Silicon Macs.  Configures with the
# macos-arm64-release CMake preset (Xcode generator, RelWithDebInfo),
# codesigns the .app bundle and .dmg, and verifies the output.
#
# Options:
#   --help      Show this help
#   --clean     Remove build directory before configuring
#   --no-sign   Skip code signing (unsigned output)
#   --sign-id   Explicit signing identity (overrides auto-resolution)
#
# Environment variables:
#   SIGNING_TEAM_ID    Apple Developer Team ID (default: 8P47J3K846)
#   SIGNING_IDENTITY   Explicit codesign identity name
#   SKIP_SIGNING       Set to 1 to skip signing (same as --no-sign)
#   CLEAN              Set to 1 for fresh build (same as --clean)
#   PRESET             CMake configure preset (default: macos-arm64-release)
#   CONFIGURATION      Xcode build configuration (default: RelWithDebInfo)

# ── Configurable variables ─────────────────────────────────────────

SIGNING_TEAM_ID="${SIGNING_TEAM_ID:-8P47J3K846}"
PRESET="${PRESET:-macos-arm64-release}"
CONFIGURATION="${CONFIGURATION:-RelWithDebInfo}"
APP_NAME="Fallout II Community Edition"
TARGET="${TARGET:-fallout2-ce}"
SKIP_SIGNING="${SKIP_SIGNING:-0}"
CLEAN="${CLEAN:-0}"
SIGNING_IDENTITY="${SIGNING_IDENTITY:-}"

# ── Helper functions ───────────────────────────────────────────────

usage() {
    cat <<EOF
Usage: $0 [--clean] [--no-sign] [--sign-id IDENTITY] [--help]

Options:
  --help       Show this help
  --clean      Remove build directory before configuring
  --no-sign    Skip code signing (unsigned .app and .dmg)
  --sign-id    Explicit signing identity name (overrides team-ID lookup)

Environment:
  SIGNING_TEAM_ID    Apple Developer Team ID (default: 8P47J3K846)
                     NOTE: Team ID is not a secret — it appears in every
                     signed binary's codesign metadata.
  SIGNING_IDENTITY   Full certificate common name for codesign.
                     Auto-resolved from SIGNING_TEAM_ID if not set.
  SKIP_SIGNING       Set to 1 to skip signing (same as --no-sign)
  CLEAN              Set to 1 to remove build directory first
  PRESET             CMake configure preset (default: macos-arm64-release)
  CONFIGURATION      Xcode build configuration (default: RelWithDebInfo)

EOF
    exit 0
}

log()    { echo "[build-macos] $*"; }
warn()   { echo "[build-macos] WARNING: $*" >&2; }
die()    { echo "[build-macos] ERROR: $*" >&2; exit 1; }

resolve_script_dir() {
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
    PROJECT_DIR="$SCRIPT_DIR"
}

# ── Pre-flight checks ──────────────────────────────────────────────

check_prerequisites() {
    log "Running pre-flight checks..."

    # 1. macOS only
    local os_name
    os_name=$(uname -s)
    [[ "$os_name" == "Darwin" ]] || die "This script is for macOS only. Detected: $os_name"

    # 2. ARM64 only
    local arch
    arch=$(uname -m)
    [[ "$arch" == "arm64" ]] || die "This script is for Apple Silicon (ARM64) only. Detected: $arch"

    # 3. Xcode Command Line Tools
    xcode-select -p &>/dev/null || die "Xcode Command Line Tools not found. Run: xcode-select --install"

    # 4. cmake available
    command -v cmake &>/dev/null || die "cmake not found. Install CMake >= 3.20 (3.21+ recommended)."

    # 5. cmake version >= 3.20 (cmake --build --preset requires 3.20+; 3.21+ used as conservative floor)
    local cmake_ver major minor
    cmake_ver=$(cmake --version 2>/dev/null | head -1 | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' || true)
    if [[ -z "$cmake_ver" ]]; then
        die "Could not parse cmake version."
    fi
    major=${cmake_ver%%.*}
    minor=${cmake_ver#*.}
    minor=${minor%%.*}
    if [[ "$major" -lt 3 ]] || { [[ "$major" -eq 3 ]] && [[ "$minor" -lt 20 ]]; }; then
        die "CMake >= 3.20 required (3.21 recommended). Found: $cmake_ver"
    fi

    # 6. security (keychain tool)
    command -v security &>/dev/null || die "security tool not found (part of macOS)."

    # 7. codesign
    command -v codesign &>/dev/null || die "codesign not found (part of Xcode CLT)."

    # 8. hdiutil
    command -v hdiutil &>/dev/null || die "hdiutil not found (part of macOS)."

    # 9. Git tree cleanliness (warn only)
    if git -C "$PROJECT_DIR" rev-parse --is-inside-work-tree &>/dev/null; then
        if ! git -C "$PROJECT_DIR" diff-index --quiet HEAD --; then
            warn "Working tree is dirty — uncommitted changes will be embedded in build metadata."
        fi
    else
        warn "Not a git repository — version metadata may be incomplete."
    fi

    # 10. Version consistency (warn only)
    local cmake_version git_tag
    cmake_version=$(grep 'MACOSX_BUNDLE_SHORT_VERSION_STRING' "$PROJECT_DIR/CMakeLists.txt" 2>/dev/null | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 || true)
    git_tag=$(git -C "$PROJECT_DIR" describe --tags --abbrev=0 2>/dev/null | sed 's/^v//' || true)
    if [[ -n "$cmake_version" ]] && [[ -n "$git_tag" ]] && [[ "$cmake_version" != "$git_tag" ]]; then
        warn "CMakeLists.txt version ($cmake_version) differs from git tag ($git_tag)."
    elif [[ -z "$git_tag" ]]; then
        warn "No git tags found — _BUILD_VER will be empty in the produced binary."
    fi
}

# ── Signing identity resolution ────────────────────────────────────

resolve_signing_identity() {
    if [[ "${SKIP_SIGNING:-0}" == "1" ]]; then
        log "Skipping code signing (SKIP_SIGNING=1)"
        return 0
    fi

    # Explicit identity takes precedence
    if [[ -n "${SIGNING_IDENTITY:-}" ]]; then
        log "Using explicit signing identity: $SIGNING_IDENTITY"
        return 0
    fi

    # Auto-resolve from team ID via keychain lookup
    local identities
    identities=$(security find-identity -v -p codesigning 2>/dev/null | \
        awk -F'"' -v tid="$SIGNING_TEAM_ID" '$0 ~ tid {print $2}')

    if [[ -z "$identities" ]]; then
        {
            echo "Available codesigning identities in keychain:"
            security find-identity -v -p codesigning 2>/dev/null || echo "  (none)"
        } >&2
        die "No codesigning identity found for team ID $SIGNING_TEAM_ID in keychain." \
            "To skip signing, set SKIP_SIGNING=1 or use --no-sign."
    fi

    local count
    count=$(echo "$identities" | wc -l | tr -d '[:space:]')
    if [[ "$count" -gt 1 ]]; then
        warn "Multiple identities match team ID $SIGNING_TEAM_ID:"
        echo "$identities" >&2
        die "Set SIGNING_IDENTITY explicitly to specify which one to use."
    fi

    SIGNING_IDENTITY="$identities"
    log "Resolved signing identity: $SIGNING_IDENTITY"

    # Warn if not distribution-grade
    if [[ "$SIGNING_IDENTITY" != *"Developer ID Application"* ]]; then
        warn "Identity is not 'Developer ID Application' — signed app may not be distributable outside your machine."
    fi
}

# ── Build ──────────────────────────────────────────────────────────

build() {
    BUILD_DIR="$PROJECT_DIR/out/build/$PRESET"

    if [[ "${CLEAN:-0}" == "1" ]]; then
        log "Cleaning build directory: $BUILD_DIR"
        rm -rf -- "$BUILD_DIR"
    fi

    log "Configuring with preset '$PRESET'..."
    cmake --preset "$PRESET" -S "$PROJECT_DIR"

    log "Building with preset '$PRESET' (configuration: $CONFIGURATION)..."
    cmake --build --preset "$PRESET"

    APP_PATH="$BUILD_DIR/$CONFIGURATION/$APP_NAME.app"
    if [[ ! -d "$APP_PATH" ]]; then
        die "Build did not produce expected .app bundle at: $APP_PATH"
    fi

    log "Build successful."
    log "App bundle: $APP_PATH"
}

# ── Code signing ───────────────────────────────────────────────────

sign_app() {
    if [[ "${SKIP_SIGNING:-0}" == "1" ]]; then
        log "Skipping .app code signing (SKIP_SIGNING=1)"
        return 0
    fi

    resolve_signing_identity

    log "Signing .app bundle: $APP_PATH"
    codesign --force --deep --options runtime --timestamp \
        --sign "$SIGNING_IDENTITY" \
        "$APP_PATH"

    log "Verifying .app code signature..."
    local verify_output
    verify_output=$(codesign --verify --deep --strict --verbose=2 "$APP_PATH" 2>&1) || {
        echo "$verify_output" >&2
        die ".app code signature verification FAILED."
    }

    log "Code signature verified."
    log "Signature details:"
    codesign -dvvv "$APP_PATH" 2>/dev/null | while IFS= read -r line; do
        log "  $line"
    done
}

# ── DMG creation ───────────────────────────────────────────────────

create_dmg() {
    log "Creating DMG..."

    local staging_dir
    staging_dir=$(mktemp -d) || die "Failed to create staging directory"

    # Set up cleanup trap for the staging dir
    trap 'rm -rf "$staging_dir"' EXIT

    # Copy the SIGNED .app into staging (preserves the signature — no re-sign)
    log "Copying signed .app to staging..."
    cp -R "$APP_PATH" "$staging_dir/" || die "Failed to copy .app to staging"

    # Create the DMG from the staging directory
    # UDZO = UDIF zlib-compressed (standard for distribution)
    DMG_PATH="$BUILD_DIR/$APP_NAME.dmg"

    # Remove existing DMG if present (hdiutil won't overwrite by default)
    rm -f "$DMG_PATH"

    if ! hdiutil create \
            -volname "$APP_NAME" \
            -srcfolder "$staging_dir" \
            -ov \
            -format UDZO \
            -imagekey zlib-level=9 \
            "$DMG_PATH"; then
        rm -rf "$staging_dir"
        die "Failed to create DMG"
    fi

    # Clean up staging
    rm -rf "$staging_dir"
    trap - EXIT

    # Verify DMG was created
    if [[ ! -f "$DMG_PATH" ]]; then
        die "DMG was not created at expected path: $DMG_PATH"
    fi

    log "DMG created: $DMG_PATH"
}

# ── Output verification ────────────────────────────────────────────

verify_outputs() {
    log "Verifying outputs..."

    # Re-verify .app signature
    if [[ "${SKIP_SIGNING:-0}" != "1" ]]; then
        codesign --verify --deep --strict --verbose=2 "$APP_PATH" || \
            die ".app code signature verification failed."
    fi

    # Verify DMG structural integrity
    hdiutil verify "$DMG_PATH" || die "DMG verification failed (may be corrupted)."

    # Verify DMG code signature (validates crypto seal, distinct from UDIF checksums)
    if [[ "${SKIP_SIGNING:-0}" != "1" ]]; then
        codesign --verify --verbose=4 "$DMG_PATH" || \
            die "DMG code signature verification failed."
    fi

    # Mount test: verify the .app inside the DMG
    log "Mounting DMG to verify contents..."
    local mount_point
    mount_point=$(mktemp -d) || die "Failed to create temp directory"
    trap 'hdiutil detach "$mount_point" 2>/dev/null || true; rmdir "$mount_point" 2>/dev/null || true' EXIT INT TERM

    if ! hdiutil attach -nobrowse -mountpoint "$mount_point" "$DMG_PATH" > /dev/null 2>&1; then
        rmdir "$mount_point" 2>/dev/null || true
        die "Failed to mount DMG for verification."
    fi

    local app_in_dmg
    app_in_dmg=$(find "$mount_point" -name "*.app" -maxdepth 2 -type d | head -1)
    if [[ -z "$app_in_dmg" ]]; then
        hdiutil detach "$mount_point" 2>/dev/null || true
        rmdir "$mount_point" 2>/dev/null || true
        die "No .app bundle found inside DMG."
    fi

    log "Found app in DMG: $app_in_dmg"

    if [[ "${SKIP_SIGNING:-0}" != "1" ]]; then
        codesign --verify --deep --strict "$app_in_dmg" || {
            hdiutil detach "$mount_point" 2>/dev/null || true
            rmdir "$mount_point" 2>/dev/null || true
            die "App inside DMG signature verification failed."
        }
        log "App signature inside DMG verified."
    fi

    hdiutil detach "$mount_point" 2>/dev/null || true
    rmdir "$mount_point" 2>/dev/null || true
    trap - EXIT INT TERM

    log "All verifications passed."
}

# ── Summary ────────────────────────────────────────────────────────

print_summary() {
    local duration
    duration=$((SECONDS - START_TIME))

    echo ""
    echo "═══════════════════════════════════════════════════════════"
    echo "  Build Complete"
    echo "═══════════════════════════════════════════════════════════"
    echo "  App:  $APP_PATH"
    echo "  DMG:  $DMG_PATH"
    if [[ "${SKIP_SIGNING:-0}" == "1" ]]; then
        echo "  Sign: unsigned (SKIP_SIGNING=1)"
    else
        echo "  Sign: $SIGNING_IDENTITY"
    fi
    echo "  Time: ${duration}s"
    echo "═══════════════════════════════════════════════════════════"
}

# ── Main ───────────────────────────────────────────────────────────

main() {
    START_TIME=$SECONDS

    resolve_script_dir

    # Parse arguments
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --help|-h)
                usage
                ;;
            --clean)
                CLEAN=1
                shift
                ;;
            --no-sign)
                SKIP_SIGNING=1
                shift
                ;;
            --sign-id)
                if [[ $# -lt 2 || "$2" == -* ]]; then
                    die "--sign-id requires a value (got: ${2:-<none>})"
                fi
                SIGNING_IDENTITY="$2"
                shift 2
                ;;
            *)
                die "Unknown option: $1"
                ;;
        esac
    done

    log "Fallout 2 CE — macOS ARM64 Release Build"
    log "Preset: $PRESET | Configuration: $CONFIGURATION"
    log "Signing: $([ "${SKIP_SIGNING:-0}" = "1" ] && echo "DISABLED" || echo "team ID $SIGNING_TEAM_ID")"

    check_prerequisites
    build
    sign_app
    create_dmg

    if [[ "${SKIP_SIGNING:-0}" != "1" ]]; then
        resolve_signing_identity
        log "Signing DMG..."
        codesign --force --sign "$SIGNING_IDENTITY" "$DMG_PATH"
        log "DMG signed."
    else
        log "Skipping DMG signing (SKIP_SIGNING=1)"
    fi

    verify_outputs
    print_summary

    exit 0
}

main "$@"
