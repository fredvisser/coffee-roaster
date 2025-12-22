# Coffee Roaster Firmware - Test Suite Documentation

## Overview

This directory contains comprehensive unit tests and hardware validation tests for the coffee roaster firmware. The tests are designed to verify correct operation of all major subsystems including profile management, PID control, state machine behavior, and critical safety features.

## Test Organization

```
tests/
├── README.md                    # This file
├── unit_tests.ino               # Main test runner
├── test_profiles.ino            # Profile management tests
├── test_pid.ino                 # PID controller tests
├── test_state_machine.ino       # State machine tests
├── test_safety.ino              # Safety system tests
└── hardware_validation.ino      # Hardware-in-the-loop tests
```

## Prerequisites

### Software Requirements

1. **arduino-cli** - Command line tool for Arduino development
   ```bash
   brew install arduino-cli  # macOS
   ```

2. **ESP32 Platform**
   ```bash
   arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   arduino-cli core update-index
   arduino-cli core install esp32:esp32
   ```

3. **AUnit Testing Framework**
   ```bash
   arduino-cli lib install "AUnit"
   ```

4. **Project Dependencies**
   ```bash
   arduino-cli lib install "EasyNextionLibrary"
   arduino-cli lib install "MAX6675 library"
   arduino-cli lib install "SimpleTimer"
   arduino-cli lib install "AutoPID"
   arduino-cli lib install "ESP32Servo"
   ```

### Hardware Requirements (for hardware validation only)

- ESP32 development board (nano_nora or compatible)
- Roaster control board with:
  - MAX6675 thermocouple amplifiers (x2)
  - Heating element control circuit
  - PWM fan
  - BDC fan with servo control
  - Appropriate power supply

## Running Unit Tests

Unit tests verify software logic without requiring hardware. They test algorithms, data structures, and control flow.

### Running Individual Test Suites

#### Profile Management Tests
```bash
cd roaster-firmware
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/test_profiles/test_profiles.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/test_profiles/test_profiles.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

Tests covered:
- Adding/clearing setpoints
- Temperature interpolation between setpoints
- Fan speed interpolation
- Profile progress calculation
- Serialization/deserialization
- Boundary conditions

#### PID Controller Tests
```bash
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/test_pid/test_pid.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/test_pid/test_pid.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

Tests covered:
- Output clamping (0-255 range)
- Response to setpoint changes
- Proportional term behavior
- Integral term accumulation
- Anti-windup protection
- Stability and settling time

#### State Machine Tests
```bash
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/test_state_machine/test_state_machine.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/test_state_machine/test_state_machine.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

Tests covered:
- State transitions (IDLE → START_ROAST → ROASTING → COOLING → IDLE)
- Safety interlocks (no heater in IDLE/COOLING)
- Emergency stop from any state
- Invalid transition prevention
- Timing and duration tracking

#### Safety System Tests
```bash
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/test_safety/test_safety.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/test_safety/test_safety.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

Tests covered (CRITICAL):
- Thermal runaway detection
- Over-temperature protection
- Sensor failure detection (open/shorted thermocouple)
- Emergency shutdown procedures
- Fail-safe behavior
- Multiple fault conditions

### Running All Unit Tests

```bash
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/unit_tests/unit_tests.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/unit_tests/unit_tests.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

## Running Hardware Validation Tests

**⚠️ SAFETY WARNING**: Hardware validation tests control real heating elements and fans. 

### Safety Precautions

- [ ] Never leave hardware unattended during testing
- [ ] Have a fire extinguisher nearby
- [ ] Ensure adequate ventilation
- [ ] Keep flammable materials away
- [ ] Monitor temperature readings continuously
- [ ] Be prepared to cut power immediately if needed

### Running Hardware Tests

```bash
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/hardware_validation/hardware_validation.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/hardware_validation/hardware_validation.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

Tests covered:
- Thermocouple reading verification
- Sensor consistency checks
- Heater control (PWM)
- PWM fan control
- BDC fan control (servo)
- Fan cooling effectiveness
- Emergency shutdown verification
- Over-temperature protection
- Short roast cycle simulation

The hardware tests include a 10-second countdown before starting. Press RESET to abort if needed.

## Interpreting Test Results

### Test Output Format

```
Testing: Profile_AddSetpoint_Basic... ✓ PASS
Testing: PID_OutputClamping_MaxValue... ✓ PASS
Testing: Safety_ThermalRunaway_Detection... ✓ PASS
```

### Test Summary

At the end of each test run:
```
═══════════════════════════════════════════════════════════
                     TEST SUMMARY                          
═══════════════════════════════════════════════════════════
Total Tests:   45
Passed:        45 (100%)
Failed:        0
Skipped:       0
═══════════════════════════════════════════════════════════
✓ ALL TESTS PASSED!
```

### Failure Analysis

When a test fails, you'll see:
```
Testing: PID_OutputClamping_MaxValue... ✗ FAIL
  Expected: 255
  Actual:   270
  At line: 87
```

Common failure causes:
- **Timing issues**: Tests may be sensitive to delays
- **Hardware variation**: Component tolerances
- **Environmental factors**: Ambient temperature affects readings
- **Sensor drift**: Thermocouples may need calibration

## Continuous Integration

### GitHub Actions Workflow

