# Coffee Roaster Firmware

ESP32-based coffee roaster control system with PID temperature control, dual fan management, and web interface.

## Features

- **Temperature Control**: Dual MAX6675 thermocouple sensors with PID control
- **Heating Element**: PWM-controlled heating element (0-255 range)
- **Dual Fan Control**: 
  - PWM fan for bean agitation
  - BDC fan with servo control for air circulation
- **HMI Display**: Nextion touchscreen interface
- **Network Features**: 
  - WiFi connectivity
  - WebSocket real-time monitoring
  - OTA firmware updates
  - mDNS discovery (roaster.local)
- **Safety Systems**:
  - Thermal runaway protection
  - Over-temperature cutoff (500°F absolute, 460°F roasting)
  - Sensor failure detection
  - Emergency shutdown procedures
  - Hardware watchdog timer
- **Profile Management**: Custom roast profiles with time/temperature/fan curves

## Hardware Requirements

- ESP32 development board (tested on ESP32 Nano)
- MAX6675 thermocouple amplifiers (x2)
- Heating element with PWM control circuit
- PWM fan
- BDC fan with servo control
- Nextion HMI display
- See `../PCBs/` for custom control board design

## Building

### Using arduino-cli (recommended)

```bash
# Install dependencies
arduino-cli lib install "EasyNextionLibrary" "MAX6675 library" "SimpleTimer" \
  "AutoPID" "ESP32Servo" "ElegantOTA@3.1.7" "ArduinoJson"

# Install async libraries manually
cd ~/Documents/Arduino/libraries
git clone https://github.com/ESP32Async/AsyncTCP.git
git clone https://github.com/ESP32Async/ESPAsyncWebServer.git

# Configure ElegantOTA for async mode
# Edit ~/Documents/Arduino/libraries/ElegantOTA/src/ElegantOTA.h
# Change: #define ELEGANTOTA_USE_ASYNC_WEBSERVER 0
# To:     #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

# Compile
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino

# Upload
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora roaster-firmware.ino
```

See `../QUICKSTART.md` for detailed setup instructions.

## Configuration

Key parameters in `roaster-firmware.ino`:
- `KP`, `KI`, `KD`: PID tuning constants
- `MAX_SAFE_TEMP`: Absolute temperature limit (500°F)
- `MAX_ROAST_TEMP`: Maximum roasting temperature (460°F)
- `COOLING_TARGET_TEMP`: Temperature to complete cooling (145°F)

## Project Structure

```
roaster-firmware/
├── roaster-firmware.ino    # Main firmware
├── Profiles.hpp            # Roast profile management
├── Network.hpp             # WiFi, WebSocket, OTA
├── TODO.md                 # Development tasks
└── tests/                  # Unit and hardware tests
    ├── test_profiles/      # Profile interpolation tests
    ├── test_pid/           # PID controller tests
    ├── test_state_machine/ # State transition tests
    ├── test_safety/        # Safety system tests (CRITICAL)
    ├── hardware_validation/# Hardware-in-the-loop tests
    └── unit_tests/         # Test runner
```

## Testing

Comprehensive test suite covering:
- Profile management and interpolation
- PID controller behavior and stability
- State machine transitions and safety interlocks
- **Critical safety features** (thermal runaway, over-temp, sensor faults)
- Hardware validation tests

```bash
# Run all tests
./run_tests.sh

# Or run individual test suites
arduino-cli compile --fqbn esp32:esp32:nano_nora tests/test_safety/test_safety.ino
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora tests/test_safety/test_safety.ino
arduino-cli monitor -p /dev/cu.usbserial-* -c baudrate=115200
```

See `tests/README.md` for complete testing documentation.

## Safety

⚠️ **This device controls high temperatures and line voltage.**

All code changes must pass safety tests:
- Thermal runaway protection
- Over-temperature cutoff
- Sensor validation
- Emergency stop capability
- Fail-safe defaults (heater OFF, fan ON)

**Never deploy untested code to hardware that will roast coffee unattended.**

## Documentation

- `../QUICKSTART.md` - Quick start guide
- `../OTA_SETUP.md` - OTA update configuration
- `../agents.md` - Development guidelines
- `tests/README.md` - Testing documentation
- `../CODE_REVIEW.md` - Code review findings and fixes

## Temperature Units

All temperatures in this firmware are in **Fahrenheit (°F)**. This is documented throughout the code to prevent confusion.

## License

See `../LICENSE` or repository root for licensing information.

## Contributing

1. Write tests for new features
2. Ensure all existing tests pass
3. Test safety features thoroughly
4. Update documentation
5. Submit pull request with test results

## Support

For issues, questions, or contributions, see the main repository README.
