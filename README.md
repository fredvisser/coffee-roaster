# DIY Hot Air Coffee Roaster

I've been building controllers for modified hot air popcorn poppers for many years. This repository is my latest attempt to make home coffee roasting simple, reliable, scientific, and affordable.

## Features

- Control both temperature and fan level during roast with user defined setpoints.
- PID for precise temperature control.
- Send roast data wirelessly to [Artisan](https://artisan-scope.org) to visualize and record roasts.
- Uses commercial-off-the-shelf components.
- Project includes 3D printed enclosure designs, custom PCB, and build instructions (coming soon).

## Build

### Case

The case is modeled in [OnShape](https://cad.onshape.com/documents/ee717da6534241d41072245e/w/709854c2acbee5e38b890bec/e/be6e653fbb47fc1f3247d478?renderMode=0&uiState=65b72c2e65c4bf7511b63553). 

Print the following components. PLA works well.
- Enclosure Botton
- Enclosure Top
- PCB Standoff
- Handle
- Power Supply Mount
- Feet (TPU)

Order the following: 
- Screws [AliExpress](https://www.aliexpress.com/item/3256802230982244.html?spm=a2g0o.order_list.order_list_main.28.174d1802Nk9p7q)

### Electronics

The roaster controller consists of a Nextion display paired with a simple interface board containing an Arduino Nano ESP32, fan speed controller, and pin headers.

Part list:
- Order the PCB (`PCBs/Roaster Control Rev I`) [PCBWay](https://pcbway.com) [AllPCB](https://www.allpcb.com) [JLCPCB](https://jlcpcb.com)
- Nextion 3.5" Discovery Series [AliExpress](https://www.aliexpress.us/item/3256803271061345.html) [Amazon](https://www.amazon.com/gp/product/B0BBLB54XM)
- Arduino Nano ESP32 [Amazon](https://www.amazon.com/dp/B0C947BHK5)
- 1200W Popcorn Popper [Amazon](https://www.amazon.com/gp/product/B091GGYCQW)
- 24v Power Supply [Amazon](https://www.amazon.com/gp/product/B018RE4CWW)
- Power Plug Socket [Amazon](https://www.amazon.com/gp/product/B07PVP8CLT)
- DC-DC Power Supply [Amazon](https://www.amazon.com/gp/product/B01MQGMOKI)
- Motor Dimmer module [AliExpress](https://www.aliexpress.com/item/2251832615710334.html)
- JST XH2.54 Connector kit [Amazon](https://www.amazon.com/gp/product/B09DBGVX5C)
- Silicon Wire [AliExpress](https://www.aliexpress.us/item/2255800441309579.html)
- Solid-State Relay (SSR) [AliExpress](https://www.aliexpress.us/item/2255800713623525.html)

## Firmware Installation

1. This project uses PlatformIO. Download the recommended VSCode extension to build and deploy the project to the Arduino Nano ESP32.
2. Copy the `Nextion/roaster1.tft` file to a microSD card and insert into the Nextion display before powering to install the display firmware.

## Software Configuration
3. (Optional) Configure WiFi
     1. Navigate to **Settings»WiFi** and enter your WiFi credentials. 
     2. Make note of the roaster IP address that appears.

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