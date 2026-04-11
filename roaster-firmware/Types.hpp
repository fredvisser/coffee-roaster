#ifndef TYPES_HPP
#define TYPES_HPP

#include <Arduino.h>

// ============================================================================
// STATE MACHINE
// ============================================================================

// Roaster state machine states
enum RoasterState
{
  IDLE = 0,
  START_ROAST = 1,
  ROASTING = 2,
  COOLING = 3,
  ERROR = 4,
  CALIBRATING = 5
};

// ============================================================================
// SAFETY LIMITS
// ============================================================================

// Temperature limits (all values in Fahrenheit)
#define MAX_SAFE_TEMP 500.0       // Absolute maximum safe temperature (°F)
#define MAX_ROAST_TEMP 460.0      // Maximum temperature during roast (°F)
#define COOLING_TARGET_TEMP 140   // Target temperature for cooling (°F)
#define MAX_SAFE_FAN_TEMP 150.0   // Maximum safe inlet/fan temperature (°F)

// Sensor failure detection
#define MAX_BAD_READINGS 5        // Consecutive bad readings before sensor failure
#define MAX_TEMP_JUMP 40.0        // Max allowed change in temp between readings (F)
#define SENSOR_FAULT_TEMP 600.0   // Thermocouple reads ~2048°F when disconnected

// Timing limits
#define MAX_COOLING_TIME 300000  // 5 minutes in milliseconds

// ============================================================================
// HARDWARE CONFIGURATION
// ============================================================================

// PWM output ranges
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255

// BDC fan servo pulse width limits (microseconds)
#define BDC_FAN_MIN 800
#define BDC_FAN_MAX 2000

// Nextion display error value
#define NEXTION_READ_ERROR 777777

// ============================================================================
// NETWORK TYPES
// ============================================================================

// WiFi credentials structure
struct WifiCredentials {
  String ssid;
  String password;
};

#endif // TYPES_HPP
