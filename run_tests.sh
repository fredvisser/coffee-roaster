#!/bin/bash

# Coffee Roaster Test Runner Script
# Runs all unit tests and generates a summary report

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
FQBN="esp32:esp32:nano_nora"
PORT="/dev/cu.usbserial-*"
TEST_DIR="roaster-firmware/tests"

# Test files
TESTS=(
    "test_profiles/test_profiles.ino"
    "test_pid/test_pid.ino"
    "test_state_machine/test_state_machine.ino"
    "test_safety/test_safety.ino"
)

echo -e "${BLUE}╔═══════════════════════════════════════════════════════════╗${NC}"
echo -e "${BLUE}║       Coffee Roaster - Automated Test Runner             ║${NC}"
echo -e "${BLUE}╚═══════════════════════════════════════════════════════════╝${NC}"
echo ""

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo -e "${RED}ERROR: arduino-cli is not installed${NC}"
    echo "Install with: brew install arduino-cli"
    exit 1
fi

# Check if AUnit is installed
echo -e "${YELLOW}Checking for AUnit library...${NC}"
if ! arduino-cli lib list | grep -q "AUnit"; then
    echo -e "${YELLOW}Installing AUnit...${NC}"
    arduino-cli lib install "AUnit"
fi

# Compile all tests
echo -e "${BLUE}Compiling all test suites...${NC}"
echo ""

COMPILE_SUCCESS=0
COMPILE_FAIL=0

for test in "${TESTS[@]}"; do
    echo -ne "${YELLOW}Compiling ${test}...${NC} "
    if arduino-cli compile --fqbn "$FQBN" "$TEST_DIR/$test" --quiet 2>/dev/null; then
        echo -e "${GREEN}✓ PASS${NC}"
        ((COMPILE_SUCCESS++))
    else
        echo -e "${RED}✗ FAIL${NC}"
        ((COMPILE_FAIL++))
    fi
done

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${BLUE}Compilation Summary${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "Tests Compiled: ${GREEN}$COMPILE_SUCCESS${NC}"
echo -e "Failed:         ${RED}$COMPILE_FAIL${NC}"
echo ""

if [ $COMPILE_FAIL -gt 0 ]; then
    echo -e "${RED}Some tests failed to compile. Fix errors before running.${NC}"
    exit 1
fi

echo -e "${GREEN}All tests compiled successfully!${NC}"
echo ""

# Option to upload and run
echo -e "${YELLOW}Do you want to upload and run tests on hardware? (y/N)${NC}"
read -r response

if [[ "$response" =~ ^([yY][eE][sS]|[yY])$ ]]; then
    # Try to detect port
    echo -e "${YELLOW}Detecting connected board...${NC}"
    arduino-cli board list
    
    echo ""
    echo -e "${YELLOW}Enter port (or press Enter to skip hardware testing):${NC}"
    read -r user_port
    
    if [ -n "$user_port" ]; then
        PORT="$user_port"
        
        for test in "${TESTS[@]}"; do
            echo ""
            echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
            echo -e "${BLUE}Running: $test${NC}"
            echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
            
            echo -e "${YELLOW}Uploading...${NC}"
            if arduino-cli upload -p "$PORT" --fqbn "$FQBN" "$TEST_DIR/$test"; then
                echo -e "${GREEN}Upload successful${NC}"
                echo ""
                echo -e "${YELLOW}Test output (press Ctrl+C to stop and continue to next test):${NC}"
                echo ""
                
                # Monitor for 60 seconds or until Ctrl+C
                timeout 60 arduino-cli monitor -p "$PORT" -c baudrate=115200 || true
                
                echo ""
                echo -e "${YELLOW}Moving to next test...${NC}"
                sleep 2
            else
                echo -e "${RED}Upload failed${NC}"
            fi
        done
        
        echo ""
        echo -e "${GREEN}All tests completed!${NC}"
    fi
else
    echo -e "${YELLOW}Skipping hardware testing.${NC}"
fi

echo ""
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo -e "${GREEN}Test run complete!${NC}"
echo -e "${BLUE}═══════════════════════════════════════════════════════════${NC}"
echo ""
echo "To run individual tests manually:"
echo "  arduino-cli compile --fqbn $FQBN $TEST_DIR/test_profiles/test_profiles.ino"
echo "  arduino-cli upload -p PORT --fqbn $FQBN $TEST_DIR/test_profiles/test_profiles.ino"
echo "  arduino-cli monitor -p PORT -c baudrate=115200"
echo ""
