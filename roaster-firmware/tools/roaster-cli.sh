#!/bin/bash
# Coffee Roaster - Shared compile/upload runner
# Canonical entrypoints are tools/firmware.sh and tools/tests.sh.

set -euo pipefail

# Configuration
TARGET_BOARD="${ROASTER_TARGET_BOARD:-jc4827w543c}"
DEFAULT_BOARD_FQBN="esp32:esp32:esp32s3:UploadSpeed=921600,USBMode=hwcdc,CDCOnBoot=cdc,MSCOnBoot=default,DFUOnBoot=default,UploadMode=default,CPUFreq=240,FlashMode=dio,FlashSize=4M,PartitionScheme=no_fs,DebugLevel=none,PSRAM=opi,LoopCore=1,EventsCore=1,EraseFlash=none,JTAGAdapter=default,ZigbeeMode=default"
BOARD_FQBN="${BOARD_FQBN:-$DEFAULT_BOARD_FQBN}"
SERIAL_PORT="${SERIAL_PORT:-auto}"
BAUD_RATE=115200
OTA_SCHEME="${OTA_SCHEME:-http}"
OTA_HOST="${OTA_HOST:-roaster-dev.local}"
OTA_PORT="${OTA_PORT:-80}"
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(cd "$TOOLS_DIR/.." && pwd)"
FIRMWARE_BUILD_DIR="$FIRMWARE_DIR/build"
TESTS_DIR="$FIRMWARE_DIR/tests"
CLI_NAME="${ROASTER_CLI_NAME:-$0}"
BUILD_JOBS="${ROASTER_BUILD_JOBS:-}"

if [[ -z "$BUILD_JOBS" ]]; then
    if command -v sysctl >/dev/null 2>&1; then
        BUILD_JOBS="$(sysctl -n hw.ncpu 2>/dev/null || true)"
    fi
    if [[ -z "$BUILD_JOBS" ]] && command -v getconf >/dev/null 2>&1; then
        BUILD_JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
    fi
    BUILD_JOBS="${BUILD_JOBS:-1}"
fi

if ! [[ "$BUILD_JOBS" =~ ^[1-9][0-9]*$ ]]; then
    echo "Invalid ROASTER_BUILD_JOBS value: $BUILD_JOBS" >&2
    exit 1
fi

BUILD_EXTRA_FLAGS="-DROASTER_TARGET_BOARD=ROASTER_BOARD_JC4827W543C -DROASTER_DISPLAY_BACKEND=ROASTER_DISPLAY_BACKEND_LVGL"

case "$TARGET_BOARD" in
    jc4827w543c)
        BUILD_EXTRA_FLAGS="-DROASTER_TARGET_BOARD=ROASTER_BOARD_JC4827W543C -DROASTER_DISPLAY_BACKEND=ROASTER_DISPLAY_BACKEND_LVGL"
        ;;
    jc4827w543r)
        BUILD_EXTRA_FLAGS="-DROASTER_TARGET_BOARD=ROASTER_BOARD_JC4827W543R -DROASTER_DISPLAY_BACKEND=ROASTER_DISPLAY_BACKEND_LVGL"
        ;;
    *)
        echo "Unknown ROASTER_TARGET_BOARD: $TARGET_BOARD" >&2
        exit 1
        ;;
