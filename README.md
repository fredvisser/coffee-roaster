# DIY Hot Air Coffee Roaster

I've been building controllers for modified hot air popcorn poppers for many years. This repository is my latest attempt to make home coffee roasting simple, reliable, scientific, and affordable.

## Configuration

1. This project uses PlatformIO. Download the recommended VSCode extension to build and deploy the project.
2. Create a `include/settings.h` file with the following to configure your WiFi information.

```h
#define SSID "<SSID>"
#define PASSWORD "<WIFI_PASSWORD>"
```

3. The Roaster's IP address is recorded in the Serial console when the device boots. Make note of this value.

## Artisan Roaster Scope

You can use the [Artisan Roaster Scope](https://artisan-scope.org) to view and record roast profiles.

### Configure

1. Download and install [Artisan](https://artisan-scope.org)
2. Configure the Artisan to connect to the Roaster via WebSockets.
    ![Artisan Port configuration dialog](./images/Artisan%20-%20Port%20Config.png)
    ![Artisan Device configuration dialog](./images/Artisan%20-%20Device%20Config.png)

### Run

1. Press the **ON** button to enable the Roaster connection. You should `WebSocket connected` along with the current roaster temperature.
    ![Artisan start roast screen](./images/Artisan%20-%20Start%20roast%20screen.png)
2. Press **Start Roast** on the Roaster and the recording should automatically start within Artisan.