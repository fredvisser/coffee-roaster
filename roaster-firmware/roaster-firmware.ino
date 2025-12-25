#include "Arduino.h"
#include <EasyNextionLibrary.h>
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <AutoPID.h>
#include <SPI.h>
#include <Preferences.h>
#include <ElegantOTA.h> // v3.1.7+ with async mode enabled for ESPAsyncWebServer compatibility
#include <ESP32Servo.h>
#include <esp_task_wdt.h> // Hardware watchdog timer

// Debug macros (must be defined before including Network.hpp)
#define DEBUG

#ifdef DEBUG
#define DEBUG_SERIALBEGIN(x) Serial.begin(x)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINTF(...) Serial.printf(__VA_ARGS__)

#else
#define DEBUG_SERIALBEGIN(x) // blank line
#define DEBUG_PRINT(x)       // blank line
#define DEBUG_PRINTLN(x)     // blank line
#define DEBUG_PRINTF(...)    // blank line
#endif

#include "Types.hpp"
#include "DebugLog.hpp"
#include "Profiles.hpp"
#include "Network.hpp"

Preferences preferences;

#define HW_SERIAL Serial0

// Pin definitions
#define TC1_CS 10
#define TC2_CS 9
#define HEATER A0
#define FAN A1
#define BDCFAN D5

// PID settings and gains
#define KP 8.0
#define KI 0.46
#define KD 0

// Preferences namespace
#define PREFS_NAMESPACE "roaster"

#define VERSION "2025-12-24"

// Use timers for simple multitasking
SimpleTimer checkTempTimer(250);
SimpleTimer tickTimer(5);
SimpleTimer stateMachineTimer(500);
SimpleTimer wsBroadcastTimer(1000);  // WebSocket broadcast every 1 second

// PWM is used to control fan and heater outputs
PWMrelay heaterRelay(HEATER, HIGH);
PWMrelay fanRelay(FAN, HIGH);

Servo bdcFan;

// Create a roast profile object
Profiles profile;

// Roaster state variables
// NOTE: All temperature values throughout this codebase are in Fahrenheit (°F)
double currentTemp = 0;     // Current bean temperature (°F)
double setpointTemp = 0;    // Target temperature from profile (°F)
double heaterOutputVal = 0; // PID output (0-255)
byte setpointFanSpeed = 0;  // Target fan speed (0-255)
int setpointProgress = 0;   // Roast time in seconds
int bdcFanMs = 800;         // BDC fan servo pulse width (800-2000 µs)
double fanTemp = 0;         // Alternate temperature sensor (°F)
int badReadingCount = 0;    // Track consecutive bad thermocouple readings

// Fan ramp state variables (for non-blocking START_ROAST)
unsigned long fanRampStartTime = 0;
int fanRampStep = 0;

// Cooling state variables
unsigned long coolingStartTime = 0; // Track cooling duration

// WiFi credentials (loaded from preferences in setup())
WifiCredentials wifiCredentials;

// Roaster state variable (enum defined in Types.hpp)
RoasterState roasterState = IDLE;

MAX6675 thermocouple(SCK, TC1_CS, MISO);
MAX6675 thermocoupleFan(SCK, TC2_CS, MISO);
AutoPID heaterPID(&currentTemp, &setpointTemp, &heaterOutputVal, 0, 255, KP, KI, KD);

// Create a Nextion display connection
EasyNex myNex(HW_SERIAL);

uint8_t profileBuffer[200];
int finalTempOverride = -1; // Nextion override for final target temp (F)

// Helper function for reliable Nextion reads with retry logic
int readNextionWithRetry(const char *component, int retries = 2)
{
  for (int i = 0; i < retries; i++)
  {
    int value = myNex.readNumber(component);
    if (value != NEXTION_READ_ERROR)
    {
      return value;
    }
    delay(10); // Small delay before retry
  }
  LOG_WARNF("Nextion read failed: %s", component);
  return NEXTION_READ_ERROR;
}

uint32_t getEffectiveFinalTargetTemp()
{
  // Use Nextion override if set; otherwise fall back to profile final target
  if (finalTempOverride > 0)
  {
    return constrain(finalTempOverride, 0, 500);
  }
  return profile.getFinalTargetTemp();
}

