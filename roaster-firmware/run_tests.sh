#!/bin/bash
# Coffee Roaster - Test Build & Upload Script
# Compiles, uploads, and monitors tests on ESP32

set -e

# Configuration
BOARD_FQBN="esp32:esp32:nano_nora"
SERIAL_PORT="${SERIAL_PORT:-auto}"
BAUD_RATE=115200
OTA_SCHEME="${OTA_SCHEME:-http}"
OTA_HOST="${OTA_HOST:-roaster.local}"
OTA_PORT="${OTA_PORT:-80}"
OTA_USER="${OTA_USER:-}"
OTA_PASS="${OTA_PASS:-}"
OTA_RESOLVED_HOST=""
OTA_CACHE_FILE="${OTA_CACHE_FILE:-${TMPDIR:-/tmp}/coffee-roaster-ota-host.cache}"
FIRMWARE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TESTS_DIR="$FIRMWARE_DIR/tests"

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
    
    print_info "Compiling $sketch_name..."
    
    if arduino-cli compile --fqbn "$BOARD_FQBN" "$sketch_path" 2>&1 | tee /tmp/compile.log; then
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
        cp "$bin_file" "$build_dir/$sketch_name.bin"
        print_success "Build artifact copied to: $build_dir/$sketch_name.bin"
        local size=$(ls -lh "$build_dir/$sketch_name.bin" | awk '{print $5}')
        print_info "Firmware size: $size"
    else
        print_info "No .bin file found in build cache (this is normal for test sketches)"
    fi
}

upload_sketch() {
    local sketch_path="$1"
    local sketch_name=$(basename "$sketch_path" .ino)
    
    resolve_serial_port
    
    print_info "Uploading $sketch_name to $SERIAL_PORT..."
    
    if arduino-cli upload -p "$SERIAL_PORT" --fqbn "$BOARD_FQBN" "$sketch_path" 2>&1 | tee /tmp/upload.log; then
        print_success "$sketch_name uploaded successfully"
        sleep 2  # Wait for board to reboot
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
    local duration="${1:-300}"  # Default 300 seconds (5 min)
    local sketch_name="$2"
    
    resolve_serial_port
    
    print_info "Monitoring serial output for ${duration}s (Ctrl+C to stop)..."
    echo -e "${YELLOW}Press Ctrl+C to stop monitoring${NC}"
    echo ""
    
    # Use timeout with --foreground flag to allow TTY signals (Ctrl+C) to work
    # -f/--foreground: Allow COMMAND to read from TTY and receive TTY signals
    # -s SIGINT: Send SIGINT (Ctrl+C signal) on timeout instead of SIGTERM
    local exit_code=0
    timeout --foreground -s SIGINT "$duration" arduino-cli monitor -p "$SERIAL_PORT" -c baudrate="$BAUD_RATE" || exit_code=$?
    
    echo ""
    if [ $exit_code -eq 0 ]; then
        print_success "Monitoring complete"
    elif [ $exit_code -eq 130 ] || [ $exit_code -eq 2 ]; then
        # 130 = SIGINT (Ctrl+C), 2 = keyboard interrupt
        print_info "Monitoring stopped by user"
    elif [ $exit_code -eq 124 ]; then
        # 124 = timeout reached
        print_success "Monitoring timeout reached (${duration}s)"
    else
        print_error "Monitoring ended with exit code $exit_code"
    fi
}

list_tests() {
    print_header "Available Tests"
    echo "Firmware:"
    echo "  1. main     - Main roaster firmware"
    echo ""
    echo "Unit Tests:"
    echo "  2. pid      - PID controller tests"
    echo "  3. profiles - Profile management tests"
    echo "  4. state    - State machine tests"
    echo "  5. safety   - Safety system tests"
    echo "  6. unit     - AUnit framework demo tests"
    echo ""
    echo "Hardware Tests:"
    echo "  7. hardware - Hardware validation (requires real hardware)"
    echo ""
    echo "Usage: $0 [1-8] [compile|upload|monitor|all]"
    echo "Example: $0 2 all     # Compile, upload, and monitor PID tests"
    echo "Example: $0 1 compile # Just compile main firmware"
    echo "Example: OTA_HOST=roaster.local $0 1 ota        # Compile and upload main firmware over ElegantOTA"
    echo ""
    echo "To run all unit tests (2-5,8): for i in 2 3 4 5 8; do $0 \$i all; done"
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

# Main script
main() {
    # Check if arduino-cli is installed
    if ! command -v arduino-cli &> /dev/null; then
        print_error "arduino-cli not found. Install with: brew install arduino-cli"
        exit 1
    fi
    
    # Show help if no arguments
    if [ $# -eq 0 ]; then
        list_tests
        exit 0
    fi
    
    # Get test selection
    test_num="$1"
    action="${2:-all}"  # Default to "all" if not specified
    
    # Get sketch path and name (handle paths with spaces)
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
    
    # Execute requested actions
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
            monitor_serial 300 "$test_name"  # 5 minute default for tests
            ;;
        ota)
            print_header "Building & OTA Uploading $test_name"
            compile_sketch "$sketch_path" && \
            ota_upload_firmware "$sketch_path"
            ;;
        all)
            print_header "Building & Running $test_name"
            compile_sketch "$sketch_path" && \
            upload_sketch "$sketch_path" && \
            monitor_serial 300 "$test_name"
            ;;
        *)
            print_error "Invalid action: $action. Use: compile, upload, ota, monitor, or all"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
