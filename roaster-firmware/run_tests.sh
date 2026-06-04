#!/bin/bash
# Legacy wrapper for the old multi-purpose build script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

if [[ "${ROASTER_SUPPRESS_LEGACY_NOTICE:-0}" != "1" ]]; then
    echo "run_tests.sh is a legacy entrypoint. Prefer ./tools/firmware.sh or ./tools/tests.sh." >&2
fi

export ROASTER_CLI_NAME="./run_tests.sh"
exec "$SCRIPT_DIR/tools/roaster-cli.sh" "$@"
