# JC4827W543C Port Plan

## Goal

Port the roaster controller from the current ESP32 Nano + Nextion architecture to a single JC4827W543C panel using the integrated ESP32-S3, LCD, capacitive touch, and an LVGL-based UI.

This plan assumes a full controller migration, not just a display swap. The roaster power electronics, thermocouple modules, fans, and heater driver remain external. The JC4827W543C replaces both the current ESP32 board and the Nextion HMI.

## Recommended Migration Strategy

Do this as a staged port, not a big-bang rewrite.

1. Keep roast-control behavior and safety logic intact.
2. Remove the Nextion coupling behind a display/UI adapter.
3. Bring up LVGL on the JC board with a read-only dashboard first.
4. Add user actions only after the live dashboard is stable.
5. Move profile editing and tuning screens after the core roast flow works.

The current firmware already has a clean state-machine center. The main migration challenge is that the UI is still directly embedded in control code through `EasyNex` writes and `triggerX()` callbacks.

## What Exists Today

### Hardware

- Main controller: separate ESP32 Nano-class board
- Display: Nextion over UART at 115200
- Sensors: two MAX6675 thermocouple interfaces
- Outputs: heater PWM, PWM fan, BDC fan servo output

### Firmware coupling points to remove

Current display logic is directly mixed into roast control in these files:

- `roaster-firmware.ino`
- `ProfileEditor.hpp`
- `Network.hpp`

Key examples:

- Direct page changes: `page Start`, `page Roasting`, `page Cooling`, `page Error`, `page ProfileActive`
- Direct variable writes: `globals.currentTempNum.val`, `globals.nextSetTempNum.val`, `globals.setTempNum.val`, `globals.setpointFan.val`, `globals.setpointProg.val`
- Direct command callbacks: `trigger0()` through `trigger4()`
- Direct waveform plotting into the Nextion waveform control in `ProfileEditor.hpp`

That direct coupling is the first thing to change.

## JC4827W543C Constraints And Assumptions

Based on the reference LVGL project and the vendor schematic the board appears to provide:

- ESP32-S3 MCU
- 480x272 integrated LCD
- GT911 capacitive touch
- USB-C for power/programming
- microSD wired on dedicated GPIOs
- several exposed GPIO headers for external I/O

### Board resources that should be treated as reserved

- LCD interface: reserved for the onboard display
- Touch controller: reserved for onboard touch
  - SDA: GPIO8
  - SCL: GPIO4
  - INT: GPIO3
  - RST: GPIO38
- microSD: likely tied to GPIO10-GPIO13 on the board schematic

### Exposed GPIOs visible in the schematic snapshot

- Header group with GPIO46, GPIO9, GPIO14, GPIO5
- Header group with GPIO6, GPIO7, GPIO15, GPIO16
- Header groups exposing GPIO17 and GPIO18 with 3.3V and GND

### Important electrical notes

- GPIO46 on ESP32-S3 is input-only. Use it only for an input such as shared MAX6675 SO.
- All external signals to the JC board must be 3.3V safe.
- If the MAX6675 modules are currently powered at 5V, confirm the SO pin is not driving 5V into the ESP32-S3. Prefer powering the MAX6675 modules from 3.3V for this design.
- Verify the heater and fan driver inputs switch reliably from 3.3V logic.
- Do not assume the board can be powered arbitrarily through USB-C and external 5V at the same time without checking the schematic and protection path.

## Wiring Changes

## Architecture change

Remove:

- current ESP32 Nano board
- UART wiring to the Nextion display
- Nextion-specific power and serial harness

Keep:

- thermocouples and MAX6675 modules
- heater driver stage
- PWM fan driver stage
- BDC fan and its control interface
- existing high-voltage and high-current power electronics

Add:

- JC4827W543C as the main controller and touchscreen HMI
- new low-voltage harness from the JC board headers to the roaster control PCB

## Proposed GPIO map

This pin map is intended to avoid the onboard display, touch, and SD wiring while leaving a debug/expansion UART available.

### Inputs and sensor bus

- `TC_SPI_SCK`: GPIO5
- `TC_SPI_SO`: GPIO46
- `TC1_CS`: GPIO9
- `TC2_CS`: GPIO14

Notes:

- The MAX6675 interface can be treated as software SPI here.
- Share `SCK` and `SO` between both MAX6675 modules and give each module its own chip select.
- GPIO46 is a good fit for `SO` because it is input-only.
- This keeps the MAX6675 wiring on the single breakout group `GPIO46, GPIO9, GPIO14, GPIO5`.

### Outputs

- `HEATER_PWM`: GPIO6
- `PWM_FAN`: GPIO7
- `BDC_FAN_SERVO`: GPIO15

