# DIY Hot Air Coffee Roaster

This repository is my latest attempt to make home coffee roasting simple, reliable, scientific, and affordable.

The tagged `v1` release preserves a legacy Nextion-based hardware design. This branch intentionally documents only the current controller, tooling, and firmware layout.

## Features

- PID temperature control with profile-based roast automation
- Dual MAX6675 thermocouple monitoring
- Integrated JC4827W543C touchscreen UI using LVGL
- Web console, REST API, and OTA firmware updates
- Artisan integration for roast logging
- Safety systems for over-temperature, sensor faults, and emergency shutdown

## Firmware Setup

For complete setup, build, and deployment instructions, see [roaster-firmware/README.md](roaster-firmware/README.md).

## Hardware Docs

Hardware assembly guidance and the project bill of materials should live at the repository root because they span the whole build, not just the firmware:

- [BUILD.md](BUILD.md) for the physical build and assembly workflow
- [BOM.md](BOM.md) for the parts list and sourcing notes

The tagged `v1` release remains the reference for the legacy hardware design. This branch should document only the current controller and any current-generation enclosure or PCB assets that are restored into the repository.

### Quick Start

```bash
git clone https://github.com/fredvisser/coffee-roaster.git
cd coffee-roaster/roaster-firmware
./tools/bootstrap.sh
./tools/firmware.sh build --board jc4827w543c
```

Canonical developer entrypoints:

- `./tools/bootstrap.sh`
- `./tools/firmware.sh`
- `./tools/tests.sh`

## Runtime Interfaces

- Web console: `http://roaster-dev.local/console`
- OTA update page: `http://roaster-dev.local/update`
- Profile editor: `http://roaster-dev.local/profile`
- PID workflow: `http://roaster-dev.local/pid`

## Roast Data and Artisan

The firmware can stream roast data to [Artisan](https://artisan-scope.org) over WebSockets. The repository includes an Artisan settings file in `Artisan/`.

## Firmware REST API (summary)

For full details see roaster-firmware/README.md. Key endpoints (id-based):

- GET `/api/profiles` → `{ profiles: [{ id, name, active }], active: id }`
- POST `/api/profiles` → Create profile `{ name, setpoints?, activate? }` → returns `{ ok, id, name, setpoints }`
- GET `/api/profiles/:id` → `{ id, name, setpoints, active? }`
- PUT `/api/profiles/:id` → Update `{ name, setpoints, activate? }` (id from path wins)
- POST `/api/profiles/:id/activate` → Activate profile
- DELETE `/api/profiles/:id` → Delete (409 if active)

Notes:
- Times are seconds in API/UI; firmware stores milliseconds.
- Temp bounds 0–500°F; Fan bounds 0–100%.
- Names are display-only; storage is keyed by opaque ids.

## Artisan

Send roast data wirelessly to [Artisan](https://artisan-scope.org) to visualize and record roasts.

### Configure Artisan

1. Download and install [Artisan](https://artisan-scope.org)
2. Configure the Artisan to connect to the Roaster via WebSockets. Apply the `Artisan/astisan-settings.aset` file (**Help»Load Settings…**) or manually configure the settings as documented below.
   1. Configure Port settings (**Config»Port…**)
      ![Artisan Port configuration dialog](./images/Artisan%20-%20Port%20Config.png)
   2. Configure Device settings (**Config»Device…**)
      ![Artisan Device configuration dialog](./images/Artisan%20-%20Device%20Config.png)
      ![Artisan Device configuration dialog 2](./images/Artisan%20-%20Device%20Config2.png)

### Run

1. Press the **ON** button to enable the Roaster connection. You should `WebSocket connected` along with the current roaster and setpoint temperatures.
   ![Artisan start roast screen](./images/Artisan%20-%20Start%20roast%20screen.png)
2. Press **Start Roast** on the Roaster and the recording should automatically start within Artisan.
