# Agent Guidelines for Coffee Roaster Project

## Project Overview

This is an ESP32-based coffee roaster control system with:
- Temperature monitoring via MAX6675 thermocouples
- PID-controlled heating element
- Variable speed fan control (PWM and BDC fans)
- Nextion HMI display interface
- WiFi connectivity with web interface
- Customizable roast profiles stored in preferences

## Development Workflow

### Prerequisites

- **arduino-cli**: Primary build and deployment tool
- ESP32 board support package
- Required libraries (see Dependencies section)

### Using arduino-cli

#### Installation

```bash
# Install arduino-cli
brew install arduino-cli  # macOS
# or download from https://arduino.github.io/arduino-cli/

# Initialize configuration
arduino-cli config init

# Add ESP32 board support
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json

# Update board index
arduino-cli core update-index

# Install ESP32 core
arduino-cli core install esp32:esp32
```

#### Required Libraries

Install these dependencies:

```bash
arduino-cli lib install "EasyNextionLibrary"
arduino-cli lib install "MAX6675 library"
arduino-cli lib install "SimpleTimer"
arduino-cli lib install "AutoPID"
arduino-cli lib install "ESP32Servo"
arduino-cli lib install "ElegantOTA@3.1.7"

# Note: PWMrelay and custom headers (Profiles.hpp, Network.hpp) are project-specific
```

**Important**: After installing ElegantOTA, you must enable async mode:
1. Open `/Users/fred/Documents/Arduino/libraries/ElegantOTA/src/ElegantOTA.h`
2. Change `#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0` to `1`
3. This enables compatibility with ESPAsyncWebServer

See `OTA_SETUP.md` for complete OTA configuration details.

#### Building the Firmware

```bash
# Navigate to firmware directory
cd roaster-firmware

# Compile for ESP32 Nano (or your specific board)
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino

# Build with verbose output for debugging
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino --verbose
```

#### Deploying to Hardware

```bash
# List connected boards
arduino-cli board list

# Upload to connected ESP32
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora roaster-firmware.ino

# Monitor serial output
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

#### Debug Mode

Enable debug output by uncommenting `#define DEBUG` in the main `.ino` file, then rebuild.

## Testing Strategy

### Unit Tests

Unit tests should validate individual components in isolation:

#### Test Categories

1. **Profile Management Tests** (`test_profiles.cpp`)
   - Setpoint interpolation accuracy
   - Profile serialization/deserialization
   - Boundary conditions (time/temp limits)
   - Profile state transitions

2. **PID Controller Tests** (`test_pid.cpp`)
   - Temperature tracking accuracy
   - Response to setpoint changes
   - Output clamping (0-255 range)
   - Stability under various loads

3. **State Machine Tests** (`test_state_machine.cpp`)
   - State transitions (IDLE → START_ROAST → ROASTING → COOLING → IDLE)
   - Safety interlocks
   - Error handling
   - Emergency stop behavior

4. **Network Communication Tests** (`test_network.cpp`)
   - WiFi connection/reconnection
   - WebSocket message handling
   - Command parsing
   - Data serialization

#### Running Unit Tests