void setup()
{
  DEBUG_SERIALBEGIN(115200);
  myNex.begin(115200);

  // Set heater and fan control pins to output
  pinMode(HEATER, OUTPUT);
  pinMode(FAN, OUTPUT);

  // Allocate timer 2 for servo to avoid conflict with PWMrelay
  ESP32PWM::allocateTimer(2);

  // Attach and initialize BDC fan controller
  int channel = bdcFan.attach(BDCFAN);
  if (channel == -1)
  {
    DEBUG_PRINTLN("ERROR: BDC fan attach FAILED!");
  }
  else
  {
    bdcFan.setPeriodHertz(50);
    bdcFan.writeMicroseconds(800);
  }

  // Explicitly enable the pinMode for the SPI pins because the library doesn't appear to do this correctly when running on an ESP32
  pinMode(TC1_CS, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  // Set output pins to safe state (LOW)
  digitalWrite(HEATER, LOW);
  heaterPID.setTimeStep(250);
  digitalWrite(FAN, LOW);
  fanRelay.setPeriod(10);

  bdcFan.writeMicroseconds(800);
  
  LOG_INFO("System initialized - entering IDLE state");

  // Initialize hardware watchdog timer (10 seconds timeout)
  // If loop() stops running, watchdog will reset the system
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 10000,  // 10 second timeout
      .idle_core_mask = 0,  // Don't watch idle tasks
      .trigger_panic = true // Reboot on timeout
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
  // Load WiFi credentials from preferences
  // Initialize preferences with error checking
  if (!preferences.begin(PREFS_NAMESPACE, false))
  {
    LOG_ERROR("Preferences initialization failed - using defaults");
    // Continue with defaults - don't halt system
  }

  wifiCredentials.ssid = preferences.getString("ssid", "");
  wifiCredentials.password = preferences.getString("password", "");

  String ipAddress = initializeWifi(wifiCredentials);
  if (ipAddress != "Failed to connect to WiFi")
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }

  // Initialize profile system: ensure default exists, then load active profile
  LOG_INFO("Initializing profile system...");
  ensureDefaultProfile();
  if (!reloadActiveProfile()) {
    LOG_WARN("Failed to load active profile on first attempt - retrying");
    // ensureDefaultProfile() will have created and activated "Default"
    if (!reloadActiveProfile()) {
      LOG_ERROR("Failed to load active profile on retry - system may be unstable");
    }
  }
  LOG_INFOF("Profile system initialized - using profile with %d setpoints", profile.getSetpointCount());

  // Update Nextion display with current profile values
  if (profile.getSetpointCount() >= 3) {
    myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
    myNex.writeNum("ConfigSetpoint.spTemp2.val", profile.getSetpoint(2).temp);
    myNex.writeNum("ConfigSetpoint.spTemp3.val", profile.getSetpoint(3).temp);
    myNex.writeNum("ConfigSetpoint.spTime1.val", profile.getSetpoint(1).time / 1000);
    myNex.writeNum("ConfigSetpoint.spTime2.val", profile.getSetpoint(2).time / 1000);
    myNex.writeNum("ConfigSetpoint.spTime3.val", profile.getSetpoint(3).time / 1000);
    myNex.writeNum("ConfigSetpoint.spFan1.val", profile.getSetpoint(1).fanSpeed);
    myNex.writeNum("ConfigSetpoint.spFan2.val", profile.getSetpoint(2).fanSpeed);
    myNex.writeNum("ConfigSetpoint.spFan3.val", profile.getSetpoint(3).fanSpeed);
  }
  myNex.writeStr("ConfigWifi.ip.txt", ipAddress);
  myNex.writeStr("ConfigNav.rev.txt", VERSION);

  myNex.writeStr("page Start");
}

