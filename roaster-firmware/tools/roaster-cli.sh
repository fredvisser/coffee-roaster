#!/bin/bash
# Coffee Roaster - Legacy compile/upload runner
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
OTA_USER="${OTA_USER:-}"
OTA_PASS="${OTA_PASS:-}"
OTA_RESOLVED_HOST=""
OTA_CACHE_FILE="${OTA_CACHE_FILE:-${TMPDIR:-/tmp}/coffee-roaster-ota-host.cache}"
TOOLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(cd "$TOOLS_DIR/.." && pwd)"
TESTS_DIR="$FIRMWARE_DIR/tests"
CLI_NAME="${ROASTER_CLI_NAME:-$0}"

BUILD_EXTRA_FLAGS="-DROASTER_TARGET_BOARD=ROASTER_BOARD_JC4827W543C -DROASTER_DISPLAY_BACKEND=ROASTER_DISPLAY_BACKEND_LVGL"

case "$TARGET_BOARD" in
    jc4827w543c)
        BUILD_EXTRA_FLAGS="-DROASTER_TARGET_BOARD=ROASTER_BOARD_JC4827W543C -DROASTER_DISPLAY_BACKEND=ROASTER_DISPLAY_BACKEND_LVGL"
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
        local build_version="${ROASTER_BUILD_VERSION:-$(date +%F)}"
        all_extra_flags="${all_extra_flags:+$all_extra_flags }-DVERSION=\"${build_version}\""
        print_info "Firmware version: $build_version"
    fi

    local build_args=(--fqbn "$BOARD_FQBN")
    if [[ -n "$all_extra_flags" ]]; then
        build_args+=(--build-property "compiler.cpp.extra_flags=$all_extra_flags")
        build_args+=(--build-property "compiler.c.extra_flags=$all_extra_flags")
    fi

    if arduino-cli compile "${build_args[@]}" "$sketch_path" 2>&1 | tee /tmp/compile.log; then
        print_success "$sketch_name compiled successfully"
        copy_build_artifacts "$sketch_path" "$sketch_name"
        return 0
    else
        print_error "$sketch_name compilation failed"
        tail -20 /tmp/compile.log
        return 1
    fi
}

copy_build_artifacts() {
    local sketch_path="$1"
    local sketch_name="$2"

    # Only copy artifacts for main firmware, not tests
    if [[ "$sketch_name" != "roaster-firmware" ]]; then
        return 0
    fi

    local sketch_dir=$(dirname "$sketch_path")
    local build_dir="$sketch_dir/build"

    # Create build directory if it doesn't exist
    mkdir -p "$build_dir"

    # Find the most recently compiled .bin file in arduino cache
    # arduino-cli stores builds in ~/Library/Caches/arduino/sketches/<HASH>/
    local cache_dir="$HOME/Library/Caches/arduino/sketches"
    local bin_file=$(find "$cache_dir" -name "$sketch_name.ino.bin" -type f -mmin -5 2>/dev/null | head -1)

    if [ -n "$bin_file" ] && [ -f "$bin_file" ]; then
        local cache_build_dir
        cache_build_dir=$(dirname "$bin_file")

        cp "$bin_file" "$build_dir/$sketch_name.bin"
        print_success "Build artifact copied to: $build_dir/$sketch_name.bin"
        local size=$(ls -lh "$build_dir/$sketch_name.bin" | awk '{print $5}')
        print_info "Firmware size: $size"

        if [[ -f "$cache_build_dir/$sketch_name.ino.bootloader.bin" ]]; then
            cp "$cache_build_dir/$sketch_name.ino.bootloader.bin" "$build_dir/$sketch_name.bootloader.bin"
        fi

        if [[ -f "$cache_build_dir/partitions.csv" ]]; then
            cp "$cache_build_dir/partitions.csv" "$build_dir/partitions.csv"
        fi

        if [[ -f "$cache_build_dir/build.options.json" ]]; then
            cp "$cache_build_dir/build.options.json" "$build_dir/build.options.json"
        fi
    else
        print_info "No .bin file found in build cache (this is normal for test sketches)"
    fi
}

resolve_build_cache_dir() {
    local sketch_name="$1"
    local cache_dir="$HOME/Library/Caches/arduino/sketches"
    local artifact

    artifact=$(find "$cache_dir" \( -name "$sketch_name.ino.bin" -o -name "$sketch_name.ino.bootloader.bin" \) -type f -mmin -30 2>/dev/null | head -1)
    if [[ -n "$artifact" ]] && [[ -f "$artifact" ]]; then
        dirname "$artifact"
        return 0
    fi

    return 1
}