```bash
# Install AUnit testing framework
arduino-cli lib install "AUnit"

# Using arduino-cli with test framework
cd roaster-firmware/tests

# Run individual test suites
arduino-cli compile --fqbn esp32:esp32:nano_nora test_profiles.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora test_profiles.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200

# Run all tests
arduino-cli compile --fqbn esp32:esp32:nano_nora unit_tests.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora unit_tests.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

**Test Suites Available:**
- `test_profiles.ino` - Profile management tests (20+ tests)
- `test_pid.ino` - PID controller tests (20+ tests)
- `test_state_machine.ino` - State machine tests (30+ tests)
- `test_safety.ino` - Safety system tests (25+ CRITICAL tests)
- `unit_tests.ino` - Main test runner
- `hardware_validation.ino` - Hardware-in-the-loop tests (15+ tests)

**Test Framework**: [AUnit](https://github.com/bxparks/AUnit) - Arduino unit testing framework

### Hardware Validation Tests

Hardware-in-the-loop tests verify physical integration:

#### Safety Tests (CRITICAL)

1. **Thermal Runaway Protection**
   - Verify heater cuts off if temp exceeds safe limits
   - Test thermocouple disconnect detection
   - Validate emergency stop functionality

2. **Over-Temperature Protection**
   - Ensure fan activates at high temperatures
   - Test cooling mode engagement
   - Verify heater lockout at danger temps

3. **Sensor Failure Detection**
   - Simulate open thermocouple (reads 500°F+)
   - Test graceful degradation
   - Validate error state handling

#### Functional Tests

1. **Temperature Control Accuracy**
   - Measure PID tracking error (±5°F target)
   - Verify setpoint interpolation during roast
   - Test response time to setpoint changes

2. **Fan Control Validation**
   - PWM fan output verification (0-255 range)
   - BDC fan servo control (800-2000 µs)
   - Fan speed transitions during roast profile

3. **Profile Execution**
   - Complete roast cycle test
   - Verify timing accuracy
   - Confirm profile persistence across resets

4. **UI Integration**
   - Nextion display updates
   - Button response times
   - Real-time data display accuracy

#### Running Hardware Tests

⚠️ **SAFETY WARNING**: Hardware tests control real heating elements and fans!
- Never leave hardware unattended during testing
- Have fire extinguisher ready
- Monitor temperature continuously

```bash
# Flash hardware test firmware
cd roaster-firmware/tests
arduino-cli compile --fqbn esp32:esp32:nano_nora hardware_validation.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora hardware_validation.ino

# Run automated test suite with logging
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200 > test_results.log
```

**Hardware Tests Include:**
- Thermocouple reading verification (both sensors)
- Sensor consistency checks (10 samples)
- Heater control validation (PWM)
- PWM fan control (0-255 range)
- BDC fan servo control (800-2000µs)
- Fan cooling effectiveness measurement
- Emergency shutdown verification
- Over-temperature protection
- Short roast cycle simulation (heating to 250°F)

Tests automatically enforce safety limits and provide a 10-second countdown before starting.

### Integration Testing

Test complete workflows:
- Full roast cycle from start to cooling completion
- Network connectivity during roasting
- Profile saving/loading/execution
- Multi-sensor coordination

## GitHub Actions CI/CD

### Continuous Integration Pipeline

Create `.github/workflows/arduino-ci.yml`:

```yaml
name: Arduino CI

on:
  push:
    branches: [ main, add-bdc, develop ]
  pull_request:
    branches: [ main ]

jobs:
  build:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup Arduino CLI
      uses: arduino/setup-arduino-cli@v1
      
    - name: Install ESP32 platform
      run: |
        arduino-cli config init
        arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
        arduino-cli core update-index
        arduino-cli core install esp32:esp32@3.3.3
        
    - name: Install dependencies
      run: |
        arduino-cli lib install "EasyNextionLibrary"
        arduino-cli lib install "MAX6675 library"
        arduino-cli lib install "SimpleTimer"
        arduino-cli lib install "AutoPID"
        arduino-cli lib install "ESP32Servo"
        arduino-cli lib install "ElegantOTA@3.1.7"
        arduino-cli lib install "ArduinoJson"
        
    - name: Install AsyncWebServer libraries
      run: |
        cd ~/Arduino/libraries
        git clone https://github.com/ESP32Async/AsyncTCP.git
        git clone https://github.com/ESP32Async/ESPAsyncWebServer.git
        
    - name: Configure ElegantOTA for async mode
      run: |
        ELEGANTOTA_H=~/Arduino/libraries/ElegantOTA/src/ElegantOTA.h
        sed -i 's/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1/' "$ELEGANTOTA_H"
        echo "✓ ElegantOTA configured for async mode"
        grep "ELEGANTOTA_USE_ASYNC_WEBSERVER" "$ELEGANTOTA_H"
        
    - name: Compile firmware
      run: |
        cd roaster-firmware
        arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino
        
    - name: Run unit tests
      run: |
        cd roaster-firmware/tests
        # Compile all test suites
        for test_dir in test_profiles test_pid test_state_machine test_safety; do
          echo "Compiling $test_dir..."
          arduino-cli compile --fqbn esp32:esp32:nano_nora "$test_dir/$test_dir.ino"
        done
        
    - name: Archive build artifacts
      uses: actions/upload-artifact@v3
      with:
        name: firmware-build
        path: roaster-firmware/build/
        
  lint:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Lint Arduino code
      uses: arduino/arduino-lint-action@v1
      with:
        library-manager: update
        compliance: strict
