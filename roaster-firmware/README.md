### Helper Scripts
# Coffee Roaster Firmware

ESP32-S3 JC4827W543C-based coffee roaster control system with PID temperature control, dual fan management, LVGL touchscreen UI, and web interfaces.

## Features

- **Temperature Control**: Dual MAX6675 thermocouple sensors with PID control
- **Heating Element**: PWM-controlled heating element (0-255 range)
- **Dual Fan Control**: 
  - PWM fan for bean agitation
  - BDC fan with servo control for air circulation
- **Local Display**: LVGL-based onboard touchscreen UI on the JC4827W543C panel
- **Network Features**: 
  - WiFi connectivity
  - WebSocket real-time monitoring
  - OTA firmware updates
  - mDNS discovery (roaster-dev.local)
- **Safety Systems**:
  - Thermal runaway protection
  - Over-temperature cutoff (500°F absolute, 460°F roasting)
  - Sensor failure detection
  - Emergency shutdown procedures
  - Hardware watchdog timer
- **Profile Management**: Custom roast profiles with time/temperature/fan curves
- **Persistent Debug Logs**: Appends JSON Lines to the onboard TF/microSD card when available

## Hardware Requirements

- JC4827W543C ESP32-S3 display board
- MAX6675 thermocouple amplifiers (x2)
- Heating element with PWM control circuit
- PWM fan
- BDC fan with servo control
- Integrated local display and touch hardware on the JC4827W543C board
- Optional FAT32-formatted microSD/TF card (8–32 GB)

### TF Card Logging

At boot, the firmware mounts the TF card over SPI using GPIO10 (CS), GPIO11 (MOSI), GPIO12 (clock), and GPIO13 (MISO). Debug entries are appended as JSON Lines with a boot ID and uptime in milliseconds. The active file is `/roaster-debug.jsonl`; up to four 1 MiB rotated segments are retained alongside it. Open `http://roaster-dev.local/console` and choose **Card History** to page through, filter, and download a segment. If the card is absent or cannot be written, the roaster continues using the RAM log buffer and Serial output. Card detection happens at boot, so insert a card before powering on.

## Building

### LVGL Simulator

You can render the display screens locally on macOS using the SDL-backed LVGL simulator:

```bash
brew install cmake pkg-config sdl2

cd roaster-firmware
./tools/lvgl-sim.sh run start
./tools/lvgl-sim.sh screenshot all
```

Screenshot output is written to `roaster-firmware/build/simulator-screens/` and can be inspected directly or shared back into Copilot for visual review.

### Quick Setup (Automated)

```bash
# 1. Navigate to firmware directory
cd roaster-firmware

# 2. Run automated setup script (installs all dependencies)
./tools/bootstrap.sh

# 3. Upload over USB (builds if needed)
./tools/firmware.sh upload

# Or just compile (creates .bin in build/ directory)
./tools/firmware.sh build
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
arduino-cli core install esp32:esp32@3.3.11
```

#### 3. Install Required Libraries

```bash
arduino-cli lib install "MAX6675 library" "SimpleTimer" \
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

The automated `./tools/bootstrap.sh` script handles this configuration automatically.

#### 6. Compile and Upload

```bash
# Compile
./tools/firmware.sh build --board jc4827w543c

# Upload that build over USB without recompiling
./tools/firmware.sh upload --board jc4827w543c
```

### Build Artifacts

Compiled firmware is automatically copied to `build/roaster-firmware.bin`:

```bash
# Compile firmware (automatically saves .bin to build/)
./tools/firmware.sh build
```

**Note:** `.bin` files are excluded from version control via `.gitignore`.

Builds use all detected CPU cores by default. Set `ROASTER_BUILD_JOBS` to tune
parallel compilation for a constrained machine, for example:

```bash
ROASTER_BUILD_JOBS=4 ./tools/firmware.sh build --board jc4827w543c
```

The firmware revision defaults to the current Git commit. Override it with
`ROASTER_BUILD_VERSION` when a release label or date is required.

## Over-The-Air (OTA) Updates

After the initial USB flash, run from `roaster-firmware/` on the same network
as the roaster. Confirm the roaster is idle with the heater off before uploading.

```bash
# Point to your roaster.
export OTA_HOST=roaster-fred.local

# Once per Mac: open Wi-Fi > Web access on the roaster and tap Pair browser/CLI.
./tools/firmware.sh pair
# Match the displayed code and tap Approve. The token is saved in macOS Keychain.

