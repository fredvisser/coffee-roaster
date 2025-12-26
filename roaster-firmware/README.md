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

### Quick Setup (Automated)

```bash
# 1. Navigate to firmware directory
cd roaster-firmware

# 2. Run automated setup script (installs all dependencies)
./setup_libraries.sh

# 3. Compile and upload firmware
./run_tests.sh 1 upload

# Or just compile (creates .bin in build/ directory)
./run_tests.sh 1 compile
```

### Manual Setup

#### 1. Install Arduino CLI

```bash
brew install arduino-cli  # macOS
# or download from https://arduino.github.io/arduino-cli/
```

#### 2. Install ESP32 Core

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
arduino-cli core update-index
arduino-cli core install esp32:esp32@3.3.3
```

#### 3. Install Required Libraries

```bash
arduino-cli lib install "EasyNextionLibrary" "MAX6675 library" "SimpleTimer" \
  "AutoPID" "ESP32Servo" "ElegantOTA@3.1.7" "ArduinoJson"
```

#### 4. Install AsyncWebServer Libraries (manual)

```bash
cd ~/Documents/Arduino/libraries
git clone https://github.com/ESP32Async/AsyncTCP.git
git clone https://github.com/ESP32Async/ESPAsyncWebServer.git
```

#### 5. Configure ElegantOTA for Async Mode ⚠️ IMPORTANT

ElegantOTA must be configured for async mode to work with ESPAsyncWebServer:

```bash
# Edit ~/Documents/Arduino/libraries/ElegantOTA/src/ElegantOTA.h
# Change: #define ELEGANTOTA_USE_ASYNC_WEBSERVER 0
# To:     #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1

# Or use this one-liner:
sed -i '' 's/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1/' ~/Documents/Arduino/libraries/ElegantOTA/src/ElegantOTA.h
```

The automated `./setup_libraries.sh` script handles this configuration automatically.

#### 6. Compile and Upload

```bash
# Compile
arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware.ino

# Upload
arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora roaster-firmware.ino
```

### Build Artifacts

Compiled firmware is automatically copied to `build/roaster-firmware.bin`:

```bash
# Compile firmware (automatically saves .bin to build/)
./run_tests.sh 1 compile
```

**Note:** `.bin` files are excluded from version control via `.gitignore`.

## Over-The-Air (OTA) Updates

After initial USB flash, firmware can be updated wirelessly without physical access to the device.

### Accessing OTA Interface

1. Connect to the same WiFi network as the roaster
2. Navigate to: **`http://roaster.local/update`**
   - Alternative: Use device IP address if mDNS doesn't work

### Uploading New Firmware

1. **Compile firmware** to generate `.bin` file:
   ```bash
   ./run_tests.sh 1 compile
   ```

2. **Access OTA interface** at `http://roaster.local/update`

3. **Upload firmware**:
   - Click "Choose File" and select `build/roaster-firmware.bin`
   - Click "Update"
   - Device will automatically update and reboot (~30 seconds)

4. **Verify update**: Check version or timestamp in debug console

### OTA Best Practices

- ✅ Test firmware on bench hardware before OTA deployment
- ✅ Keep backup `.bin` files of known-good firmware
- ✅ Ensure stable WiFi connection during update
- ✅ Wait for device to fully reboot before reconnecting
- ⚠️ Never interrupt power during OTA update
- ⚠️ OTA updates preserve WiFi credentials and roast profiles

### Helper Scripts

- **`setup_libraries.sh`**: Automated dependency installation (arduino-cli required)
- **`run_tests.sh`**: Build, upload, and test firmware and unit tests
  - `./run_tests.sh 1 compile` - Compile main firmware
  - `./run_tests.sh 1 upload` - Upload to connected board
  - `./run_tests.sh 1 all` - Compile, upload, and monitor
  - See script for test suite options (2-7)

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

## Web Profile Editor

- Access at: `http://roaster.local/profile`
- Features:
  - Drag-and-drop points on an interactive graph (time vs temperature)
  - Up to 10 setpoints, each with time (seconds), temperature (°F), fan (%)
  - Save named profiles to NVS; activate and delete from the UI
  - Lists saved profiles and highlights active one
  - Undo/Redo for edits (drag, add, remove, apply)
  - Snapping controls (toggle + step sizes for time/temperature)
  - Import/Export profiles as JSON files

### REST API (id-based)

- GET `/api/profiles`: List all saved profiles
  - Response: `{ profiles: [{ id, name, active }], active: id }`
- POST `/api/profiles`: Create a profile
  - Body: `{ name, setpoints?: [...], activate?: boolean }`
  - Response: `{ ok: true, id, name, setpoints }`
- GET `/api/profiles/:id`: Fetch a saved profile by id
  - Response: `{ id, name, setpoints, active?: boolean }`
- PUT `/api/profiles/:id`: Create/update a profile by id (id from path wins)
  - Body: `{ name, setpoints, activate?: boolean }`
  - Response: `{ ok: true, id, name, setpoints, active?: id }`
- POST `/api/profiles/:id/activate`: Activate a saved profile by id
  - Response: `{ ok: true, active: id, name }`
- DELETE `/api/profiles/:id`: Delete a saved profile (409 if active)

Notes:
- Times are seconds in API/UI; firmware stores milliseconds internally.
- Temp bounds: 0–500°F; Fan bounds: 0–100% (validated server-side).
- Names are display-only; storage is keyed by opaque ids (`pf_<id>` data, `pm_<id>` meta).
- Active profile id is stored under `preferences` key `active_profile_id` for boot-time load.

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