### Reserved for service and expansion

- `UART1_TX`: GPIO17
- `UART1_RX`: GPIO18
- `GPIO16`: spare adjacent I/O for a future output or interlock input

## Harness changes to implement

1. Remove the existing UART TX/RX connection between controller and Nextion.
2. Route both MAX6675 modules to the JC board:
   - shared `SCK`
   - shared `SO`
   - separate `CS` lines
   - `3.3V`
   - `GND`
3. Route heater driver control input to `GPIO6`.
4. Route PWM fan control input to `GPIO7`.
5. Route BDC fan control input to `GPIO15`.
6. Leave `GPIO16` available as the adjacent spare on the output connector unless a fourth low-voltage output is added later.
7. Bring a clean common ground between the JC board and the roaster control board.
8. Decide how the JC board is powered in the enclosure:
   - preferred: regulated low-noise 5V feed into the board's intended power input path
   - acceptable during bench work: USB-C
9. If the existing control PCB exposes only a connector intended for the old ESP32 board, create a small adapter harness or interposer board instead of hand-wiring directly.

## Wiring Validation Checklist

Before any firmware porting beyond board bring-up:

1. Verify no external line exceeds 3.3V logic.
2. Verify common ground continuity.
3. Confirm heater output defaults OFF on reset.
4. Confirm both fans default to safe values on reset.
5. Confirm thermocouple readings are stable with the display running.
6. Confirm the LCD backlight and touch do not inject enough noise to corrupt MAX6675 reads.

## Firmware Port Plan

## Phase 0: Decide the control boundary

There are two viable approaches.

### Option A: Full migration to the JC board

The JC board runs:

- roast state machine
- PID and tuning logic
- WiFi, OTA, SystemLink, web API
- LVGL UI

This is the direction assumed by the rest of this document.

### Option B: Lower-risk split architecture

Keep the current controller dedicated to roast control and use the JC board only as an LVGL HMI talking over UART or WebSocket.

Use this if bench tests show UI rendering causes sensor jitter or timing instability. It is the safer fallback, but it leaves two firmwares to maintain.

## Phase 1: Board bring-up on JC4827W543C

### Build and platform changes

Replace the current board target with the ESP32-S3 target used by the reference project.

Expected build changes:

- board target: `ESP32S3 Dev Module`
- remove `EasyNextionLibrary`
- add `lvgl`
- add `GFX Library for Arduino`
- add the pin support package used by the reference project
- add `TAMC_GT911`
- add `lv_conf.h` tuned for 480x272 and partial buffering

### First bring-up goal

Boot a bare LVGL app that shows:

- firmware version
- live tick counter
- touch coordinates
- current bean temperature and fan temperature

Do not add start/stop roast actions until this read-only screen is stable.

## Phase 2: Remove Nextion-specific coupling

Create a display abstraction layer so the roast logic no longer talks in page names and widget paths.

### New module split

Recommended new files:

- `BoardConfig.hpp`
- `DisplayHal.hpp`
- `DisplayHal.cpp`
- `UiModel.hpp`
- `UiModel.cpp`
- `UiController.hpp`
- `UiController.cpp`
- `UiScreens.hpp`
- `UiScreens.cpp`
- `TouchInput.hpp`

### New responsibilities

#### `BoardConfig`

- all JC board GPIO assignments
- display/touch constants
- feature flags for future fallback builds

#### `DisplayHal`

- LVGL init
- display flush callback
- touch read callback
- periodic `lv_timer_handler()` call
- screen creation and screen switching

#### `UiModel`

Read-only snapshot of controller state for rendering.

Suggested contents:

- roaster state
- current temp
- fan temp
- setpoint temp
- fan setpoint percent
- roast progress seconds
- active profile name
- final target override
- WiFi status and IP
- autotune state
- validation state
- fault code and fault message

#### `UiController`

Small action queue from UI to control logic.

Suggested actions:

- start roast
- stop roast
- stop cooling
- apply WiFi credentials
- select profile
- adjust final target
- start tuning
- cancel tuning
- start validation
- acknowledge or inspect error

## Phase 3: Replace direct `myNex` writes with model updates

Every current `myNex.writeNum`, `myNex.writeStr`, `myNex.readNumber`, `myNex.readStr`, and `triggerX()` path should be converted into one of two patterns.

### Pattern 1: control code publishes state

Instead of this:

- write current temp directly to the display
- write new page directly to the display

Do this:

- update controller state
- build a `UiModel` snapshot
- let the UI decide which screen to present

### Pattern 2: UI emits intent

Instead of `trigger0()` through `trigger4()` being display-library callbacks, the UI should enqueue typed actions.