flash_jc_main_firmware() {
    local sketch_name="$1"
    local sketch_dir="$FIRMWARE_DIR/build"
    local app_bin="$sketch_dir/$sketch_name.bin"
    local bootloader_bin="$sketch_dir/$sketch_name.bootloader.bin"
    local partitions_csv="$sketch_dir/partitions.csv"
    local cache_build_dir=""

    if [[ ! -f "$app_bin" ]] || [[ ! -f "$bootloader_bin" ]] || [[ ! -f "$partitions_csv" ]]; then
        cache_build_dir=$(resolve_build_cache_dir "$sketch_name") || {
            print_error "Build artifacts not found for $sketch_name. Run compile first."
            return 1
        }

        [[ -f "$app_bin" ]] || app_bin="$cache_build_dir/$sketch_name.ino.bin"
        [[ -f "$bootloader_bin" ]] || bootloader_bin="$cache_build_dir/$sketch_name.ino.bootloader.bin"
        [[ -f "$partitions_csv" ]] || partitions_csv="$cache_build_dir/partitions.csv"
    fi

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

    if [[ "$TARGET_BOARD" == "jc4827w543c" ]] && [[ "$sketch_name" == "roaster-firmware" ]]; then
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
    local host="${OTA_RESOLVED_HOST:-$OTA_HOST}"
    echo "${OTA_SCHEME}://${host}:${OTA_PORT}"
}

ota_python() {
    if command -v python3 >/dev/null 2>&1; then
        echo "python3"
        return 0
    fi

    if command -v python >/dev/null 2>&1; then
        echo "python"
        return 0
    fi

    return 1
}