The project uses GitHub Actions for automated testing. See `.github/workflows/arduino-ci.yml` for the CI configuration.

Workflow triggers:
- Push to `main`, `add-bdc`, or `develop` branches
- Pull requests to `main`

CI steps:
1. Install arduino-cli
2. Install ESP32 platform
3. Install dependencies
4. Compile firmware
5. Compile all test suites
6. Archive build artifacts

### Local Pre-Commit Testing

Before committing changes, run:

```bash
# Quick compile check
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware/roaster-firmware.ino

# Run unit tests (if you have hardware)
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware/tests/test_profiles/test_profiles.ino
```

## Test Coverage

### Current Coverage

| Component | Unit Tests | Hardware Tests | Coverage |
|-----------|------------|----------------|----------|
| Profile Management | ✓ | ✓ | High |
| PID Controller | ✓ | ✓ | High |
| State Machine | ✓ | ✓ | High |
| Safety Systems | ✓ | ✓ | Critical |
| Temperature Sensors | ✓ | ✓ | High |
| Heater Control | ✓ | ✓ | High |
| Fan Control | ✓ | ✓ | High |
| Network/WiFi | ✗ | ✗ | Low |
| Nextion Display | ✗ | ✗ | Low |

### Areas for Improvement

- Network communication tests
- WebSocket message handling
- Nextion display integration tests
- Long-duration roast tests
- Temperature ramp rate tests
- Power cycle/recovery tests

## Adding New Tests

### Creating a New Test

1. Create a new `.ino` file in the `tests/` directory
2. Include AUnit framework:
   ```cpp
   #include <AUnit.h>
   using namespace aunit;
   ```

3. Write test cases:
   ```cpp
   test(YourTest_Description) {
     // Arrange
     int expected = 42;
     
     // Act
     int actual = yourFunction();
     
     // Assert
     assertEqual(expected, actual);
   }
   ```

4. Add setup and loop:
   ```cpp
   void setup() {
     Serial.begin(115200);
     while (!Serial);
   }
   
   void loop() {
     TestRunner::run();
   }
   ```

5. Compile and run:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:nano_nora tests/your_test.ino
   ```

### Test Naming Conventions

- Test files: `test_<component>.ino`
- Test functions: `test(<Component>_<Feature>_<Scenario>)`
- Examples:
  - `test(Profile_Interpolation_LinearRamp)`
  - `test(Safety_OverTemp_EmergencyShutdown)`
  - `test(PID_Response_LargeStepChange)`

### AUnit Assertions

Common assertions:
- `assertEqual(expected, actual)` - Values must match
- `assertNotEqual(a, b)` - Values must differ
- `assertTrue(condition)` - Condition must be true
- `assertFalse(condition)` - Condition must be false
- `assertLess(a, b)` - a must be < b
- `assertMore(a, b)` - a must be > b
- `assertNear(expected, actual, tolerance)` - For floating point

## Troubleshooting

### Common Issues

**Problem**: Tests won't compile
- **Solution**: Ensure AUnit library is installed: `arduino-cli lib install "AUnit"`

**Problem**: Upload fails
- **Solution**: Check USB connection, verify correct port: `arduino-cli board list`

**Problem**: Tests hang or timeout
- **Solution**: Increase timeout in test setup: `TestRunner::setTimeout(120)`

**Problem**: Hardware tests exceed temperature limits
- **Solution**: Reduce `MAX_TEST_HEATER_OUTPUT` in `hardware_validation.ino`

**Problem**: Inconsistent sensor readings
- **Solution**: Check thermocouple connections, verify MAX6675 power supply

### Getting Help

- Review main firmware code in `roaster-firmware.ino`
- Check component datasheets (MAX6675, ESP32)
- Review AUnit documentation: https://github.com/bxparks/AUnit
- Check GitHub Issues for known problems

## Best Practices

1. **Run tests frequently** during development
2. **Write tests first** for new features (TDD)
3. **Test safety features** thoroughly and repeatedly
4. **Never skip hardware validation** before deploying to production
5. **Document test failures** and resolutions
6. **Keep tests fast** - unit tests should complete in seconds
7. **Make tests deterministic** - avoid random values
8. **Test boundary conditions** - min/max values, edge cases
9. **Test error conditions** - sensor failures, invalid inputs
10. **Update tests** when changing firmware behavior

## Safety Testing Checklist

Before declaring the firmware production-ready:

- [ ] All unit tests pass
- [ ] All hardware validation tests pass
- [ ] Thermal runaway protection verified
- [ ] Over-temperature protection verified
- [ ] Sensor failure detection verified
- [ ] Emergency stop tested from all states
- [ ] Multiple complete roast cycles tested successfully
- [ ] Cooling cycle tested to completion
- [ ] Power cycle recovery tested
- [ ] Sensor disconnection during roast tested
- [ ] Maximum temperature limits respected
- [ ] Fan failure handling verified

## License

This test suite is part of the coffee roaster firmware project. Refer to the main project README for licensing information.

## Contributing

When contributing new features:
1. Write tests for new functionality
2. Ensure all existing tests still pass
3. Add hardware validation if applicable
4. Update this README with new test coverage
5. Submit PR with test results

---

**Remember**: This device controls high temperatures and line voltage. All code changes must pass safety tests. Never deploy untested code to hardware that will roast coffee unattended.