Example mapping:

- `trigger0()` -> `UiAction::StartRoast`
- `trigger1()` -> `UiAction::StopRoast`
- `trigger2()` -> `UiAction::StopCooling`
- `trigger3()` -> `UiAction::ApplyWifi`
- `trigger4()` -> `UiAction::OpenActiveProfile`

## Phase 4: Keep control timing deterministic

The most important architectural rule for the port is this:

The UI must never be able to delay the safety and control loop.

### Recommended runtime structure

Keep the current timer-driven control structure, but separate rendering cadence from control cadence.

- temperature acquisition: keep at the current cadence
- control loop: keep at the current cadence
- state machine: keep at the current cadence
- LVGL handler: run every 5-10 ms
- UI model refresh: 5-10 Hz is enough for values, 20 Hz max for animations

### Practical implementation guidance

1. Do not call LVGL object creation from roast control paths.
2. Do not redraw charts from inside sensor or state-machine code.
3. Use a double-buffered or copied `UiModel` snapshot so control code never waits on UI code.
4. Keep waveform and chart updates incremental.
5. Preserve the watchdog and add a UI heartbeat watchdog log if needed.

## Phase 5: Screen plan

This is the first-pass screen map. It is intentionally functional, not visual design.

## 1. Boot / Startup Screen

Purpose:

- board identity
- firmware version
- self-check progress
- WiFi connect progress
- profile load result

Shown during:

- power-on
- OTA reboot
- crash recovery

Must expose:

- visible fault if profile load fails
- visible fault if sensors fail at boot

## 2. Home / Idle Dashboard

Purpose:

- primary idle landing screen
- quick visibility into system readiness

Data:

- current bean temp
- fan temp
- active profile name
- final target temp
- WiFi status and IP
- last fault summary if any
- controller state badge: `IDLE`

Actions:

- start roast
- open profile browser
- open settings
- open tuning tools

## 3. Profile Browser

Purpose:

- choose active roast profile

Data:

- list of saved profiles
- currently active profile indicator
- final temp and duration summary

Actions:

- activate profile
- duplicate profile
- delete non-active profile
- open profile details

Phase split:

- phase 1: selection only
- phase 2: editing on-device

## 4. Profile Details

Purpose:

- inspect the chosen profile before starting

Data:

- profile name
- target final temp
- total duration
- fan schedule summary
- roast curve chart

Actions:

- set active
- adjust final target override
- start roast

LVGL replacement for the Nextion waveform:

- use `lv_chart` for temperature target visualization
- optionally overlay fan percentage as a second series or show it in a secondary mini-chart

## 5. Roasting Screen

Purpose:

- the live production screen during roast

Data:

- current bean temp
- target temp
- fan temp
- target fan percent
- elapsed roast time
- roast profile progress
- heater output percent or PWM
- active PID mode or band
- live roast curve and setpoint trace

Actions:

- stop roast
- enter cooling manually
- optional event markers later: charge, yellowing, first crack, drop

Behavior notes:

- this must be the most readable screen in the system
- prioritize large temperature and countdown/progress elements over settings clutter

## 6. Cooling Screen

Purpose:

- dedicated post-roast cooling state

Data:

- current temp
- target cooling temp
- cooling timer
- fan status

Actions:

- stop cooling early
- return to home when allowed

Behavior notes:

- if auto-validation is queued, show that explicitly

## 7. Error / Safety Screen

Purpose:

- latched presentation for critical faults

Faults to support immediately:

- sensor failed
- over temp
- exhaust over temp
- boot/profile recovery failure

Data:

- fault title
- concise operator instruction
- current temp
- fan state

Actions:

- no automatic clear for safety faults
- if a non-critical fault is introduced later, treat it separately from hard safety faults

## 8. WiFi / Network Screen

Purpose:

- replace the current Nextion WiFi configuration screen

Data:

- SSID
- password entry
- IP address
- connection status
- mDNS hostname

Actions:

- connect
- forget credentials
- test reconnect

Implementation note:

- use LVGL textarea + software keyboard
- consider keeping password entry behind a modal to avoid clutter on 480x272

## 9. Tuning / Validation Screen

Purpose:

- expose the step-response and validation workflows already present in firmware

Data:

- current controller gains
- schedule enabled/disabled
- tuning status
- last validation summary

Actions:

- start step-response tuning
- cancel tuning
- start validation roast
- apply manual gains

This can be a second-wave screen if the initial goal is production roasting first.

## 10. Diagnostics Screen

Purpose:

- replace the most valuable parts of the current browser console for local service work

Data:

- heap usage
- WiFi RSSI
- bad thermocouple read count
- board uptime
- active profile id
- last error text