esac

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Resolve serial port - try multiple patterns for ESP32
resolve_serial_port() {
    if [[ "$SERIAL_PORT" == "auto" ]] || [[ "$SERIAL_PORT" == *"*"* ]]; then
        # Try common ESP32 port patterns
        local patterns=(
            "/dev/cu.usbmodem*"
            "/dev/cu.usbserial-*"
            "/dev/cu.SLAB_USBtoUART*"
            "/dev/cu.wchusbserial*"
        )

        for pattern in "${patterns[@]}"; do
            ports=($pattern)
            if [ ${#ports[@]} -gt 0 ] && [ -e "${ports[0]}" ]; then
                SERIAL_PORT="${ports[0]}"
                print_info "Detected serial port: $SERIAL_PORT"
                return 0
            fi
        done

        # No port found
        echo -e "${RED}❌ No serial port found. Tried patterns:${NC}"
        for pattern in "${patterns[@]}"; do
            echo -e "${RED}   - $pattern${NC}"
        done
        echo -e "${YELLOW}💡 Specify manually: export SERIAL_PORT=/dev/cu.YOUR_PORT${NC}"
        echo -e "${YELLOW}💡 List ports: ls /dev/cu.*${NC}"
        exit 1
    fi
}

print_header() {
    echo ""
    echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║ $1${NC}"
    echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
    echo ""
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${YELLOW}ℹ️  $1${NC}"
}

compile_sketch() {
    local sketch_path="$1"
    local sketch_name=$(basename "$sketch_path" .ino)
    local all_extra_flags="$BUILD_EXTRA_FLAGS"

    print_info "Compiling $sketch_name..."

    print_info "Target board: $TARGET_BOARD ($BOARD_FQBN)"

    if [[ "$sketch_name" == "roaster-firmware" ]]; then
        local build_version="${ROASTER_BUILD_VERSION:-}"
        if [[ -z "$build_version" ]]; then
            build_version="$(git -C "$FIRMWARE_DIR" rev-parse --short HEAD 2>/dev/null || true)"
            build_version="${build_version:-dev}"
        fi
        all_extra_flags="${all_extra_flags:+$all_extra_flags }-DVERSION=\"${build_version}\""
        print_info "Firmware version: $build_version"
    fi

    local build_args=(--jobs "$BUILD_JOBS" --fqbn "$BOARD_FQBN")
    local arduino_build_dir="$FIRMWARE_BUILD_DIR/.arduino"
    if [[ "$sketch_name" == "roaster-firmware" ]]; then
        # ponytail: publish from this compile's directory, never a cache-wide search.
        build_args+=(--build-path "$arduino_build_dir")
        if [[ "${ROASTER_FORCE_REBUILD:-0}" == 1 ]]; then
            build_args+=(--clean)
        fi
    fi
    if [[ -n "$all_extra_flags" ]]; then
        build_args+=(--build-property "compiler.cpp.extra_flags=$all_extra_flags")
        build_args+=(--build-property "compiler.c.extra_flags=$all_extra_flags")
    fi

    if arduino-cli compile "${build_args[@]}" "$sketch_path" 2>&1 | tee /tmp/compile.log; then
        print_success "$sketch_name compiled successfully"
        if [[ "$sketch_name" == "roaster-firmware" ]]; then
            copy_build_artifacts "$arduino_build_dir" || return 1
        fi
        return 0
    else
        print_error "$sketch_name compilation failed"
        tail -20 /tmp/compile.log
        return 1
    fi
}

copy_build_artifacts() {
    local source="$1" file
    for file in roaster-firmware.ino.bin roaster-firmware.ino.bootloader.bin partitions.csv build.options.json; do
        if [[ ! -s "$source/$file" ]]; then
            print_error "Compile did not produce $source/$file"
            return 1
        fi
    done
    # Publish metadata last so a partial copy cannot pass the reuse check.
    rm -f "$FIRMWARE_BUILD_DIR/build.options.json" &&
        cp "$source/roaster-firmware.ino.bin" "$FIRMWARE_BUILD_DIR/roaster-firmware.bin" &&
        cp "$source/roaster-firmware.ino.bootloader.bin" "$FIRMWARE_BUILD_DIR/roaster-firmware.bootloader.bin" &&
        cp "$source/partitions.csv" "$FIRMWARE_BUILD_DIR/" &&
        cp "$source/build.options.json" "$FIRMWARE_BUILD_DIR/" || return 1
    print_success "Build artifact copied to: $FIRMWARE_BUILD_DIR/roaster-firmware.bin"
}

flash_jc_main_firmware() {
    local sketch_name="$1"
    local sketch_dir="$FIRMWARE_BUILD_DIR"
    local app_bin="$sketch_dir/$sketch_name.bin"
    local bootloader_bin="$sketch_dir/$sketch_name.bootloader.bin"
    local partitions_csv="$sketch_dir/partitions.csv"
    if [[ ! -f "$app_bin" ]] || [[ ! -f "$bootloader_bin" ]] || [[ ! -f "$partitions_csv" ]]; then
        print_error "Incomplete JC flash bundle. Expected app, bootloader, and partitions artifacts."
        return 1
    fi

    local esp32_pkg_dir
    esp32_pkg_dir=$(find "$HOME/Library/Arduino15/packages/esp32/hardware/esp32" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -1)
    if [[ -z "$esp32_pkg_dir" ]]; then
        print_error "Unable to locate ESP32 hardware package"
        return 1
    fi

    local esptool_bin
    esptool_bin=$(find "$HOME/Library/Arduino15/packages/esp32/tools/esptool_py" -path '*/esptool' -type f 2>/dev/null | sort -V | tail -1)
    if [[ -z "$esptool_bin" ]]; then
        print_error "Unable to locate esptool"
        return 1
    fi

    local gen_part_py="$esp32_pkg_dir/tools/gen_esp32part.py"
    local boot_app0_bin="$esp32_pkg_dir/tools/partitions/boot_app0.bin"
    local partition_bin
    partition_bin=$(mktemp "${TMPDIR:-/tmp}/roaster-partitions.XXXXXX.bin")

    python3 "$gen_part_py" "$partitions_csv" "$partition_bin" >/tmp/gen_partitions.log 2>&1 || {
        print_error "Failed to generate partition table binary"
        cat /tmp/gen_partitions.log 2>/dev/null || true
        rm -f "$partition_bin"
        return 1
    }

    print_info "Flashing JC firmware bundle with OTA-capable partition layout..."
    if "$esptool_bin" --chip esp32s3 --port "$SERIAL_PORT" --baud 921600 write-flash \
        0x0 "$bootloader_bin" \
        0x8000 "$partition_bin" \
        0xe000 "$boot_app0_bin" \
        0x10000 "$app_bin" 2>&1 | tee /tmp/upload.log; then
        rm -f "$partition_bin"
        print_success "$sketch_name uploaded successfully"
        sleep 2
        return 0
    fi

    rm -f "$partition_bin"
    print_error "$sketch_name upload failed"
    tail -20 /tmp/upload.log
    return 1
}

upload_sketch() {
    local sketch_path="$1"
    local sketch_name=$(basename "$sketch_path" .ino)

    resolve_serial_port

    print_info "Uploading $sketch_name to $SERIAL_PORT..."

    print_info "Target board: $TARGET_BOARD ($BOARD_FQBN)"

    if [[ "$TARGET_BOARD" == "jc4827w543c" || "$TARGET_BOARD" == "jc4827w543r" ]] && [[ "$sketch_name" == "roaster-firmware" ]]; then
        flash_jc_main_firmware "$sketch_name"
        return $?
    fi

    local upload_args=(-p "$SERIAL_PORT" --fqbn "$BOARD_FQBN")

    local cache_dir="$HOME/Library/Caches/arduino/sketches"
    local bin_file
    bin_file=$(find "$cache_dir" -name "$sketch_name.ino.bin" -type f -mmin -10 2>/dev/null | head -1)
    if [[ -n "$bin_file" ]] && [[ -f "$bin_file" ]]; then
        upload_args+=(--build-path "$(dirname "$bin_file")")
    fi

    if arduino-cli upload "${upload_args[@]}" "$sketch_path" 2>&1 | tee /tmp/upload.log; then
        print_success "$sketch_name uploaded successfully"
        sleep 2
        return 0
    else
        print_error "$sketch_name upload failed"
        tail -20 /tmp/upload.log
        return 1
    fi
}

ota_base_url() {
    echo "${OTA_SCHEME}://${OTA_HOST}:${OTA_PORT}"
}

ota_start_session() {
    ota_curl --fail --silent --show-error --http1.1 --connect-timeout 5 --max-time 20 \
        "$1/ota/start?mode=fw&hash=$md5_hash" >/tmp/ota_start.log
}

ota_send_firmware() {
    local base_url="$1"
    local firmware_bin="$2"
    ota_curl --fail --silent --show-error --http1.1 --connect-timeout 10 --max-time 180 \
        -F "file=@$firmware_bin;type=application/octet-stream" \
        "$base_url/ota/upload" >/tmp/ota_upload.log
}

firmware_artifact_matches_target() {
    local metadata="$FIRMWARE_BUILD_DIR/build.options.json"
    [[ -s "$FIRMWARE_BUILD_DIR/roaster-firmware.bin" && -s "$metadata" ]] &&
        grep -Fq "\"fqbn\": \"$BOARD_FQBN\"" "$metadata" &&
        grep -Fq -- "compiler.cpp.extra_flags=$BUILD_EXTRA_FLAGS " "$metadata"
}

ensure_firmware_build() {
    local sketch_path="$1" transport="$2"
    if [[ "$(basename "$sketch_path")" != roaster-firmware.ino ]]; then
        compile_sketch "$sketch_path"
        return $?
    fi

    if [[ "${ROASTER_FORCE_REBUILD:-0}" == "1" ]]; then
        print_info "Forced rebuild requested"
    elif firmware_artifact_matches_target &&
        { [[ "$transport" == ota ]] ||
          [[ -s "$FIRMWARE_BUILD_DIR/roaster-firmware.bootloader.bin" && -s "$FIRMWARE_BUILD_DIR/partitions.csv" ]]; }; then
        print_info "Reusing build/roaster-firmware.bin for $TARGET_BOARD"
        return 0
    else
        print_info "No complete build for $TARGET_BOARD; compiling before upload"
    fi

    compile_sketch "$sketch_path" || return 1
    if ! firmware_artifact_matches_target; then
        print_error "Compiled artifact metadata does not match $TARGET_BOARD ($BOARD_FQBN)"
        return 1
    fi
}

firmware_md5() {
    local file="$1"
    if command -v md5 >/dev/null 2>&1; then
        md5 -q "$file"
    elif command -v md5sum >/dev/null 2>&1; then
        md5sum "$file" | awk '{print $1}'
    else
        print_error "No MD5 tool found (expected md5 or md5sum)"
        return 1
    fi
}

ota_upload_firmware() {
    local sketch_path="$1"
    local firmware_bin="$FIRMWARE_BUILD_DIR/roaster-firmware.bin"
    local token="${OTA_TOKEN:-}"
    if [[ -z "$token" ]] && command -v security >/dev/null 2>&1; then
        token=$(security find-generic-password -s coffee-roaster -a "${OTA_SCHEME}://${OTA_HOST}:${OTA_PORT}" -w 2>/dev/null) || token=""
    fi
    if [[ -z "$token" ]]; then
        print_error "No OTA session for $OTA_HOST. Run ./tools/firmware.sh pair with the same OTA_HOST first."
        return 1
    fi
    if [[ ! "$token" =~ ^[0-9a-f]{64}$ ]]; then
        print_error "Invalid OTA session token"
        return 1
    fi

    ensure_firmware_build "$sketch_path" ota || return 1
    local md5_hash
    md5_hash=$(firmware_md5 "$firmware_bin") || return 1

    local attempt base_url
    for attempt in 1 2 3; do
        base_url=$(ota_base_url)
        print_info "Starting OTA session at $base_url (attempt $attempt/3)"
        if ! ota_start_session "$base_url"; then
            print_info "OTA start request failed on attempt $attempt"
            cat /tmp/ota_start.log 2>/dev/null || true
        else
            print_info "Uploading $(basename "$firmware_bin") via OTA..."
            if ota_send_firmware "$base_url" "$firmware_bin"; then
                print_success "OTA upload accepted by device"
                print_info "Device should reboot after the delayed restart window"
                return 0
            fi

            print_info "OTA upload failed on attempt $attempt"
            cat /tmp/ota_upload.log 2>/dev/null || true
        fi

        if [[ $attempt -lt 3 ]]; then
            print_info "Retrying OTA"
        fi
    done

    print_error "OTA upload failed after 3 attempts"
    return 1
}

ota_curl() {
    local -a address_args=()
    # Avoid dual-stack mDNS lookup timeouts with macOS curl.
    case "$OTA_HOST" in
        *.[lL][oO][cC][aA][lL]|*.[lL][oO][cC][aA][lL].) address_args=(-4) ;;
    esac
    printf 'header = "Authorization: Bearer %s"\n' "$token" |
        curl -q --config - --noproxy '*' ${address_args[@]+"${address_args[@]}"} "$@"
}

monitor_serial() {
    local duration="${1:-300}"
    local sketch_name="$2"

    resolve_serial_port

    print_info "Monitoring serial output for ${duration}s (Ctrl+C to stop)..."
    echo -e "${YELLOW}Press Ctrl+C to stop monitoring${NC}"
    echo ""

    pkill -f "arduino-cli monitor -p $SERIAL_PORT" >/dev/null 2>&1 || true

    local exit_code=0
    timeout --foreground -s SIGINT "$duration" arduino-cli monitor -p "$SERIAL_PORT" -c baudrate="$BAUD_RATE" || exit_code=$?
    pkill -f "arduino-cli monitor -p $SERIAL_PORT" >/dev/null 2>&1 || true

    echo ""
    if [ $exit_code -eq 0 ]; then
        print_success "Monitoring complete"
    elif [ $exit_code -eq 130 ] || [ $exit_code -eq 2 ]; then
        print_info "Monitoring stopped by user"
    elif [ $exit_code -eq 124 ]; then
        print_success "Monitoring timeout reached (${duration}s)"
    else
        print_error "Monitoring ended with exit code $exit_code"
    fi
}

show_port_status() {
    resolve_serial_port

    print_header "Serial Port Status"
    print_info "Target board: $TARGET_BOARD ($BOARD_FQBN)"
    print_info "Detected serial port: $SERIAL_PORT"

    local lsof_output
    lsof_output=$(lsof "$SERIAL_PORT" 2>/dev/null || true)
    if [[ -n "$lsof_output" ]]; then
        echo "$lsof_output"
    else
        print_success "No process currently owns $SERIAL_PORT"
    fi

    local monitor_pids
    monitor_pids=$(pgrep -f "arduino-cli monitor -p $SERIAL_PORT|serial-monitor $SERIAL_PORT" 2>/dev/null || true)
    if [[ -n "$monitor_pids" ]]; then
        print_info "Matching monitor processes:"
        ps -p "$(echo "$monitor_pids" | paste -sd, -)" -o pid=,stat=,command=
    fi
}

list_tests() {
    print_header "Available Targets"
    echo "Canonical entrypoints:"
    echo "  ./tools/firmware.sh build --board jc4827w543c"
    echo "  ./tools/tests.sh compile safety --board jc4827w543c"
    echo "  ./tools/firmware.sh port --board jc4827w543c"
    echo ""
    echo "Legacy numeric targets:"
    echo "  1. main          - Main roaster firmware"
    echo "  2. pid           - PID controller tests"
    echo "  3. profiles      - Profile management tests"
    echo "  4. state         - State machine tests"
    echo "  5. safety        - Safety system tests"
    echo "  6. unit          - AUnit framework demo tests"
    echo "  7. hardware      - Hardware validation"
    echo "  8. step_response - Step response tuner tests"
    echo ""
    echo "Legacy usage: $CLI_NAME [1-8] [compile|upload|monitor|ota|port|all]"
    echo "Example: $CLI_NAME 2 all"
    echo "Example: OTA_HOST=roaster-dev.local $CLI_NAME 1 ota"
}

get_sketch_path() {
    case "$1" in
        1|main)
            echo "$FIRMWARE_DIR/roaster-firmware.ino"
            echo "main"
            ;;
        2|pid)
            echo "$TESTS_DIR/test_pid/test_pid.ino"
            echo "PID"
            ;;
        3|profiles)
            echo "$TESTS_DIR/test_profiles/test_profiles.ino"
            echo "Profiles"
            ;;
        4|state|state_machine)
            echo "$TESTS_DIR/test_state_machine/test_state_machine.ino"
            echo "State Machine"
            ;;
        5|safety)
            echo "$TESTS_DIR/test_safety/test_safety.ino"
            echo "Safety"
            ;;
        6|unit)
            echo "$TESTS_DIR/unit_tests/unit_tests.ino"
            echo "Unit Tests"
            ;;
        7|hardware)
            echo "$TESTS_DIR/hardware_validation/hardware_validation.ino"
            echo "Hardware Validation"
            ;;
        8|step_response)
            echo "$TESTS_DIR/test_step_response/test_step_response.ino"
            echo "Step Response"
            ;;
        *)
            echo "INVALID"
            echo "Invalid test"
            ;;
    esac
}