void loop()
{
  // Reset watchdog timer every loop iteration
  // If loop hangs for >10 seconds, system will reset
  esp_task_wdt_reset();

  // Check WiFi connection and auto-reconnect if needed
  checkWiFiConnection(wifiCredentials);

  if (tickTimer.isReady())
  {
    myNex.NextionListen();
    heaterRelay.tick();
    fanRelay.tick();
    heaterPID.run();
    wsCleanup();
    ElegantOTA.loop(); // Handle OTA updates
    tickTimer.reset();
  }

  if (checkTempTimer.isReady())
  {
    // Read thermocouple with failure detection
    double reading = thermocouple.readFarenheit();

    // MAX6675 returns ~2048°F when disconnected, also check for unreasonable values
    if (reading < 0 || reading > 600)
    {
      badReadingCount++;
      if (badReadingCount >= MAX_BAD_READINGS)
      {
        // SENSOR FAILURE - EMERGENCY STOP
        roasterState = ERROR;
        digitalWrite(HEATER, LOW);
        heaterPID.stop();
        heaterOutputVal = 0;
        heaterRelay.setPWM(0);
        fanRelay.setPWM(255); // Full fan for safety
        bdcFan.writeMicroseconds(2000);
        DEBUG_PRINTLN("EMERGENCY: Thermocouple failure detected!");
        myNex.writeStr("page Error");
        myNex.writeStr("Error.message.txt", "Sensor Failed");
      }
    }
    else
    {
      currentTemp = reading;
      badReadingCount = 0; // Reset counter on good reading

      // THERMAL RUNAWAY PROTECTION
      if (currentTemp > MAX_SAFE_TEMP)
      {
        // EMERGENCY SHUTDOWN
        roasterState = ERROR;
        digitalWrite(HEATER, LOW);
        heaterPID.stop();
        heaterOutputVal = 0;
        heaterRelay.setPWM(0);
        fanRelay.setPWM(255); // Full fan to cool down
        bdcFan.writeMicroseconds(2000);
        DEBUG_PRINTLN("EMERGENCY: Thermal runaway detected!");
        myNex.writeStr("page Error");
        myNex.writeStr("Error.message.txt", "Over Temp");
      }
    }

    fanTemp = thermocoupleFan.readFarenheit();
    // Metrics now available via debug console at /console
    checkTempTimer.reset();
  }

  if (stateMachineTimer.isReady())
  {
    switch (roasterState)
    {
    case IDLE:
      digitalWrite(HEATER, LOW);
      digitalWrite(FAN, LOW);
      bdcFan.writeMicroseconds(800); // Ensure BDC stays at low speed
      heaterPID.stop();
      heaterOutputVal = 0;
      break;

    case START_ROAST:
    {
      // Non-blocking fan ramp-up
      if (fanRampStep == 0)
      {
        LOG_INFOF("Starting roast - Fan ramp-up initiated (%.1fF)", currentTemp);
        fanRampStartTime = millis();
        fanRampStep = 800;
      }

      unsigned long elapsed = millis() - fanRampStartTime;
      int targetStep = 800 + (elapsed / 500) * 100;

      // Update fan speed every 500ms
      if (targetStep <= 2000 && targetStep > fanRampStep)
      {
        bdcFan.writeMicroseconds(targetStep);
        fanRampStep = targetStep;
      }

      // Fan ramp complete after reaching 2000 and waiting additional 500ms
      if (fanRampStep >= 2000 && elapsed >= 6500)
      {
        // Reset for next roast
        fanRampStep = 0;

        // Start roasting with current active profile
        roasterState = ROASTING;
        profile.startProfile((int)currentTemp, millis());
        heaterPID.run();
        heaterRelay.setPWM(heaterOutputVal);
        LOG_INFOF("Roast started: Target=%.0fF, Setpoints=%d", (float)getEffectiveFinalTargetTemp(), profile.getSetpointCount());
        sendWsMessage("{ \"pushMessage\": \"startRoasting\" }");
      }

      // Set initial PWM fan speed
      fanRelay.setPWM(profile.getTargetFanSpeed(millis()));
      break;
    }

    case ROASTING:
    {
      // Ensure PID is actively running every loop iteration
      heaterPID.run();

      setpointTemp = profile.getTargetTemp(millis());
      setpointFanSpeed = profile.getTargetFanSpeed(millis());
      setpointProgress = profile.getProfileProgress(millis());

      fanRelay.setPWM(setpointFanSpeed);

      // Clamp BDC fan to safe range (800-2000 microseconds)
      int bdcValue = 5 * setpointFanSpeed + 700;
      bdcValue = constrain(bdcValue, 800, 2000);
      bdcFan.writeMicroseconds(bdcValue);

      heaterRelay.setPWM(heaterOutputVal);

      if (currentTemp >= getEffectiveFinalTargetTemp())
      {
        heaterPID.stop();
        heaterOutputVal = 0;
        heaterRelay.setPWM(heaterOutputVal);

        setpointTemp = 145;
        roasterState = COOLING;
        coolingStartTime = millis(); // Start cooling timer

        setpointFanSpeed = 255;
        fanRelay.setPWM(setpointFanSpeed);
        bdcFan.writeMicroseconds(2000);

        setpointProgress = 0;
        myNex.writeStr("page Cooling");
        LOG_INFOF("Roast complete at %.1fF - entering cooling phase", currentTemp);
      }
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
      myNex.writeNum("globals.setpointFan.val", round(setpointFanSpeed * 100 / 255));
      myNex.writeNum("globals.setpointProg.val", setpointProgress);
      break;
    }

    case COOLING:
    {
      digitalWrite(HEATER, LOW);
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);

      // Check for cooling timeout (30 minutes max)
      unsigned long coolingDuration = millis() - coolingStartTime;
      if (coolingDuration > MAX_COOLING_TIME)
      {
        LOG_WARNF("Cooling timeout after %lu minutes - forcing IDLE", coolingDuration / 60000);
        fanRelay.setPWM(0);
        bdcFan.writeMicroseconds(800);
        digitalWrite(FAN, LOW);
        roasterState = IDLE;
        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        myNex.writeStr("page Start");
        break;
      }

      if (currentTemp <= 145)
      {
        fanRelay.setPWM(0);
        bdcFan.writeMicroseconds(800);
        digitalWrite(FAN, LOW);
        roasterState = IDLE;

        LOG_INFOF("Cooling complete at %.1fF - returning to IDLE", currentTemp);
        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        myNex.writeStr("page Start");
      }
      break;
    }

    case ERROR:
      // ERROR state: Keep system in safe mode until manual reset
      // Heater must stay OFF, cooling fan at safe speed
      digitalWrite(HEATER, LOW);
      heaterRelay.setPWM(0);
      heaterPID.stop();

      // Run cooling fan at safe speed (not maximum to avoid mechanical stress)
      fanRelay.setPWM(200);           // ~78% speed for sustained cooling
      bdcFan.writeMicroseconds(1500); // Mid-range for BDC fan

      // Update display with current temperature
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);

      // ERROR state logged when entered, not every loop iteration

      // Only exit ERROR state through hardware reset
      // No automatic recovery to ensure user acknowledges the fault
      break;

    default:
      LOG_WARNF("Unknown state: %d - returning to IDLE", roasterState);
      roasterState = IDLE;
      break;
    }
    stateMachineTimer.reset();
  }

  // Broadcast system state via WebSocket to debug console
  if (wsBroadcastTimer.isReady())
  {
    broadcastSystemState();
    broadcastLogs(50);  // Send last 50 log entries
    wsBroadcastTimer.reset();
  }
}

