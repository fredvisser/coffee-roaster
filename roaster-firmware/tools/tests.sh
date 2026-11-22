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
  host          Deterministic control regressions (no hardware)
  host-json     PID history serialization (installed ArduinoJson, no hardware)

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
            echo "pid"
            ;;
        profiles|profile)
            echo "profiles"
            ;;
        state|state-machine|state_machine)
            echo "state"
            ;;
        safety)
            echo "safety"
            ;;
        unit|runner)
            echo "unit"
            ;;
        hardware|hardware-validation|hardware_validation)
            echo "hardware"
            ;;
        step-response|step_response)
            echo "step-response"
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

if [[ ( "${1:-}" == "host" || "${1:-}" == "host-json" ) && "$command_name" == "run" ]]; then
    test_binary="$(mktemp "${TMPDIR:-/tmp}/roaster-control.XXXXXX")"
    trap 'rm -f "$test_binary"' EXIT
    source_file="control_test.cpp"
    compile_args=(-std=c++17 -Wall -Wextra -Werror -I "$TOOLS_DIR/../tests/host")
    if [[ "$1" == "host-json" ]]; then
        source_file="pid_json_test.cpp"
        compile_args+=(-I "$(arduino-cli config get directories.user)/libraries/ArduinoJson/src")
    fi
    "${CXX:-c++}" "${compile_args[@]}" "$TOOLS_DIR/../tests/host/$source_file" -o "$test_binary"
    "$test_binary"
    exit
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
        action="compile"
        ;;
    upload)
        action="upload"
        ;;
    monitor)
        action="monitor"
        ;;
    run|all)
        action="run"
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

suite_target=$(map_suite "$suite") || {
    echo "Unknown suite: $suite" >&2
    usage >&2
    exit 1
}

if [[ -n "$board" ]]; then
    export ROASTER_TARGET_BOARD="$board"
fi

exec "$TOOLS_DIR/workflow-runner.sh" "$suite_target" "$action"