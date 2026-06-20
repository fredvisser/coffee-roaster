# Hardware Build Guide

Use this guide to assemble the current JC4827W543C-based coffee roaster. Use [BOM.md](BOM.md) for the parts list and sourcing links. Use [roaster-firmware/README.md](roaster-firmware/README.md) for firmware installation, OTA updates, and test workflows.

## Before You Start

This project combines mains voltage, high temperatures, and moving parts. Build it only if you are comfortable with basic electrical work and safe mains wiring practices.

- Never power the heater unattended.
- Keep mains wiring separated from low-voltage wiring.
- Use insulated connectors, heat shrink, and an enclosed mains inlet.
- Verify heater-off fail-safe behavior before the first roast.
- Keep a fire extinguisher nearby during bring-up and testing.

## Choose Your Fan Build

You need to decide which airflow build you are making before ordering parts or wiring the controller.

### Option A: Stock Popper Fan Motor

Choose this if you want the simplest mechanical build.

- Keep the stock popper fan motor.
- Use a 24 V power supply.
- Use the DC speed controller listed in [BOM.md](BOM.md).
- Connect the speed controller to the firmware-controlled fan PWM output.

### Option B: Custom Brushless Fan Conversion

Choose this if you want to replace the stock fan hardware.

- Print the replacement fan and fan mount in ASA.
- Use a 12 V power supply.
- Use the brushless DC motor and controller listed in [BOM.md](BOM.md).
- Treat the fan, mount, motor, controller, and 12 V supply as one matched system.

## What You Are Building

The current roaster uses:

- a JC4827W543C ESP32-S3 display board for the controller and touchscreen UI
- two MAX6675 thermocouple modules for bean and exhaust temperature
- one popcorn popper modified for separate heater and fan control
- one AC SSR in the heater path
- one main low-voltage supply, either 24 V or 12 V depending on the fan build

![Assembled roaster](images/v2-roaster-idle.jpeg)
![Assembled roaster internals](images/v2-wiring-complete.jpeg)

## Tools And Consumables

Have these on hand before starting:

- screwdrivers, hex keys, pliers, and flush cutters
- soldering iron and solder
- drill and bits for thermocouple and mounting holes
- heat shrink and crimp or splice connectors
- silicone-insulated wire for heater and fan rewiring
- super glue for the power switch and other light-duty mounting tasks
- fasteners and printed parts from [BOM.md](BOM.md)

## Step 1: Print The Parts

Print or obtain the enclosure parts before modifying the popper.

Print the parts from the `3D printing/` folder. The current filenames are:

- `Enclosure Bottom.stl`
- `Enclosure Top.stl`
- `Handle - Main.stl`
- `Handle - Trim.stl`
- `12V - Power Supply Mount.stl`
- `Breakout Mount.stl`
- `Feet.stl`
- `BDC Fan Mount.stl`
- `Fan.stl`
- `Roaster Enclosure v2.3mf`

Recommended materials:

- PLA for enclosure parts away from concentrated heat
- TPU for feet
- ASA for the custom fan and fan mount if you are building Option B

Use `Roaster Enclosure v2.3mf` as the assembled project file if you want to inspect the overall print set before slicing individual STL files.

![Roaster back view](images/Roaster - Back.jpeg)

![Roaster bottom view](images/Roaster - Bottom.jpeg)

Before moving on, test fit the display opening, mounting bosses, power supply mount, and wire-routing paths.

## Step 2: Build The Controller Hardware

Assemble the controller-side hardware next.

You will need:

- the JC4827W543C display board
- two MAX6675 modules
- the heater SSR
- the selected fan-control hardware for your build option
- the mains inlet and power switch
- the low-voltage power supply
- either the project breakout PCB or a perf-board equivalent

The breakout-board files currently in the repository are:

- `PCBs/Roaster Breakout/Roaster Breakout.sch`
- `PCBs/Roaster Breakout/Roaster Breakout.brd`
- `PCBs/Roaster Breakout/Breakout Perfboard.diy`

Mount the controller components so that:

- the display board is secure and accessible
- the thermocouple modules are reachable for wiring and service
- the heater control path is clearly separated from signal wiring
- the low-voltage supply and breakout hardware have adequate clearance

If you are making a breakout board on perf board, finish that assembly before starting the enclosure wiring.

![Roaster breakout PCB](images/v2-pcb.png)

![Roaster breakout schematic](images/v2-schematic.png)

![Perfboard breakout example](images/roaster-breakout-perfboard.png)

![Controller hardware layout](images/v2-wiring-in-process.jpeg)

## Step 3: Modify The Popcorn Popper

Modify the popper in this order.

1. Remove the screws from the bottom of the popper.
2. Lift the popper core out of the base.
3. Remove the stock fan-motor diode wiring and separate the heater connections.
4. Remove or bypass the stock thermal cutout behavior used for stand-alone popper operation.
5. Rewire the fan and heater for external control.
6. Drill the roasting chamber and install the thermocouples.
7. Reassemble the popper core into the base.
8. Route the modified wiring so it can exit cleanly into the enclosure.

