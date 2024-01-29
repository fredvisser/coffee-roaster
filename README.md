# DIY Hot Air Coffee Roaster

I've been building controllers for modified hot air popcorn poppers for many years. This repository is my latest attempt to make home coffee roasting simple, reliable, scientific, and affordable.

## Features

- Control both temperature and fan level during roast with user defined setpoints.
- PID for precise temperature control.
- Send roast data wirelessly to [Artisan](https://artisan-scope.org) to visualize and record roasts.
- Uses commercial-off-the-shelf components.
- Project includes 3D printed enclosure designs, custom PCB, and build instructions (coming soon).

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