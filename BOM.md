# Bill of Materials

This document is the home for the parts list for the current coffee roaster design.

Keep the BOM separate from the build guide so that:

- part substitutions can be updated without rewriting assembly steps
- sourcing links can change independently of the build procedure
- the physical build flow in [BUILD.md](BUILD.md) stays readable

## Scope

This file describes only the current branch hardware. Legacy parts lists should stay with the tagged `v1` release.

## Build Variants

There are two valid airflow build options for this design.

### Option A: Stock Popper Fan Motor

Use the stock popper fan motor when you want the simplest mechanical build.

- Use a 24 V power supply.
- Use a DC speed controller in the fan power path.
- Connect that speed-control path to the firmware-controlled fan PWM output.

### Option B: Custom Brushless Fan Conversion

Use the custom fan path when replacing the stock motor and fan hardware.

- Print the replacement fan and fan mount in ASA.
- Use a 12 V power supply.
- Use a brushless DC motor and matching controller.
- Treat the custom fan hardware, motor, controller, and 12 V supply as a matched set.

The tables below call out which parts apply to both options and which ones are specific to only one variant.

## Controller And Display Hardware

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| JC4827W543C board | 1 | ESP32-S3 display controller with integrated LCD and touch | Main controller and local UI | [Amazon display listing](https://www.amazon.com/ESP32-S3-Bluetooth-Compatible-Micropython-Capacitive/dp/B0FW9H5YZY/) | [AliExpress display listing](https://www.aliexpress.com/item/3256806543063048.html?spm=a2g0o.order_list.order_list_main.11.2caa1802G17NYU) | Replaces the old Nano plus Nextion split |
| Roaster Breakout PCB | 1 | Breakout board from `PCBs/Roaster Breakout/` | Wiring distribution and mounting | N/A | N/A | Use `Roaster Breakout.brd` and `Roaster Breakout.sch`, or build the breakout on perf board instead |
| Perf board fallback | 1 | Generic through-hole prototyping board | Manual breakout-board build option | [Perf board example](https://www.amazon.com/dp/B072Z7Y19F?ref=ppx_yo2ov_dt_b_fed_asin_title) | TBD | Use this only if not ordering or fabricating a dedicated breakout PCB |
| MAX6675 thermocouple module | 2 | K-type thermocouple amplifier module | Bean and exhaust sensing | TBD | TBD | Firmware expects two channels |
| JC IO harness parts | Assorted | 4-pin 1.25 mm board-side and harness-side connector parts | Connects to JC4827W543C I/O | [4-pin JST 1.25 mm connectors](https://www.amazon.com/dp/B0DS3MCBBK?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) and [JST GH housing and terminal kit](https://www.amazon.com/CHENBO-Connector-Housing-Assortment-Terminal/dp/B077X8XV2J/) or [pre-crimped JST GH to Dupont leads](https://www.amazon.com/XUGERIP-JST-GH-Dupont-Pre-Crimped/dp/B0CW6J3NXM) | TBD | Use whichever harness approach best matches the current breakout and service plan |

## Power Electronics

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| 24 V power supply | 1 | Main low-voltage supply | Required for the stock popper fan motor build | TBD | [AliExpress power supply listing](https://www.aliexpress.us/item/3256805366654681.html) | Option A only |
| 12 V power supply | 1 | Main low-voltage supply for custom fan conversion | Powers the brushless fan path | [12 V power supply](https://www.amazon.com/dp/B078RY6YY3?ref=ppx_yo2ov_dt_b_fed_asin_title) | [AliExpress power supply listing](https://www.aliexpress.us/item/3256805366654681.html) | Option B only; confirm the ordered voltage and current variant |
| DC-DC converter or regulator | 1 | Secondary low-voltage conversion stage | Supplies any required rail below the main supply voltage | TBD | TBD | Keep exact part aligned with the restored current board design |
| Heater SSR | 1 | AC-output solid-state relay | Controls heater power | [SSR-25DA example](https://www.amazon.com/SSR-25DA-3-32VDC-Output-24-480VAC-Plastic/dp/B08GPJ1V2J) | [SSR example](https://www.aliexpress.us/item/2255800713623525.html) | Must be correctly rated for mains and heater load |
| DC speed controller for stock fan motor | 1 | MOSFET-based DC motor speed controller | Controls the stock popper fan motor path from the PWM-controlled output | [DC speed controller](https://www.amazon.com/Anmbest-High-Power-Adjustment-Electronic-Brightness/dp/B07NWD8W26) | [AliExpress DC speed controller search](https://www.aliexpress.us/w/wholesale-dc-speed-MOSFET.html?spm=a2g0o.productlist.search.0) | Option A only |
| Power inlet or socket assembly | 1 | Enclosed mains input | Safe power entry | [Power inlet](https://www.amazon.com/dp/B07PVP8CLT?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_31&th=1) | TBD | The popper lamp cord can be reused for outlet, switch, and power-supply wiring |
| Power switch | 1 | Panel-style rocker or toggle switch | Main power control | [Power switch](https://www.amazon.com/dp/B09YCG3NM1?ref=ppx_yo2ov_dt_b_fed_asin_title) | TBD | Clip off the flexible plastic tabs and super glue the switch to the case to avoid overstressing the printed enclosure |

## Sensors And Wiring

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| K-type thermocouple | 2 | Temperature probes | Bean and exhaust temperature measurement | [Thermocouples](https://www.amazon.com/dp/B092ZCSM7J?ref=ppx_yo2ov_dt_b_fed_asin_title) | [AliExpress thermocouples](https://www.aliexpress.us/item/3256806044229155.html) | Carefully hacksaw roughly 3/16 in off the tip to expose the junction so the probe responds faster to moving air |
| Silicone wire | Assorted | High-temp flexible wire | Heater and fan rewiring | [Silicone wire example](https://www.amazon.com/dp/B07FMLVF84?ref=ppx_yo2ov_dt_b_fed_asin_title) | [Silicone wire example](https://www.aliexpress.us/item/2255800441309579.html) | Use the popper lamp cord for outlet, switch, and supply wiring; use higher-temp silicone wire for motor and heater wiring |
| JST XH 2.54 connector kit | Assorted | Locking low-voltage connectors | Internal wiring harnesses away from the JC board I/O header | [JST XH connector kit](https://www.amazon.com/gp/product/B09DBGVX5C) | TBD | Adjust counts once the current PCB set is fixed |
| Splice or crimp connectors | Assorted | Inline electrical splice terminals | Joins motor, heater, and supply wiring | [Lever splice connectors](https://www.amazon.com/dp/B0CJ5QF3VX?ref=ppx_yo2ov_dt_b_fed_asin_title) and [crimp connector assortment](https://www.amazon.com/Feggizuli-Connectors-Connector-Terminals-Electrical/dp/B0B4H54KPS) | TBD | Use these or equivalent crimp connectors where removable or insulated splices are needed |
| Heat shrink tubing | Assorted | Wire insulation and strain relief | Safer harness construction | TBD | TBD | Use on all splices and terminations where practical |

## Airflow And Load Hardware

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| 1200 W popcorn popper | 1 | Base roasting chamber and blower assembly | Core roasting hardware | [Updated popper](https://www.amazon.com/dp/B0FKBZH5P5?ref=ppx_yo2ov_dt_b_fed_asin_title) | TBD | Modified for external heater and fan control |
| Stock popper fan motor | 1 | Original motor retained from the popper | Airflow source | Included with popper | N/A | Option A only |
| Brushless DC motor and controller | 1 set | Replacement motor and matched driver | Airflow source for custom fan conversion | [Brushless motor and controller](https://www.amazon.com/dp/B075ZSDR2T?ref=ppx_yo2ov_dt_b_fed_asin_title) | TBD | Option B only |

## Mechanical And Printed Parts

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| Enclosure Bottom.stl | 1 | Main lower enclosure body | Supports popper base and internal hardware | N/A | N/A | Current filename in `3D printing/` |
| Enclosure Top.stl | 1 | Upper enclosure shell | Covers and protects internal hardware | N/A | N/A | Current filename in `3D printing/` |
| Handle - Main.stl | 1 | Main carrying handle | Mechanical handling | N/A | N/A | Current filename in `3D printing/` |
| Handle - Trim.stl | 1 | Cosmetic or fit trim for handle | Finish and fit | N/A | N/A | Current filename in `3D printing/` |
| 12V - Power Supply Mount.stl | 1 | Printed mount for the power supply | Internal mechanical support | N/A | N/A | Current filename in `3D printing/`; verify fit for the selected supply |
| Breakout Mount.stl | 1 | Printed mount for the breakout board | Supports controller-side hardware | N/A | N/A | Current filename in `3D printing/` |
| Feet.stl | 1 set | Printed feet | Stability and surface protection | N/A | N/A | Current filename in `3D printing/`; TPU is still a good default |
| BDC Fan Mount.stl | 1 | Printed fan mount for the custom airflow option | Supports the brushless motor conversion | N/A | N/A | Option B only |
| Fan.stl | 1 | Printed fan for the custom airflow option | Airflow hardware for the brushless conversion | N/A | N/A | Option B only; print in ASA |
| Roaster Enclosure v2.3mf | 1 | Combined enclosure project file | Print preparation and fit reference | N/A | N/A | Use this to inspect the assembled print set before slicing |
| Roaster Breakout.brd and Roaster Breakout.sch | 1 set | Current breakout board design files | Board fabrication and reference | N/A | N/A | Located in `PCBs/Roaster Breakout/` |

## Fasteners And Consumables

| Item | Qty | Description | Role | Amazon | AliExpress | Notes |
| --- | ---: | --- | --- | --- | --- | --- |
| M3 fastener kit | 1 set | Assorted M3 screws, nuts, and standoffs | Standard enclosure and electronics fasteners | [M3 fastener set](https://www.amazon.com/dp/B097Y6KCVW?ref_=ppx_hzsearch_conn_dt_b_fed_asin_title_3) | TBD | Use M3 hardware as the standard fastener set wherever practical |
| M4 nuts and bolts | Assorted | Popper base and enclosure mounting hardware | Mechanical attachment of the popper assembly | TBD | TBD | Legacy assembly used M4 hardware through the enclosure bottom |
| Heat shrink and cable management | Assorted | Insulation and strain relief | Safer wiring | TBD | TBD | Strongly recommended |
| Super glue or equivalent adhesive | As needed | Adhesive for low-load mounting tasks | Switch and trim attachment | TBD | TBD | Use to mount the power switch after clipping the plastic tabs; use only where heat exposure is acceptable |

## Remaining Gaps

The core v2 parts list is in place, but a few sourcing fields still need confirmation before this BOM is fully complete:

- remaining Amazon and AliExpress links still marked `TBD`
- final quantities for connectors, heat shrink, and some fasteners
- any additional board-fabrication outputs you want listed beyond `Roaster Breakout.brd` and `Roaster Breakout.sch`