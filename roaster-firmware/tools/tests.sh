#!/bin/bash
# Coffee Roaster test workflow entrypoint.

set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    cat <<'EOF'
Usage: ./tools/tests.sh <command> <suite> [--board <target>]

Commands:
  list       Show available suites
  compile    Compile a suite
  upload     Compile and upload a suite
  monitor    Open the serial monitor for a suite target
  run        Compile, upload, and monitor a suite
  help       Show this help

Suites:
  pid
  profiles
  state
  safety
  unit
  hardware
  step-response

Boards:
  jc4827w543c

Examples:
  ./tools/tests.sh compile safety
  ./tools/tests.sh run pid --board jc4827w543c
EOF
}

map_suite() {
    case "$1" in
        pid)
            echo "2"
            ;;
        profiles|profile)
            echo "3"
            ;;
        state|state-machine|state_machine)
            echo "4"
            ;;
        safety)
            echo "5"
            ;;
        unit|runner)
            echo "6"
            ;;
        hardware|hardware-validation|hardware_validation)
            echo "7"
            ;;
        step-response|step_response)
            echo "8"
            ;;
        *)
            return 1
            ;;
    esac
}

command_name="${1:-help}"
if [[ $# -gt 0 ]]; then
    shift
fi

if [[ "$command_name" == "list" ]]; then
    usage
    exit 0
fi

suite="${1:-}"
if [[ $# -gt 0 ]]; then
    shift
fi

board=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)
            board="${2:-}"
            shift 2
            ;;
        -h|--help|help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

case "$command_name" in
    compile|build)
        legacy_action="compile"
        ;;
    upload)
        legacy_action="upload"
        ;;
    monitor)
        legacy_action="monitor"
        ;;
    run|all)
        legacy_action="all"
        ;;
    help|-h|--help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown command: $command_name" >&2
        usage >&2
        exit 1
        ;;
esac

if [[ -z "$suite" ]]; then
    echo "A test suite is required." >&2
    usage >&2
    exit 1
fi

suite_id=$(map_suite "$suite") || {
    echo "Unknown suite: $suite" >&2
    usage >&2
    exit 1
}

if [[ -n "$board" ]]; then
    export ROASTER_TARGET_BOARD="$board"
fi

export ROASTER_CLI_NAME="./tools/tests.sh"
exec "$TOOLS_DIR/roaster-cli.sh" "$suite_id" "$legacy_action"