Actions:

- export logs later
- reboot later

This is optional for initial bring-up because the web UI already covers much of it.

## Phase 6: Decide what stays on the web UI

Do not force the 480x272 touchscreen to do every job the browser UI already does well.

Recommended split:

- touchscreen: roast operation, profile selection, live monitoring, WiFi setup, fault handling
- web UI: deep profile editing, SystemLink config, debug console, advanced autotune results, firmware update

That split keeps the embedded UI manageable and reduces porting time.

## Code Changes By Area

## `roaster-firmware.ino`

Change:

- remove `EasyNextionLibrary`
- remove `EasyNex myNex`
- replace direct display writes with `UiModel` updates
- replace `trigger0-4` with typed UI actions consumed from a queue
- replace setup-time Nextion init with LVGL/display/touch init
- replace `myNex.NextionListen()` with `DisplayHal::tick()` or equivalent

Keep mostly unchanged:

- roast state machine
- sensor validation
- PID control
- cooling logic
- watchdog logic
- SystemLink hooks

## `ProfileEditor.hpp`

Change:

- remove waveform commands and page-switch commands
- expose profile data in a UI-neutral form that the LVGL chart can render
- keep profile CRUD behavior and storage logic

New helper worth adding:

- a function that serializes active profile points into a simple chart-friendly array

## `Network.hpp`

Change:

- remove `updateNextionActiveProfile()` and replace it with a controller-level `notifyUiProfileChanged()` call
- keep async web server and API behavior intact
- optionally expose a compact `/api/ui-state` endpoint that mirrors the new `UiModel` for debugging

## Build and repo support

Change:

- update `setup_libraries.sh`
- update `README.md`
- update `run_tests.sh` compile target for the new board
- add an LVGL config header and any display support files

## Suggested Implementation Order

1. Add JC board build target and verify serial logging.
2. Bring up LCD, touch, and a single LVGL screen.
3. Move thermocouple and output pins to the new board and verify bench I/O.
4. Replace Nextion init and direct writes with a read-only `UiModel` dashboard.
5. Add UI action queue and wire start, stop, and stop-cooling.
6. Port profile selection and profile chart.
7. Port WiFi configuration screen.
8. Port tuning and validation screens.
9. Remove all remaining Nextion code and dependencies.
10. Run full safety and roast validation on hardware.

## Test Plan For The Port

## Bench bring-up

1. Boot the JC board with display and touch only.
2. Verify thermocouple reads on both channels.
3. Verify heater output pin toggles only when commanded.
4. Verify PWM fan range.
5. Verify BDC servo pulse range.
6. Verify touch remains responsive during continuous sensor reads.

## Safety regression

1. Simulate thermocouple disconnect and confirm transition to `ERROR`.
2. Simulate over-temperature and confirm heater off and cooling fan on.
3. Simulate exhaust over-temperature and confirm error handling.
4. Confirm reboot returns outputs to safe defaults.

## UI regression

1. Confirm screen changes follow state machine transitions exactly.
2. Confirm start, stop, and cooling actions work through the new action queue.
3. Confirm profile activation updates both UI and control logic.
4. Confirm WiFi credentials save and reconnect correctly.
5. Confirm long roasts do not degrade touch or rendering performance.

## Roast validation

1. Dry-run roast without heat output connected.
2. Controlled heat test with tight supervision.
3. Full roast with known profile and compare temperature trace to the old controller.
4. Run tuning and validation workflows after the core roast path is stable.

## Open Decisions To Resolve Early

1. Full migration versus split controller/HMI fallback.
2. Final GPIO assignment after checking the complete JC board schematic.
3. Exact power-entry method inside the enclosure.
4. Whether on-device profile editing is in scope for the first release.
5. Whether the local touchscreen needs SystemLink settings or if those remain web-only.

## Recommended First Deliverable

The first usable milestone should be:

- JC board boots the roaster firmware
- both thermocouples read correctly
- heater and both fans can be driven on bench hardware
- a single LVGL dashboard shows live state
- start, stop, and cooling actions work from touch

Do not start with profile editing, WiFi text entry, or tuning screens. Those are second-wave features after the live roast path is stable.# JC4827W543C Port Plan

## Purpose

Port the current roaster controller firmware and display logic from the ESP32 Nano + Nextion architecture to the JC4827W543C board, using LVGL in the same general pattern as the reference project:

- `lvgl`
- `GFX Library for Arduino`
- `Dev Device Pins` / `PINS_JC4827W543`
- `TAMC_GT911`

This plan assumes the JC4827W543C becomes the main controller, not just a secondary display panel.

## Current Firmware Surface To Migrate