main() {
    if ! command -v arduino-cli &> /dev/null; then
        print_error "arduino-cli not found. Install with: brew install arduino-cli"
        exit 1
    fi

    if [ $# -eq 0 ]; then
        list_tests
        exit 0
    fi

    test_num="$1"
    action="${2:-all}"

    local output
    output=$(get_sketch_path "$test_num")
    sketch_path=$(echo "$output" | head -n1)
    test_name=$(echo "$output" | tail -n1)

    if [ "$sketch_path" = "INVALID" ]; then
        print_error "Invalid test selection: $test_num"
        list_tests
        exit 1
    fi

    if [ ! -f "$sketch_path" ]; then
        print_error "Sketch file not found: $sketch_path"
        exit 1
    fi

    case "$action" in
        pair)
            python3 "$TOOLS_DIR/roaster-pair.py" "${OTA_SCHEME}://${OTA_HOST}:${OTA_PORT}"
            ;;
        compile)
            print_header "Compiling $test_name"
            compile_sketch "$sketch_path" || {
                print_error "Compilation failed - aborting"
                exit 1
            }
            ;;
        upload)
            print_header "Uploading $test_name"
            ensure_firmware_build "$sketch_path" usb && upload_sketch "$sketch_path" || {
                print_error "Build/upload failed - aborting"
                exit 1
            }
            ;;
        monitor)
            print_header "Monitoring $test_name"
            resolve_serial_port
            monitor_serial 300 "$test_name"
            ;;
        port)
            show_port_status
            ;;
        ota)
            if [[ "$test_num" != 1 && "$test_num" != main ]]; then
                print_error "OTA upload is only supported for the main firmware"
                exit 1
            fi
            print_header "OTA Uploading $test_name"
            ota_upload_firmware "$sketch_path"
            ;;
        all)
            print_header "Building & Running $test_name"
            compile_sketch "$sketch_path" && \
            upload_sketch "$sketch_path" && \
            monitor_serial 300 "$test_name"
            ;;
        *)
            print_error "Invalid action: $action. Use: compile, upload, ota, monitor, port, or all"
            exit 1
            ;;
    esac
}

main "$@"