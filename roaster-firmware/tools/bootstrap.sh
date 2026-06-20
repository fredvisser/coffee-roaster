#!/bin/bash
# Coffee Roaster dependency bootstrap script.

set -e

echo "=========================================="
echo "Coffee Roaster - Library Setup"
echo "=========================================="
echo ""

RECOMMENDED_ESP32_CORE="${ESP32_CORE_VERSION:-3.3.5}"
ESP32_BOARD_URL="https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

if ! command -v arduino-cli &> /dev/null; then
    echo "❌ Error: arduino-cli is not installed"
    echo "Install it with: brew install arduino-cli"
    echo "Or download from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

echo "✓ arduino-cli found: $(arduino-cli version)"
echo ""

echo "🔧 Ensuring ESP32 board manager URL..."
if ! arduino-cli config dump | grep -q "$ESP32_BOARD_URL"; then
    arduino-cli config init || true
    arduino-cli config add board_manager.additional_urls "$ESP32_BOARD_URL"
else
    echo "✓ Board manager URL already configured"
fi

echo "🔄 Updating core index..."
arduino-cli core update-index
echo ""

echo "📦 Checking ESP32 core..."
if arduino-cli core list | grep -q "esp32:esp32"; then
    INSTALLED_VERSION=$(arduino-cli core list | grep "esp32:esp32" | awk '{print $2}')
    echo "✓ ESP32 core installed: $INSTALLED_VERSION"
    if [ "$INSTALLED_VERSION" != "$RECOMMENDED_ESP32_CORE" ]; then
        echo "⚠️  Warning: Recommended version is $RECOMMENDED_ESP32_CORE, you have $INSTALLED_VERSION"
        read -p "Update to $RECOMMENDED_ESP32_CORE? (y/n) " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            arduino-cli core install esp32:esp32@"$RECOMMENDED_ESP32_CORE"
        fi
    fi
else
    echo "Installing ESP32 core $RECOMMENDED_ESP32_CORE..."
    arduino-cli core install esp32:esp32@"$RECOMMENDED_ESP32_CORE"
fi
echo ""

echo "📚 Installing required libraries..."
echo ""

echo "🔄 Updating library index..."
arduino-cli lib update-index
echo ""

ARDUINO_LIB_DIR="$HOME/Documents/Arduino/libraries"
if [ ! -d "$ARDUINO_LIB_DIR" ]; then
    mkdir -p "$ARDUINO_LIB_DIR"
fi

LIBRARIES=(
    "MAX6675 library"
    "SimpleTimer"
    "AutoPID"
    "ESP32Servo"
    "ElegantOTA@3.1.7"
    "PWMrelay"
    "ArduinoJson"
    "lvgl"
    "GFX Library for Arduino"
    "Dev Device Pins"
    "TAMC_GT911"
)

for lib in "${LIBRARIES[@]}"; do
    echo "Installing $lib..."
    arduino-cli lib install "$lib" || echo "⚠️  $lib may already be installed"
done

echo ""
echo "⚙️  Staging LVGL configuration..."

LVGL_CONFIG_SOURCE="$ARDUINO_LIB_DIR/lvgl/lv_conf_template.h"
LVGL_CONFIG_ROOT="$ARDUINO_LIB_DIR/lv_conf.h"
LVGL_CONFIG_LIBRARY="$ARDUINO_LIB_DIR/lvgl/lv_conf.h"

if [ -f "$LVGL_CONFIG_SOURCE" ]; then
    cp "$LVGL_CONFIG_SOURCE" "$LVGL_CONFIG_ROOT"
    if [ -d "$ARDUINO_LIB_DIR/lvgl" ]; then
        cp "$LVGL_CONFIG_SOURCE" "$LVGL_CONFIG_LIBRARY"
        for config in "$LVGL_CONFIG_ROOT" "$LVGL_CONFIG_LIBRARY"; do
            perl -0pi -e 's/^#if 0 /#if 1 /m; s/^#define LV_FONT_MONTSERRAT_16 0$/#define LV_FONT_MONTSERRAT_16 1/m; s/^#define LV_FONT_MONTSERRAT_22 0$/#define LV_FONT_MONTSERRAT_22 1/m; s/^#define LV_FONT_MONTSERRAT_48 0$/#define LV_FONT_MONTSERRAT_48 1/m' "$config"
        done
        echo "✓ LVGL configuration staged"
    else
        echo "❌ Error: lvgl library directory not found at $ARDUINO_LIB_DIR/lvgl"
        exit 1
    fi
else
    echo "❌ Error: LVGL configuration not found at $LVGL_CONFIG_SOURCE"
    exit 1
fi

echo ""
echo "📥 Installing ESPAsyncWebServer and AsyncTCP..."
echo "Note: These libraries must be installed manually from GitHub"

if [ ! -d "$ARDUINO_LIB_DIR/Async_TCP" ] && [ ! -d "$ARDUINO_LIB_DIR/AsyncTCP" ]; then
    echo "Downloading AsyncTCP from GitHub..."
    cd "$ARDUINO_LIB_DIR"
    git clone https://github.com/ESP32Async/AsyncTCP.git Async_TCP
    echo "✓ AsyncTCP installed"
else
    echo "✓ AsyncTCP already installed"
fi

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

ELEGANTOTA_H="$ARDUINO_LIB_DIR/ElegantOTA/src/ElegantOTA.h"

if [ -f "$ELEGANTOTA_H" ]; then
    if grep -q "#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1" "$ELEGANTOTA_H"; then
        echo "✓ ElegantOTA already configured for async mode"
    else
        echo "Enabling async mode in ElegantOTA..."
        cp "$ELEGANTOTA_H" "$ELEGANTOTA_H.bak"
        sed -i '' 's/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 0/#define ELEGANTOTA_USE_ASYNC_WEBSERVER 1/' "$ELEGANTOTA_H"

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
echo "1. Compile firmware: ./tools/firmware.sh build"
echo "2. Upload to device: ./tools/firmware.sh upload"
echo "3. Build for JC4827W543C: ./tools/firmware.sh build --board jc4827w543c"
echo "   Uses ESP32S3 Dev Module with: USB CDC on boot enabled, Hardware CDC + JTAG, DIO 80MHz, 4MB flash, Huge APP, OPI PSRAM, 240MHz WiFi, Core 1 loop/events"
echo ""
echo "For OTA updates after initial flash, see OTA_SETUP.md"
echo ""