The current UI is not isolated. It is coupled directly into the roast control flow and profile helpers.

Primary coupling points:

- `roaster-firmware.ino`
  - creates `EasyNex myNex(HW_SERIAL)`
  - writes page changes and numeric updates directly from the state machine
  - reads UI values directly in `trigger0()` and `trigger3()`
- `ProfileEditor.hpp`
  - writes active profile details and waveform data directly to Nextion
- `Network.hpp`
  - updates the active profile page directly on the Nextion

Key existing UI behaviors that must survive the port:

- Start roast
- Stop roast / enter cooling
- Stop cooling / return idle
- Show safety faults
- Show live roast temperature, target temperature, fan setpoint, and progress
- Show and select the active roast profile
- Override final target temperature from the UI
- Configure WiFi credentials
- Keep web UI, OTA, WebSocket telemetry, and SystemLink support working

## Recommended Migration Strategy

Do this in two layers instead of rewriting the screen logic in place.

### Layer 1: Replace the hardware and display stack

- Move the target board to `ESP32S3 Dev Module`
- Replace Nextion UART communication with an onboard LVGL display stack
- Keep the roast state machine, profile logic, web server, OTA, and SystemLink logic intact at first

### Layer 2: Extract a display adapter API

Replace all direct `myNex.write*`, `myNex.read*`, `page ...`, and `triggerN()` calls with a local display interface.

Recommended interface shape:

- `display.begin()`
- `display.tick()`
- `display.showScreen(DisplayScreen screen)`
- `display.updateTelemetry(const DisplayTelemetry& snapshot)`
- `display.showError(const char* message)`
- `display.setWifiStatus(...)`
- `display.setActiveProfileSummary(...)`
- `display.setProfilePlotData(...)`
- `display.pollEvents(DisplayEvents& events)`

The main firmware should only consume typed events such as:

- `startRequested`
- `stopRequested`
- `coolingStopRequested`
- `wifiSaveRequested`
- `finalTargetOverrideChanged`
- `profileSelected`

That separation is the main risk reducer for this port.

## JC4827W543C Hardware Constraints

Based on the reference LVGL project and the vendor schematic snippet:

- Display resolution: `480 x 272`
- MCU: `ESP32-S3`
- Touch controller: `GT911`
- Touch wiring used by the reference project:
  - `SDA = GPIO8`
  - `SCL = GPIO4`
  - `INT = GPIO3`
  - `RST = GPIO38`
- The display bus is onboard and should be treated as reserved
- Backlight is onboard and exposed through the display board support package
- MicroSD appears to consume `GPIO10-13` on the board schematic, so do not assume those pins are free
- The schematic snippet shows these external headers as the likely usable GPIO set:
  - `GPIO5`
  - `GPIO6`
  - `GPIO7`
  - `GPIO9`
  - `GPIO14`
  - `GPIO15`
  - `GPIO16`
  - `GPIO17`
  - `GPIO18`
  - `GPIO46`

Important constraint:

- `GPIO46` on ESP32-S3 is input-only, so use it only for inputs such as MAX6675 data out, not for PWM or chip-select outputs.

Before layout or harness changes, verify all of the above against the exact JC4827W543C board revision you have in hand.

## Wiring Changes

## 1. Remove the Nextion-specific wiring

Current architecture includes a separate UART HMI. That goes away.

Remove:

- Nextion TX/RX wiring
- Nextion dedicated 5 V supply branch, if separate
- Any Nextion page/trigger assumptions in the harness

Keep:

- Main heater drive path
- PWM fan drive path
- BDC fan drive path
- Thermocouple interfaces
- Existing power stage and safety hardware

## 2. Replace the ESP32 Nano with the JC4827W543C

The JC4827W543C becomes the MCU and HMI in one board.

Base electrical rules:

- Common ground between JC board and roaster power/control board
- Keep noisy heater switching currents off the display power path
- Feed the JC board from a stable 5 V rail sized for the LCD backlight and ESP32-S3 current peaks
- Assume all JC GPIO are `3.3 V` only
- Verify the existing heater/fan interface stages accept `3.3 V` logic; add level shifting or a transistor stage if any input expects 5 V drive

## 3. Proposed first-pass GPIO assignment

This mapping preserves the current external devices while avoiding the known onboard display and touch pins.