```

### Hardware-in-the-Loop Testing (Future)

For hardware validation in CI:
- Use GitHub self-hosted runners with connected test hardware
- Implement automated test sequences
- Generate test reports with temperature/timing data
- Add safety monitoring and auto-abort conditions

### Release Pipeline

Create `.github/workflows/release.yml` for automated releases:

```yaml
name: Release Firmware

on:
  push:
    tags:
      - 'v*.*.*'

jobs:
  release:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    
    - name: Setup Arduino CLI
      uses: arduino/setup-arduino-cli@v1
      
    - name: Build release firmware
      run: |
        # ... install deps and compile ...
        
    - name: Create Release
      uses: softprops/action-gh-release@v1
      with:
        files: |
          roaster-firmware/build/**/*.bin
          roaster-firmware/build/**/*.elf
        generate_release_notes: true
```

## Code Quality Standards

### Arduino Best Practices

1. **Memory Management**
   - Minimize dynamic allocation (use stack/static when possible)
   - Monitor heap fragmentation
   - Use `F()` macro for string literals in flash memory

2. **Timing**
   - Use `millis()` for non-blocking delays
   - Avoid `delay()` in main loop (already done well in this project)
   - Keep interrupt handlers fast

3. **Safety**
   - Always initialize hardware to safe state in `setup()`
   - Implement watchdog timer for fault recovery
   - Add bounds checking on all user inputs
   - Validate sensor readings before use

4. **Code Organization**
   - Keep `.ino` file focused on high-level logic
   - Move complex functionality to `.hpp` files
   - Use meaningful variable names
   - Document safety-critical sections

### Pre-commit Checks

```bash
# Format check (consider clang-format with Arduino style)
# Compile check
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino

# Size check (ensure firmware fits in flash)
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino | grep "Sketch uses"
```

## Development Tips for AI Agents

### When Modifying Code

1. **Safety First**: Any changes affecting heater control, temperature limits, or fan control require extra scrutiny
2. **Test State Transitions**: Changes to the state machine need validation across all transitions
3. **Preserve Timing**: The SimpleTimer intervals (250ms, 5ms, 500ms) are carefully chosen
4. **Check Bounds**: Heater output (0-255), fan speed (0-255), BDC servo (800-2000µs)

### Understanding the Architecture

- **State Machine**: Manages roast lifecycle (see `RoasterState` enum)
- **PID Control**: AutoPID handles heater output based on temperature error
- **Profile System**: `Profiles.hpp` manages time-temperature-fan curves
- **Nextion Interface**: Bidirectional communication with HMI display
- **Network Layer**: `Network.hpp` handles WiFi and WebSocket for monitoring

### Common Tasks

**Adding a new roast profile parameter:**
1. Update `Profiles.hpp` data structure
2. Modify serialization in `flattenProfile()`/`unflattenProfile()`
3. Update Nextion UI to expose parameter
4. Add to `trigger0()` for reading from display
5. Test persistence across power cycles

**Adding a safety feature:**
1. Implement in state machine with highest priority
2. Add comprehensive testing
3. Document in safety section
4. Consider adding to ERROR state handling

**Integrating a new sensor:**
1. Add pin definition
2. Initialize in `setup()`
3. Read in `checkTempTimer` block
4. Add validation/filtering
5. Expose via network API for monitoring

## Resources

- [Arduino CLI Documentation](https://arduino.github.io/arduino-cli/)
- [ESP32 Arduino Core](https://github.com/espressif/arduino-esp32)
- [GitHub Actions for Arduino](https://github.com/arduino/setup-arduino-cli)
- [AUnit Testing Framework](https://github.com/bxparks/AUnit)
- [MAX6675 Datasheet](https://datasheets.maximintegrated.com/en/ds/MAX6675.pdf)

## Safety Reminder

**This device controls high temperatures and line voltage.** All code changes must be reviewed with safety in mind:
- ✅ Fail-safe defaults (heater OFF, fan ON)
- ✅ Temperature limits enforced
- ✅ Sensor validation
- ✅ Emergency stop capability
- ✅ Thermal runaway protection

Never deploy untested code to hardware that will roast coffee unattended.