![Popper disassembly step 1](images/popper-mod-1.jpeg)

![Popper disassembly step 1b](images/popper-mod-1b.jpeg)

![Popper disassembly step 2](images/popper-mod-2.jpeg)

![Popper disassembly step 3](images/popper-mod-3.jpeg)

![Popper disassembly step 4](images/popper-mod-4.jpeg)

![Popper rewiring step](images/popper-mod-5.jpeg)

Important details:

- Use the updated popper model listed in [BOM.md](BOM.md).
- Use the original popper lamp cord for outlet, switch, and power-supply wiring if it is in good condition.
- Use higher-temperature silicone wire for the motor and heater wiring.
- Keep thermocouple wiring away from switched heater and motor wiring to reduce noise.

### Thermocouple Preparation

Before installing the thermocouples, carefully cut about 3/16 in from the probe tip with a hacksaw so the thermocouple junction is exposed. This makes the probe more responsive to moving air inside the roasting chamber.

Mount the probe so the tip is in the process air stream, not buried inside a closed metal sleeve.

![Thermocouple installation](images/popper-mod-7.jpeg)

## Step 4: Assemble The Enclosure

Once the printed parts, controller hardware, and modified popper are ready, assemble the enclosure.

1. Install the feet, breakout mount, and power-supply mount.
2. Mount the popper base to the enclosure bottom.
3. Mount the display board, breakout hardware, SSR, power supply, and thermocouple modules.
4. Mount the mains inlet and power switch.
5. Fit the handle and trim pieces.

![Mount the popper base](images/mount-popcorn-base-1.jpeg)

![Mount the popper base detail](images/mount-popcorn-base-2.jpeg)

![Assembly layout reference](images/v2-wiring-in-process.jpeg)

Power switch note:

- Clip off the flexible plastic tabs on the switch before installation.
- Super glue the switch to the case instead of forcing the tabs into the printed wall.
- This reduces stress on the enclosure and makes the switch easier to mount cleanly.

Before continuing, confirm that the internal components clear the enclosure walls and that the popper mounting hardware does not interfere with the controller hardware.

## Step 5: Wire The Roaster

Wire the system carefully and deliberately.

1. Run mains wiring for the inlet, switch, heater SSR, and power supply.
2. Run low-voltage wiring for the controller, thermocouple modules, and fan-control hardware.
3. Connect the heater through the AC SSR.
4. Connect the fan-control path for your chosen build option.
5. Connect both thermocouples to their MAX6675 modules.
6. Connect the JC4827W543C board I/O using the harness parts listed in [BOM.md](BOM.md).
7. Secure and strain-relieve all wiring before closing the enclosure.

![Annotated wiring reference](images/Annotated wires.jpeg)

![Wiring reference 1](images/wiring-1.jpeg)

![Wiring reference 2](images/wiring-2.jpeg)

![Wiring reference 3](images/wiring-3.jpeg)

![Wiring reference 4](images/wiring-4.jpeg)

![Wiring reference 5](images/wiring-5.jpeg)

![Completed wiring reference](images/v2-wiring-complete.jpeg)

Wiring rules:

- Keep mains wiring separate from thermocouple and logic wiring.
- Use heat shrink or insulated splice connectors on every exposed splice.
- Use M3 fasteners as the standard hardware wherever practical.
- Use M4 hardware where needed for the popper base mounting.

Before applying power, verify:

- no exposed mains conductors
- correct heater routing through the SSR
- correct fan wiring for the chosen motor option
- thermocouples connected to the expected bean and exhaust inputs
- low-voltage polarity is correct throughout the controller wiring

## Step 6: Install Firmware

After the hardware is complete:

1. Go to `roaster-firmware/`.
2. Run `./tools/bootstrap.sh` if you have not prepared the environment yet.
3. Build with `./tools/firmware.sh build --board jc4827w543c`.
4. Upload with `./tools/firmware.sh upload --board jc4827w543c` or use OTA if the unit is already provisioned.
5. Open the local UI and web console.

## Step 7: First Power-Up And Validation

Perform the first power-up with the heater disabled or closely supervised.

Check the following before roasting coffee:

- the display boots cleanly
- WiFi setup works
- both temperatures look plausible at room temperature
- heater output changes only when commanded
- fan outputs respond correctly for the chosen motor option
- emergency and error states force the heater off
- cooldown behavior works and can be observed from the UI or web console

Do not start roasting until all of those checks pass.

![Assembled roaster ready for validation](images/v2-roasting.jpeg)

## Troubleshooting Notes

- If temperatures read erratically, inspect thermocouple routing first and move the probe leads farther away from heater and motor wiring.
- If the switch is difficult to fit, confirm the plastic tabs were removed before gluing it into the enclosure.
- If the fan does not respond as expected, confirm that you wired the correct control hardware for Option A or Option B.

## Related Files

- [BOM.md](BOM.md) for sourcing links and part quantities
- [roaster-firmware/README.md](roaster-firmware/README.md) for firmware workflows