| Function                                  | Current firmware | Proposed JC4827W543C GPIO | Notes                                       |
| ----------------------------------------- | ---------------- | ------------------------- | ------------------------------------------- |
| MAX6675 shared clock                      | `SCK` default    | `GPIO5`                   | Use software SPI or explicit pin assignment |
| MAX6675 shared data out                   | `MISO` default   | `GPIO46`                  | Input-only, good fit                        |
| Bean thermocouple CS                      | `TC1_CS = 10`    | `GPIO9`                   | MAX6675 CS on the same breakout as SCK/SO   |
| Fan/exhaust thermocouple CS               | `TC2_CS = 9`     | `GPIO14`                  | MAX6675 CS on the same breakout as SCK/SO   |
| Heater PWM output                         | `HEATER = A0`    | `GPIO6`                   | Output-group connector                      |
| PWM fan output                            | `FAN = A1`       | `GPIO7`                   | Output-group connector                      |
| BDC fan servo output                      | `BDCFAN = D5`    | `GPIO15`                  | Output-group connector                      |
| Optional spare / estop input / lid switch | not present      | `GPIO16`                  | Adjacent spare I/O on the output connector  |
| Optional service UART TX                  | not present      | `GPIO17`                  | Useful for debug or external peripheral     |
| Optional service UART RX                  | not present      | `GPIO18`                  | Useful for debug or external peripheral     |

Notes:

- If the MAX6675 library depends on default SPI macros for the new board, force explicit pin configuration rather than relying on board defaults.
- If `ESP32Servo` proves awkward on `GPIO15`, replace it with direct `ledcWrite` or MCPWM-based pulse generation.
- Do not use `GPIO3`, `GPIO4`, `GPIO8`, or `GPIO38`; they are already consumed by the onboard GT911 touch path in the reference design.
- Do not plan around `GPIO10-13` unless you intentionally give up the onboard SD function and confirm those traces on the real board.

## 4. Harness and PCB implications

Minimum hardware change if reusing the current roaster control PCB:

- Make an adapter harness from the JC4827W543C breakout headers to the existing control board header
- Reassign the old Nano pin labels to the new S3 GPIO in firmware and in a harness table
- Add mounting and strain relief for the display board since it is now both controller and HMI

If spinning a new controller PCB:

- Route only low-voltage I/O to the JC board headers
- Keep heater power switching physically separated from the display board and touch flex
- Add a dedicated connector for the JC board with keyed power, ground, and labeled signal pins
- Add an optional hardware estop or interlock input to one spare GPIO

## Firmware Port Plan

## Phase 0: Freeze current behavior

Before changing architecture, capture the UI behavior that exists today.

Create a checklist for:

- Idle startup state
- Roast start path
- Roast stop path
- Cooling completion path
- Error state path
- WiFi credential update path
- Active profile load and plot path
- Final target override path

The goal is to compare the new LVGL UI against these behaviors, not against the old visuals.

## Phase 1: Board bring-up on JC4827W543C

Create a hardware-only bring-up branch that does not yet port the roaster UI.

Tasks:

- Change compile target from the current Nano board to `ESP32S3 Dev Module`
- Add dependencies:
  - `lvgl`
  - `GFX Library for Arduino`
  - `Dev Device Pins`
  - `TAMC_GT911`
- Bring up LCD, backlight, and touch using the same basic approach as the reference project
- Prove that `lv_timer_handler()` can run alongside:
  - `SimpleTimer`
  - Async web server
  - OTA
  - existing control loop timers
- Verify heap headroom with WiFi, web UI, and LVGL enabled together

Deliverable:

- A simple debug screen showing current temperature, state, and one touch button

## Phase 2: Extract a UI-independent model

This is the main refactor.

Introduce plain data structures for what the UI needs:

- `DisplayTelemetry`
  - `roasterState`
  - `currentTemp`
  - `fanTemp`
  - `setpointTemp`
  - `setpointFanPercent`
  - `setpointProgress`
  - `heaterOutput`
  - `bdcFanMs`
  - `wifiConnected`
  - `ipAddress`
  - `activeProfileName`
  - `finalTargetTemp`
  - `faultMessage`
- `DisplayEvents`
  - button taps
  - keyboard submit events
  - profile selection events
  - settings save events

Files likely to change first:

- `roaster-firmware.ino`
- `ProfileEditor.hpp`
- `Network.hpp`

Refactor rules:

- No direct LVGL calls from the roast state machine
- No direct reads from widgets inside control logic
- No blocking `delay()` calls in the display path
- UI receives state snapshots; firmware receives user intent events

## Phase 3: Replace Nextion flows one by one

Port the current Nextion behaviors into LVGL scenes in this order.

### 3.1 Start / idle flow

Replace:

- `page Start`
- `globals.setTempNum.val`
- profile summary text

New LVGL behavior:

- Idle dashboard screen
- current active profile name
- editable final target temperature
- start roast button
- quick navigation to profiles, WiFi, diagnostics

### 3.2 Roast run flow

Replace:

- `page Roasting`
- current temp and target temp number updates
- progress and fan setpoint numeric updates

