#!/bin/bash
# Coffee Roaster - Library Setup Script
# This script installs required Arduino libraries and configures them for the project

set -e  # Exit on error

echo "=========================================="
echo "Coffee Roaster - Library Setup"
echo "=========================================="
echo ""

# Check if arduino-cli is installed
if ! command -v arduino-cli &> /dev/null; then
    echo "❌ Error: arduino-cli is not installed"
    echo "Install it with: brew install arduino-cli"
    echo "Or download from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

echo "✓ arduino-cli found: $(arduino-cli version)"
echo ""

# Ensure ESP32 core is installed
echo "📦 Checking ESP32 core..."
if arduino-cli core list | grep -q "esp32:esp32"; then
    INSTALLED_VERSION=$(arduino-cli core list | grep "esp32:esp32" | awk '{print $2}')
    echo "✓ ESP32 core installed: $INSTALLED_VERSION"
    if [ "$INSTALLED_VERSION" != "3.3.3" ]; then
        echo "⚠️  Warning: Recommended version is 3.3.3, you have $INSTALLED_VERSION"
        read -p "Update to 3.3.3? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            arduino-cli core install esp32:esp32@3.3.3
        fi
    fi
else
    echo "Installing ESP32 core 3.3.3..."
    arduino-cli core install esp32:esp32@3.3.3
fi
echo ""

# Install required libraries
echo "📚 Installing required libraries..."
echo ""

LIBRARIES=(
    "EasyNextionLibrary"
    "MAX6675 library"
    "SimpleTimer"
    "AutoPID"
    "ESP32Servo"
    "ElegantOTA@3.1.7"
    "ArduinoJson"
)

for lib in "${LIBRARIES[@]}"; do
    echo "Installing $lib..."
    arduino-cli lib install "$lib" || echo "⚠️  $lib may already be installed"
done

echo ""
echo "📥 Installing ESPAsyncWebServer and AsyncTCP..."
echo "Note: These libraries must be installed manually from GitHub"

# Check if libraries directory exists
ARDUINO_LIB_DIR="$HOME/Documents/Arduino/libraries"
if [ ! -d "$ARDUINO_LIB_DIR" ]; then
    mkdir -p "$ARDUINO_LIB_DIR"
fi

# Install AsyncTCP if not present
if [ ! -d "$ARDUINO_LIB_DIR/Async_TCP" ] && [ ! -d "$ARDUINO_LIB_DIR/AsyncTCP" ]; then
    echo "Downloading AsyncTCP from GitHub..."
    cd "$ARDUINO_LIB_DIR"
    git clone https://github.com/ESP32Async/AsyncTCP.git Async_TCP
    echo "✓ AsyncTCP installed"
else
    echo "✓ AsyncTCP already installed"
fi

# Install ESPAsyncWebServer if not present
if [ ! -d "$ARDUINO_LIB_DIR/ESP_Async_WebServer" ] && [ ! -d "$ARDUINO_LIB_DIR/ESPAsyncWebServer" ]; then
    echo "Downloading ESPAsyncWebServer from GitHub..."
    cd "$ARDUINO_LIB_DIR"
    git clone https://github.com/ESP32Async/ESPAsyncWebServer.git ESP_Async_WebServer
    echo "✓ ESPAsyncWebServer installed"
else
    echo "✓ ESPAsyncWebServer already installed"
fi

echo ""
echo "⚙️  Configuring ElegantOTA for async mode..."

# Find ElegantOTA.h file
ELEGANTOTA_H="$ARDUINO_LIB_DIR/ElegantOTA/src/ElegantOTA.h"

if [ -f "$ELEGANTOTA_H" ]; then
    # Check if already configured
    if grep -q "#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1" "$ELEGANTOTA_H"; then
        echo "✓ ElegantOTA already configured for async mode"
    else
        echo "Enabling async mode in ElegantOTA..."
        # Backup original file
        cp "$ELEGANTOTA_H" "$ELEGANTOTA_H.bak"
        # Enable async mode
        sed -i '' 's/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1/' "$ELEGANTOTA_H"
        
        # Verify the change
        if grep -q "#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1" "$ELEGANTOTA_H"; then
            echo "✓ ElegantOTA configured for async mode"
            echo "  Backup saved to: $ELEGANTOTA_H.bak"
        else
            echo "❌ Error: Failed to configure ElegantOTA"
            echo "  Please manually edit: $ELEGANTOTA_H"
            echo "  Change: #define ELEGANTOTA_USE_ASYNC_WEBSERVER 0"
            echo "  To:     #define ELEGANTOTA_USE_ASYNC_WEBSERVER 1"
            exit 1
        fi
    fi
else
    echo "❌ Error: ElegantOTA.h not found at $ELEGANTOTA_H"
    echo "Make sure ElegantOTA library is installed"
    exit 1
fi

echo ""
echo "=========================================="
echo "✅ Library setup complete!"
echo "=========================================="
echo ""
echo "Next steps:"
echo "1. Compile firmware: arduino-cli compile --fqbn esp32:esp32:nano_nora roaster-firmware/roaster-firmware.ino"
echo "2. Upload to device: arduino-cli upload -p /dev/cu.usbserial-* --fqbn esp32:esp32:nano_nora roaster-firmware/roaster-firmware.ino"
echo ""
echo "For OTA updates after initial flash, see OTA_SETUP.md"
echo ""
