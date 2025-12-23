#!/bin/bash
# Coffee Roaster - Test Build & Upload Script
# Compiles, uploads, and monitors tests on ESP32

set -e

# Configuration
BOARD_FQBN="esp32:esp32:nano_nora"
SERIAL_PORT="${SERIAL_PORT:-auto}"
BAUD_RATE=115200
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$PROJECT_DIR/roaster-firmware"
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
        return 0
    else
        print_error "$sketch_name compilation failed"
        tail -20 /tmp/compile.log
        return 1
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
    echo "Usage: $0 [1-7] [compile|upload|monitor|all]"
    echo "Example: $0 2 all     # Compile, upload, and monitor PID tests"
    echo "Example: $0 1 compile # Just compile main firmware"
    echo ""
    echo "To run all unit tests (2-5): for i in {2..5}; do $0 \$i all; done"
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
    
    # Get sketch path and name
    read sketch_path test_name <<< "$(get_sketch_path "$test_num")"
    
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
            compile_sketch "$sketch_path"
            ;;
        upload)
            print_header "Uploading $test_name"
            compile_sketch "$sketch_path" && upload_sketch "$sketch_path"
            ;;
        monitor)
            print_header "Monitoring $test_name"
            resolve_serial_port
            monitor_serial 300 "$test_name"  # 5 minute default for tests
            ;;
        all)
            print_header "Building & Running $test_name"
            compile_sketch "$sketch_path" && \
            upload_sketch "$sketch_path" && \
            monitor_serial 300 "$test_name"
            ;;
        *)
            print_error "Invalid action: $action. Use: compile, upload, monitor, or all"
            exit 1
            ;;
    esac
}

# Run main function
main "$@"