New LVGL behavior:

- Roast live screen with prominent current temperature
- target temperature and live delta
- progress bar and elapsed time
- fan percentage and heater output
- stop button with confirm dialog

### 3.3 Cooling flow

Replace:

- `page Cooling`
- cooling target display
- stop cooling trigger

New LVGL behavior:

- Cooling screen with large current temperature
- cooling target and estimated completion indicator
- fan status indicators
- end cooling button with confirmation

### 3.4 Error flow

Replace:

- `page Error`
- `Error.message.txt`

New LVGL behavior:

- Full-screen fault banner
- plain-language fault title
- key live values at time of fault
- required operator action text
- optional reset/reboot action only if safe and explicitly desired

### 3.5 Profile visualization flow

Replace:

- `page ProfileActive`
- waveform plotting by repeated `add 2,0,...` commands

New LVGL behavior:

- A native chart widget or custom canvas plot
- current active profile metadata
- setpoint list summary
- active final target override shown separately from stored profile target

The chart should be rendered from profile data in memory, not by replaying serialized commands.

## Phase 4: Add screens the new HMI can support well

The JC4827W543C can support a better navigation model than the current page-flip design.

Recommended screen set for phase 1 of the LVGL UI:

### Screen 1: Home

Purpose:

- Default idle screen

Content:

- Current bean temperature
- Current fan/exhaust temperature
- Active profile name
- Final target temperature editor
- WiFi status
- Main actions: Start, Profiles, Settings, Diagnostics

### Screen 2: Roast Live

Purpose:

- Primary in-process roast screen

Content:

- Large current temperature readout
- Target temperature readout
- Rate/progress strip
- Fan percent
- Heater output percent or PWM
- Current phase label: `START_ROAST`, `ROASTING`, `COOLING`
- Stop roast button

### Screen 3: Cooling

Purpose:

- Dedicated post-roast view

Content:

- Current temperature
- Cooling target temperature
- Cooling timer
- Fan status
- Finish cooling button

### Screen 4: Fault / Safety

Purpose:

- Safety-critical operator feedback

Content:

- Fault title: sensor failed, over temp, exhaust over temp, etc.
- Time of fault / current temperature
- Heater forced off status
- Fan forced on status
- Recovery instructions

### Screen 5: Profiles

Purpose:

- Browse and activate saved roast profiles

Content:

- Scrollable list of profiles
- Active badge
- Preview button
- Activate button
- Duplicate button
- Delete button for non-active profiles

### Screen 6: Profile Preview

Purpose:

- Replace `ProfileActive` page

Content:

- Profile name
- Duration
- Final target
- Chart of temperature and optionally fan over time
- Activate button if not active
- Back button

### Screen 7: Profile Edit Lite

Purpose:

- Minimal onboard editing without recreating the entire web editor

Recommended scope for first pass:

- Rename profile
- Adjust final target override
- Duplicate existing profile
- Maybe edit a small number of setpoints

Recommendation:

- Keep full profile authoring on the existing web UI initially. A full touch editor is possible, but it will consume time better spent on stability and safety.

### Screen 8: WiFi / Network

Purpose:

- Replace current credential entry flow

Content:

- SSID entry
- Password entry
- Connect button
- IP address
- mDNS name
- Connection result/status text

Implementation note:

- On-screen keyboard support is required here. LVGL can do this cleanly.

### Screen 9: Diagnostics

Purpose:

- Service and debug information on-device

Content:

- Current state
- Heater output
- Fan outputs
- PID gains in effect
- Bad thermocouple reading counter
- Heap / uptime
- Firmware version

### Screen 10: Tuning / Validation

Purpose:

- Expose the newer tuning work already present in firmware

Content:

- Start step-response tuning
- Validation roast entry point
- Latest tuning summary
- Current banded PID status

This can stay lower priority than core roast operation and safety views.

## LVGL Architecture Recommendations

## 1. Split model, view, and input

- `DisplayModel.hpp` for typed state snapshots
- `DisplayController.hpp/.cpp` for screen orchestration
- `screens/` directory for each LVGL screen
- `widgets/` directory for reusable cards, gauges, and dialogs

## 2. Use screen-local update functions

Example pattern:

- `homeScreen.show()`
- `homeScreen.update(const DisplayTelemetry&)`
- `roastScreen.update(const DisplayTelemetry&)`
- `faultScreen.setFault(...)`

Avoid a giant `switch` that manually touches every widget on every cycle.

## 3. Keep LVGL work bounded

- Run `lv_timer_handler()` from the main loop at about `5 ms`
- Refresh high-rate values at a lower UI cadence, for example `100-250 ms`
- Only redraw charts when data changes materially
- Use partial render buffers as in the reference project
- Prefer internal RAM for the draw buffer; only use PSRAM deliberately after measuring latency