void trigger0()
{ // Start roast command received
  LOG_INFO("trigger0() called - Start button pressed");
  
  // Use the currently active profile (managed by web UI)
  // Nextion display values are for display only, not for modifying the profile
  int spCount = profile.getSetpointCount();
  LOG_INFOF("Starting roast with active profile (%d setpoints)", spCount);
  
  if (spCount == 0) {
    LOG_ERROR("Cannot start roast - profile has no setpoints!");
    myNex.writeStr("page Error");
    myNex.writeStr("Error.message.txt", "No Profile");
    return;
  }
  
  int uiFinalTemp = readNextionWithRetry("globals.setTempNum.val");
  if (uiFinalTemp != NEXTION_READ_ERROR && uiFinalTemp > 0) {
    finalTempOverride = constrain(uiFinalTemp, 0, 500);
    LOG_INFOF("Using Nextion final target override: %dF", finalTempOverride);
  } else {
    finalTempOverride = profile.getFinalTargetTemp();
    LOG_WARN("Nextion final target not available - using profile final temp");
  }
  
  roasterState = START_ROAST;
  myNex.writeStr("page Roasting");
  myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
  LOG_INFO("trigger0() complete - state set to START_ROAST");
}

void trigger1()
{ // Stop roast command received
  roasterState = COOLING;

  heaterPID.stop();
  heaterOutputVal = 0;
  heaterRelay.setPWM(heaterOutputVal);
  digitalWrite(HEATER, LOW);

  setpointFanSpeed = 255;
  fanRelay.setPWM(setpointFanSpeed);
  bdcFan.writeMicroseconds(2000);

  myNex.writeStr("page Cooling");
  myNex.writeNum("globals.nextSetTempNum.val", 145);
}

void trigger2()
{ // Stop cooling command received
  roasterState = IDLE;
  myNex.writeStr("page Start");
}

void trigger3()
{ // Apply WiFi credentials
  // Update global WiFi credentials
  wifiCredentials.ssid = myNex.readStr("ConfigWifi.ssid.txt");
  wifiCredentials.password = myNex.readStr("ConfigWifi.password.txt");
  myNex.writeStr("ConfigWifi.ip.txt", "Connecting...");
  String ip = initializeWifi(wifiCredentials);
  myNex.writeStr("ConfigWifi.ip.txt", ip);
  if (ip != "Failed to connect to WiFi")
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }
}

void trigger4()
{ // ProfileActive page button - plot current profile
  LOG_INFO("trigger4() called - ProfileActive plot button");
  onProfileActivePageEnter();
}
