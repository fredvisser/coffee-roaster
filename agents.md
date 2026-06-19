# Agent Guidelines for Coffee Roaster Project

## Project Overview

This repository targets the current JC4827W543C-based coffee roaster controller with:
- MAX6675 thermocouple monitoring
- PID-controlled heating
- PWM and BDC fan control
- LVGL-based touchscreen UI
- WiFi, OTA, and web tooling

## Primary Entry Points

```bash
cd roaster-firmware

./tools/bootstrap.sh
./tools/firmware.sh build --board jc4827w543c
./tools/firmware.sh upload --board jc4827w543c
./tools/firmware.sh ota --board jc4827w543c
./tools/tests.sh list
./tools/tests.sh run safety --board jc4827w543c
./tools/lvgl-sim.sh run start
```

## Build Notes

- The supported firmware target in this branch is `jc4827w543c`.
- The canonical compile and upload surface is the `tools/` wrapper scripts.
- OTA requires ElegantOTA async mode plus AsyncTCP and ESPAsyncWebServer.

## Safety Expectations

- Treat heater, fan, thermocouple, and error-state changes as safety-critical.
- Preserve fail-safe behavior: heater off, cooling available, explicit error latching.
- Prefer focused validation after edits: safety tests or a JC firmware build.

## Testing

- Use `./tools/tests.sh` for named suites.
- Use `./tools/firmware.sh build --board jc4827w543c` as the baseline firmware validation.
- Use the LVGL simulator for UI work when hardware is unavailable.