## 4. Avoid blocking UI patterns that already caused problems with Nextion

Current code includes blocking UI calls and delays around waveform/page updates. Do not carry those forward.

Specific targets to eliminate:

- `delay(50)` / `delay(100)` around page changes
- synchronous widget reads inside the roast start path
- page transitions as a side effect of lower-level profile storage code

## Build and Dependency Changes

## Remove or retire

- `EasyNextionLibrary`
- Nextion trigger model
- UART transport used only for display

## Add

- `lvgl`
- `GFX Library for Arduino`
- `Dev Device Pins`
- `TAMC_GT911`

## Likely code organization additions

- `DisplayTypes.hpp`
- `DisplayController.hpp`
- `DisplayController.cpp`
- `display/BoardPins.hpp`
- `display/LvglPort.cpp`
- `display/screens/HomeScreen.*`
- `display/screens/RoastScreen.*`
- `display/screens/CoolingScreen.*`
- `display/screens/FaultScreen.*`
- `display/screens/ProfileListScreen.*`
- `display/screens/ProfilePreviewScreen.*`
- `display/screens/SettingsScreen.*`

## Firmware Touch List

These are the code areas most likely to change during the port.

### `roaster-firmware.ino`

Changes:

- remove `EasyNex myNex(HW_SERIAL)`
- remove `readNextionWithRetry()`
- replace direct page/number writes with display adapter calls
- replace `trigger0-4()` with typed UI event handlers
- add LVGL tick and display controller service call
- remap GPIO definitions for ESP32-S3 target

### `ProfileEditor.hpp`

Changes:

- remove direct plotting and page navigation responsibility
- return profile plot data or profile summaries to the display layer instead
- keep storage and serialization logic, not screen control

### `Network.hpp`

Changes:

- remove direct Nextion updates from profile activation path
- optionally expose the same telemetry/profile data model used by the LVGL UI

### `tests/`

Changes:

- keep existing control and profile tests board-agnostic where possible
- add smoke tests or at least compile coverage for the display abstraction layer

## Validation Plan

## Hardware bring-up validation

- Display initializes consistently after cold boot
- Touch coordinates are correct in the chosen rotation
- WiFi + web UI + LVGL coexist without watchdog resets
- Thermocouple reads remain stable with display active
- PWM heater/fan outputs remain on expected pins after remap

## Safety validation

- Sensor disconnect still forces `ERROR`
- Over-temperature still forces `ERROR`
- Exhaust over-temperature still forces `ERROR`
- Heater is off during idle, fault, and cooling as expected
- UI freezes do not stall the control loop or safety checks

## Functional validation

- Start roast from the new Home screen
- Confirm target override is applied correctly
- Confirm cooling flow can be stopped correctly
- Confirm active profile selection updates both UI and control logic
- Confirm WiFi credentials can be entered and saved
- Confirm OTA and web profile editing still work

## Suggested Implementation Order

1. Bring up JC4827W543C display, touch, and a dummy LVGL screen.
2. Remap roaster I/O on the S3 and prove sensors, heater, and fans work without UI changes.
3. Introduce a display abstraction layer while still stubbing most UI actions.
4. Port the Home, Roast Live, Cooling, and Fault screens.
5. Port profile list and profile preview.
6. Port WiFi settings.
7. Add diagnostics and tuning screens.
8. Remove remaining Nextion dependencies.
9. Run hardware validation and safety regression tests.

## Recommendation On Scope

Do not try to port every Nextion behavior and also design a full touch-first profile editor in the first pass.

Best first release:

- Full roast control
- Full safety handling
- Profile selection
- Profile preview chart
- Final target override
- WiFi settings
- Diagnostics

Keep complex profile editing on the web UI until the LVGL base is stable.

## Open Questions Before Implementation

- Confirm the exact JC4827W543C revision and exported GPIO headers on the physical board.
- Confirm whether the current heater/fan interface stages accept `3.3 V` logic directly.
- Decide whether the onboard SD slot matters for this product. If not, some reserved pins may become negotiable, but that should be intentional.
- Decide whether to keep the web profile editor as the only authoring tool for v1 of the LVGL port.
- Decide whether to add a dedicated physical estop/interlock input while rewiring to the new controller.

## Bottom Line

This port is feasible, but the main job is not drawing LVGL widgets. The main job is separating the UI transport from the roast controller so the controller does not care whether the front end is Nextion, LVGL, or the web console.

If that separation is done first, the JC4827W543C port becomes a controlled migration instead of a high-risk rewrite.
