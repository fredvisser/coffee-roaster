#!/bin/bash
# Legacy wrapper for the old dependency setup script.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

echo "setup_libraries.sh is a legacy entrypoint. Prefer ./tools/bootstrap.sh." >&2
exec "$SCRIPT_DIR/tools/bootstrap.sh" "$@"