ota_cache_host() {
    local host="$1"
    if [[ "$host" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
        printf '%s\n' "$host" > "$OTA_CACHE_FILE"
    fi
}

ota_load_cached_host() {
    if [[ ! -f "$OTA_CACHE_FILE" ]]; then
        return 1
    fi

    local cached_host
    cached_host=$(tr -d '[:space:]' < "$OTA_CACHE_FILE")
    if [[ "$cached_host" =~ ^([0-9]{1,3}\.){3}[0-9]{1,3}$ ]]; then
        OTA_RESOLVED_HOST="$cached_host"
        return 0
    fi

    return 1
}

ota_probe_host() {
    local host="$1"
    local base_url="${OTA_SCHEME}://${host}:${OTA_PORT}"

    curl --silent --show-error --connect-timeout 3 --max-time 5 \
        --output /dev/null "$base_url/api/state"
}

resolve_ota_host_once() {
    local python_bin
    python_bin=$(ota_python) || return 1

    "$python_bin" - "$OTA_HOST" <<'PY'
import subprocess
import sys
import time

host = sys.argv[1]
try:
    proc = subprocess.Popen(
        ['dns-sd', '-G', 'v4', host],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
    )
except Exception:
    print('')
    raise SystemExit(0)

deadline = time.time() + 5.0
resolved = ''
try:
    while time.time() < deadline:
        line = proc.stdout.readline()
        if not line:
            time.sleep(0.1)
            continue
        parts = line.split()
        if len(parts) >= 6 and parts[1] == 'Add':
            resolved = parts[5]
            break
finally:
    proc.terminate()
    try:
        proc.wait(timeout=1)
    except Exception:
        proc.kill()

print(resolved)
PY
}

resolve_ota_host() {
    OTA_RESOLVED_HOST="$OTA_HOST"

    if [[ "$OTA_HOST" != *.local ]]; then
        return 0
    fi

    if command -v dns-sd >/dev/null 2>&1; then
        local attempt resolved
        for attempt in 1 2 3; do
            resolved=$(resolve_ota_host_once)
            if [[ -n "$resolved" ]] && ota_probe_host "$resolved"; then
                OTA_RESOLVED_HOST="$resolved"
                ota_cache_host "$OTA_RESOLVED_HOST"
                print_info "Resolved $OTA_HOST to $OTA_RESOLVED_HOST for OTA upload"
                return 0
            fi
            print_info "mDNS resolution attempt $attempt for $OTA_HOST did not produce a reachable host"
        done
    fi

    if ota_probe_host "$OTA_HOST"; then
        OTA_RESOLVED_HOST="$OTA_HOST"
        print_info "Using directly reachable host $OTA_HOST for OTA upload"
        return 0
    fi

    if ota_load_cached_host && ota_probe_host "$OTA_RESOLVED_HOST"; then
        print_info "Using cached OTA host $OTA_RESOLVED_HOST after mDNS lookup failed"
        return 0
    fi

    print_error "Unable to resolve a reachable OTA host for $OTA_HOST"
    return 1
}

ota_start_session() {
    local base_url="$1"
    shift

    curl --fail --silent --show-error --http1.1 --connect-timeout 5 --max-time 20 \
        "$@" "$base_url/ota/start?mode=fw&hash=$md5_hash" >/tmp/ota_start.log
}

ota_send_firmware() {
    local base_url="$1"
    local firmware_bin="$2"
    shift 2

    curl --fail --silent --show-error --http1.1 --connect-timeout 10 --max-time 180 \
        "$@" -F "file=@$firmware_bin;type=application/octet-stream" \
        "$base_url/ota/upload" >/tmp/ota_upload.log
}

resolve_firmware_bin() {
    local build_bin="$FIRMWARE_DIR/build/roaster-firmware.bin"
    if [[ -f "$build_bin" ]]; then
        echo "$build_bin"
        return 0
    fi

    local cache_dir="$HOME/Library/Caches/arduino/sketches"
    local bin_file
    bin_file=$(find "$cache_dir" -name "roaster-firmware.ino.bin" -type f -mmin -10 2>/dev/null | head -1)
    if [[ -n "$bin_file" ]] && [[ -f "$bin_file" ]]; then
        echo "$bin_file"
        return 0
    fi

    return 1
}

resolve_firmware_metadata() {
    local build_metadata="$FIRMWARE_DIR/build/build.options.json"
    if [[ -f "$build_metadata" ]]; then
        echo "$build_metadata"
        return 0
    fi

    local cache_dir="$HOME/Library/Caches/arduino/sketches"
    local metadata_file
    metadata_file=$(find "$cache_dir" -name build.options.json -type f -mmin -30 2>/dev/null | head -1)
    if [[ -n "$metadata_file" ]] && [[ -f "$metadata_file" ]]; then
        echo "$metadata_file"
        return 0
    fi

    return 1
}

firmware_artifact_matches_target() {
    local firmware_bin="$1"
    local metadata_file
    metadata_file=$(resolve_firmware_metadata) || return 1

    if [[ ! -f "$firmware_bin" ]]; then
        return 1
    fi

    if ! grep -Fq '"fqbn":'" \"$BOARD_FQBN\"" "$metadata_file"; then
        return 1
    fi

    return 0
}

ensure_firmware_build() {
    local sketch_path="$1"
    local sketch_name=$(basename "$sketch_path" .ino)

    if [[ "${ROASTER_FORCE_REBUILD:-0}" == "1" ]]; then
        print_info "Forced rebuild requested for OTA upload"
        compile_sketch "$sketch_path"
        return $?
    fi

    local firmware_bin
    firmware_bin=$(resolve_firmware_bin) || {
        print_info "No existing firmware binary found; compiling before OTA upload"
        compile_sketch "$sketch_path"
        return $?
    }

    if firmware_artifact_matches_target "$firmware_bin"; then
        print_info "Reusing existing firmware build for OTA upload"
        return 0
    fi

    print_info "Existing firmware build does not match $TARGET_BOARD; compiling before OTA upload"
    compile_sketch "$sketch_path"
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
    local sketch_name=$(basename "$sketch_path" .ino)

    if [[ "$sketch_name" != "roaster-firmware" ]]; then
        print_error "OTA upload is only supported for the main firmware"
        return 1
    fi

    local firmware_bin
    firmware_bin=$(resolve_firmware_bin) || {
        print_error "Firmware binary not found. Run compile first."
        return 1
    }

    local md5_hash
    md5_hash=$(firmware_md5 "$firmware_bin") || return 1
    resolve_ota_host || return 1

    local auth_args=()
    if [[ -n "$OTA_USER" ]] || [[ -n "$OTA_PASS" ]]; then
        auth_args=(-u "$OTA_USER:$OTA_PASS")
    fi

    local attempt base_url
    for attempt in 1 2 3; do
        base_url=$(ota_base_url)
        print_info "Starting OTA session at $base_url (attempt $attempt/3)"
        if ! ota_start_session "$base_url" "${auth_args[@]}"; then
            print_info "OTA start request failed on attempt $attempt"
            cat /tmp/ota_start.log 2>/dev/null || true
        else
            print_info "Uploading $(basename "$firmware_bin") via OTA..."
            if ota_send_firmware "$base_url" "$firmware_bin" "${auth_args[@]}"; then
                print_success "OTA upload accepted by device"
                print_info "Device should reboot after the delayed restart window"
                ota_cache_host "${OTA_RESOLVED_HOST:-$OTA_HOST}"
                return 0
            fi

            print_info "OTA upload failed on attempt $attempt"
            cat /tmp/ota_upload.log 2>/dev/null || true
        fi

        if [[ $attempt -lt 3 ]]; then
            print_info "Retrying OTA after refreshing host resolution"
            resolve_ota_host || true
        fi
    done

    print_error "OTA upload failed after 3 attempts"
    return 1
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
        compile)
            print_header "Compiling $test_name"
            compile_sketch "$sketch_path" || {
                print_error "Compilation failed - aborting"
                exit 1
            }
            ;;
        upload)
            print_header "Uploading $test_name"
            compile_sketch "$sketch_path" && upload_sketch "$sketch_path" || {
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
            print_header "Building & OTA Uploading $test_name"
            ensure_firmware_build "$sketch_path" && \
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