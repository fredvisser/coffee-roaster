#!/bin/bash
# Coffee Roaster firmware workflow entrypoint.

set -euo pipefail

TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() {
    cat <<'EOF'
Usage: ./tools/firmware.sh <command> [options]

Commands:
  build       Compile the main firmware
  upload      Upload the latest matching build over USB
  ota-upload  Upload the latest matching build over Wi-Fi (alias: ota)
  pair        Request a persistent CLI session (macOS Keychain)
  monitor     Open the serial monitor for the main firmware target
  port        Show the detected serial port and any owning processes
  run         Compile, upload, and monitor over serial
  help        Show this help

Uploads build if compatible artifacts are missing. Use --rebuild with upload
or ota-upload to clean-compile first. Source edits do not invalidate a saved build.
Pairing and OTA use --host (hostname or IP address; default: roaster-dev.local).

Options:
    --board <target>          Select the board target
    --host <hostname>         Host for pairing and OTA operations
    --rebuild                 Force a clean build before upload
    --display-invert <0|1>    Invert LCD colors
    --touch-invert-x <0|1>    Invert touch coordinates on the X axis
    --touch-invert-y <0|1>    Invert touch coordinates on the Y axis

Boards:
    jc4827w543c   ESP32-S3 JC4827W543C display board target
    jc4827w543r   ESP32-S3 JC4827W543R resistive-touch board target

Examples:
  ./tools/firmware.sh build
  ./tools/firmware.sh build --board jc4827w543c
  ./tools/firmware.sh build --board jc4827w543r
  ./tools/firmware.sh build --board jc4827w543c --display-invert 1 --touch-invert-x 0 --touch-invert-y 0
  ./tools/firmware.sh pair --host roaster-dev.local
  ./tools/firmware.sh ota-upload --host roaster-dev.local --board jc4827w543c
  ./tools/firmware.sh upload --rebuild
EOF
}

command_name="${1:-help}"
if [[ $# -gt 0 ]]; then
    shift
fi

default_board_target="${ROASTER_DEFAULT_BOARD:-jc4827w543c}"
board="$default_board_target"
host="roaster-dev.local"
rebuild=0
display_invert="${ROASTER_DISPLAY_INVERT:-}"
touch_invert_x="${ROASTER_TOUCH_INVERT_X:-}"
touch_invert_y="${ROASTER_TOUCH_INVERT_Y:-}"
while [[ $# -gt 0 ]]; do
    case "$1" in
        --board)
            if [[ $# -lt 2 || -z "$2" || "$2" == -* ]]; then
                echo "--board requires a target" >&2
                exit 1
            fi
            board="$2"
            shift 2
            ;;
        --host)
            if [[ $# -lt 2 || -z "$2" || "$2" == -* ]]; then
                echo "--host requires a hostname or IP address" >&2
                exit 1
            fi
            host="$2"
            shift 2
            ;;
        --rebuild)
            rebuild=1
            shift
            ;;
        --display-invert|--touch-invert-x|--touch-invert-y)
            option_name="$1"
            option_value="${2:-}"
            if [[ "$option_value" != "0" && "$option_value" != "1" ]]; then
                echo "$option_name requires 0 or 1" >&2
                usage >&2
                exit 1
            fi
            case "$option_name" in
                --display-invert)
                    display_invert="$option_value"
                    ;;
                --touch-invert-x)
                    touch_invert_x="$option_value"
                    ;;
                --touch-invert-y)
                    touch_invert_y="$option_value"
                    ;;
            esac
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
    build|compile)
        action="compile"
        ;;
    upload)
        action="upload"
        ;;
    pair)
        action="pair"
        ;;
    ota|ota-upload)
        action="ota"
        ;;
    monitor)
        action="monitor"
        ;;
    port)
        action="port"
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

if [[ "$rebuild" == 1 ]]; then
    case "$action" in
        upload|ota) export ROASTER_FORCE_REBUILD=1 ;;
        *) echo "--rebuild is only supported for upload and ota-upload" >&2; exit 1 ;;
    esac
fi

if [[ -n "$board" ]]; then
    export ROASTER_TARGET_BOARD="$board"
fi
if [[ -n "$display_invert" ]]; then
    export ROASTER_DISPLAY_INVERT="$display_invert"
fi
if [[ -n "$touch_invert_x" ]]; then
    export ROASTER_TOUCH_INVERT_X="$touch_invert_x"
fi
if [[ -n "$touch_invert_y" ]]; then
    export ROASTER_TOUCH_INVERT_Y="$touch_invert_y"
fi

export OTA_HOST="$host"
exec "$TOOLS_DIR/workflow-runner.sh" main "$action"