# Upload the last matching build without recompiling (`ota` is an alias).
./tools/firmware.sh ota-upload --board jc4827w543c

# If you've edited the source and haven't built it yet, use this instead:
./tools/firmware.sh ota-upload --board jc4827w543c --rebuild

```

Use the same `OTA_HOST` for pairing and uploading. OTA uses IPv4 for `.local`
names to avoid mDNS lookup timeouts on macOS. If the name still doesn't resolve,
use the roaster's IP address for both commands and pair again under that address.
If the roaster still runs older firmware without pairing, install this version
over USB or through its existing `/update` page, then pair before using `ota-upload`.

Both `ota-upload` and USB `upload` reuse `build/roaster-firmware.bin` from the
last successful build if it matches the selected board and the required artifacts
are present. Otherwise they build automatically. Source edits do **not** invalidate
the saved binary: after editing, run `build` before uploading or pass `--rebuild`
to either upload command.

Keep power and Wi-Fi connected until the roaster reboots, then confirm the expected
change on the device. If needed, use the roaster's `/update` page and select
`build/roaster-firmware.bin` for a manual OTA upload.

### Helper Scripts

- **`tools/bootstrap.sh`**: Automated dependency installation (arduino-cli required)
- **`tools/firmware.sh`**: Main firmware build, upload, OTA, and monitor entrypoint
  - `./tools/firmware.sh build` - Compile main firmware
  - `./tools/firmware.sh upload` - Upload over USB; build only if needed
  - `./tools/firmware.sh ota-upload` - Upload over Wi-Fi; build only if needed (`ota` also works)
  - `./tools/firmware.sh run` - Compile, upload, and monitor
  - `./tools/firmware.sh build --board jc4827w543c` - Compile for the JC4827W543C target
- **`tools/tests.sh`**: Named test-suite entrypoint
  - `./tools/tests.sh compile safety` - Compile the safety suite
  - `./tools/tests.sh run pid` - Compile, upload, and monitor the PID suite

## PID tuning

With the idle roaster below 140F, load 1/2 cup of coffee beans. On mains power, open `/pid` and select
**Start autotune**. Stay with the roaster; you can cancel while it runs.
The tune uses full fan speed and fits one local thermal model
without waiting for equilibrium, then runs a ramp/hold check at the same fan PWM.
Successive fits must stabilize before early completion; acquisition has a 175-second
ceiling. Heating stops by 295 seconds with either a verified candidate or an explicit
inconclusive result. Cooling continues separately. Current settings are not
overwritten: review the proposed gains next to the saved gains, then select
**Apply verified gains** if you want to use them.

The tuning record includes the measured temperature range. A quick local check does **not**
qualify the full roasting range or different airflow/bean loads. Initial SIMC
tuning uses PI (Kd = 0), retaining effective model delay rather than replacing it
with a noise-threshold onset estimate.

The latest ten run summaries survive reboot in NVS, including failures and manual
adjustments and current-controller verification results. History, downloads,
and whole-controller restore remain available through the PID API. Active, previous, and
last-verified snapshots are stored independently of the rotating history.
**Manual override** edits Kp, Ki, and Kd and scales scheduled gains without deleting
the thermal model. These edits are unverified. Legacy model coefficients are disabled; retune
after upgrading. Existing scalar PID gains remain available as fallback.

The current full trace is available at `/api/calibrate-pid/trace` (JSON) or
`/api/pid/trace.csv`. With a TF card, existing
console logging retains `PIDTRACE` rows with fields:
`runId,elapsedMs,tempF,targetF,heater,feedforward,P,I,D,inletF,fanPWM,targetRateFps,Kp,Ki,Kd,phase`.
Older traces are found in the SD console logs, not retained in NVS. A new run or
reboot clears the RAM trace. Storage failures are reported, not treated as saves.
JSON trace metadata includes the terminal error code and run ID. Heater output is
the requested PWM, not a measured electrical-power reading. The heater driver
latches duty for each one-second window, so 250ms controller updates cannot restart
the window and suppress heating; zero output still switches it off immediately.
Optional SystemLink mirroring runs after cooling, never during tuning/control.

If a tune stops with `sensor_pair_inconsistent`, inspect both thermocouples and
their wiring: the fan/inlet reading may be normal while the bean reading drops
implausibly low. `fan_sensor_failed` indicates an invalid or discontinuous fan
reading. Both stop heating and retain error latching; do not bypass the checks.

Quick tweaks (change one setting about 10%, repeat the same check, then compare):

| Symptom | Adjustment |
|---|---|
| Lag at full heater power | Gains cannot add missing heat; reduce the ramp or correct airflow/load safely. |
| Ramp lag, good hold, available power | Check airflow and load; the local tune may not cover that ramp. |
| Slow but stable response | Increase Kp 10%. |
| Settled hold error | Check sensor/model bias first; then increase Ki 10%. |
| Oscillation | Reduce Kp; reduce Ki for slow integral-driven hunting. |
| Overshoot / noisy output | Check saturation/anti-windup; reduce Ki for overshoot or Kd for noise. |

Ki is integral **gain**, not integral time. The restore API restores gains, model,
and feedforward together. The separate current-controller check API covers only 120-200F; a pass
updates the last-verified snapshot without claiming higher-temperature qualification.
Targets for the built-in candidate check are ramp MAE <= 2F, hold MAE <= 1F,
90% of ramp points within 3F, and overshoot <= 4F after stated recovery windows.
These targets require supervised hardware qualification before deployment.

Run deterministic, hardware-free regressions with `./tools/tests.sh run host`.
Compile the embedded checks with `./tools/tests.sh compile step-response` and
`./tools/tests.sh compile pid`; run them on a supervised bench when available.

## Web UI Security

Console, Profiles, PID, Update, SystemLink, and Server share the same navigation
and page frame. The Update page uses the built-in ElegantOTA uploader.

Open **Wi-Fi > Web access** on the touchscreen and tap **Pair browser/CLI**.
Open the address or scan the QR code, then select **Request to pair** in the
browser. Compare the four-digit codes and tap **Approve** on the roaster.
Pairing requests expire after one minute. Scripts can use `firmware.sh pair`
on macOS, or the `/api/pair-request?cli=1` and `/api/pair-claim` endpoints and
send `Authorization: Bearer <token>` on subsequent requests.

Paired sessions survive a firmware reboot and have full access to the web UI,
API, and OTA. The **Server** page lists and revokes sessions and lets you change
the mDNS name. New sessions record the pairing date, using the browser or CLI
clock if the roaster clock isn't set. Sessions saved without a date stay valid.
Changing the address requires pairing the browser again. The
`/WebSocket` endpoint remains unauthenticated for Artisan telemetry.

The device still serves plain HTTP. Anyone able to intercept traffic can steal
a session and use it until revoked. Keep the roaster on a trusted, isolated LAN;
do not expose it directly to the internet.

## Configuration

Key parameters in `roaster-firmware.ino`:
- `KP`, `KI`, `KD`: PID tuning constants
- `MAX_SAFE_TEMP`: Absolute temperature limit (500°F)
- `MAX_ROAST_TEMP`: Maximum roasting temperature (460°F)
- `COOLING_TARGET_TEMP`: Temperature to complete cooling (145°F)

## Project Structure

```
roaster-firmware/
├── roaster-firmware.ino    # Main firmware sketch
├── src/
│   ├── platform/           # Board config, shared roaster types, calibration types
│   ├── display/            # Display backend selection and adapters
│   ├── profiles/           # RoastProfile model and profile storage logic
│   ├── control/            # PID control, validation, and autotuning
│   ├── network/            # WiFi, OTA, and embedded web UIs
│   ├── integrations/       # External service integrations such as SystemLink
│   └── support/            # Shared support utilities
├── tools/                  # Canonical developer entrypoints
└── tests/                  # Unit and hardware tests
    ├── test_profiles/      # Profile interpolation tests
    ├── test_pid/           # PID controller tests
    ├── test_state_machine/ # State transition tests
    ├── test_safety/        # Safety system tests (CRITICAL)
    ├── hardware_validation/# Hardware-in-the-loop tests
  ├── test_step_response/ # Step response tuning tests
  ├── unit_tests/         # Test runner
  └── wifi_probe/         # Connectivity probe sketch
```

## Testing

Comprehensive test suite covering:
- Profile management and interpolation
- PID controller behavior and stability
- State machine transitions and safety interlocks
- **Critical safety features** (thermal runaway, over-temp, sensor faults)
- Hardware validation tests

```bash
# Show named test suites
./tools/tests.sh list

# Run a test suite
./tools/tests.sh run safety

# Or run individual test suites
./tools/tests.sh run safety --board jc4827w543c
```

See `tests/README.md` for complete testing documentation.

## Web Profile Editor

- Access at: `http://roaster-dev.local/profile`
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
