#!/usr/bin/env bash
set -euo pipefail

# Fallout 2 CE — format C++ sources with clang-format via Docker.
# Runs clang-format 18 (silkeh/clang:18) on all .cc/.h files under src/.
# Safe to re-run: no-op when no files match; idempotent once formatted.

# Resolve the repo root from this script's location so it works from any CWD.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)" || {
    echo "ERROR: cannot resolve script directory." >&2
    exit 1
}
cd "$SCRIPT_DIR" || {
    echo "ERROR: cannot cd to $SCRIPT_DIR." >&2
    exit 1
}

if [[ ! -d "$SCRIPT_DIR/src" ]]; then
    echo "ERROR: src/ directory not found at $SCRIPT_DIR/src." >&2
    exit 1
fi

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker not found in PATH." >&2
    exit 1
fi

# One-shot container: --rm discards the container, --user maps host uid:gid
# so bind-mounted files keep host ownership (not root).
if ! docker run --rm \
    -v "$PWD:/app" --workdir /app \
    --user "$(id -u):$(id -g)" silkeh/clang:18 \
    bash -c 'set -euo pipefail; find src -type f \( -name \*.cc -o -name \*.h \) -print0 | xargs -0 -r clang-format -i'; then
    echo "ERROR: docker run failed (is the Docker daemon running?)." >&2
    exit 1
fi
