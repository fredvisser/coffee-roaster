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
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255
#define KP 8.0
#define KI 0.46
#define KD 0

// Safety limits
#define MAX_SAFE_TEMP 500.0       // Absolute maximum safe temperature (°F)
#define MAX_ROAST_TEMP 460.0      // Maximum temperature during roast (°F)
#define COOLING_TARGET_TEMP 145   // Target temperature for cooling (°F)
#define MAX_BAD_READINGS 5        // Consecutive bad readings before sensor failure
#define NEXTION_READ_ERROR 777777 // Error value returned by Nextion

// Preferences namespace
#define PREFS_NAMESPACE "roaster"

#define VERSION "2025-12-22"

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
#define MAX_COOLING_TIME 1800000    // 30 minutes in milliseconds

// WiFi credentials (loaded from preferences in setup())
WifiCredentials wifiCredentials;

// Roaster state variable (enum defined in Types.hpp)
RoasterState roasterState = IDLE;

MAX6675 thermocouple(SCK, TC1_CS, MISO);
MAX6675 thermocoupleFan(SCK, TC2_CS, MISO);
AutoPID heaterPID(&currentTemp, &setpointTemp, &heaterOutputVal, 0, 255, KP, KI, KD);

// Create a Nextion display connection
EasyNex myNex(HW_SERIAL);

// Default roast profile. Any changes stored in EEPROM will override these values.
void setDefaultRoastProfile()
{
  profile.clearSetpoints();
  profile.addSetpoint(150000, 300, 100);
  profile.addSetpoint(300000, 380, 90);
  profile.addSetpoint(480000, 440, 90);
}
uint8_t profileBuffer[200];

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
  DEBUG_PRINT("Nextion read failed: ");
  DEBUG_PRINTLN(component);
  return NEXTION_READ_ERROR;
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

  setDefaultRoastProfile();

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
    DEBUG_PRINTLN("ERROR: Failed to initialize preferences!");
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

  preferences.getBytes("profile", profileBuffer, 200);
  profile.unflattenProfile(profileBuffer);

  myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
  myNex.writeNum("ConfigSetpoint.spTemp2.val", profile.getSetpoint(2).temp);
  myNex.writeNum("ConfigSetpoint.spTemp3.val", profile.getSetpoint(3).temp);
  myNex.writeNum("ConfigSetpoint.spTime1.val", profile.getSetpoint(1).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime2.val", profile.getSetpoint(2).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime3.val", profile.getSetpoint(3).time / 1000);
  myNex.writeNum("ConfigSetpoint.spFan1.val", profile.getSetpoint(1).fanSpeed);
  myNex.writeNum("ConfigSetpoint.spFan2.val", profile.getSetpoint(2).fanSpeed);
  myNex.writeNum("ConfigSetpoint.spFan3.val", profile.getSetpoint(3).fanSpeed);
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
    DEBUG_PRINT("Temp: ");
    DEBUG_PRINTLN(currentTemp);
    DEBUG_PRINT("Fan speed: ");
    DEBUG_PRINTLN(setpointFanSpeed);
    DEBUG_PRINT("Heater output: ");
    DEBUG_PRINTLN(heaterOutputVal);
    DEBUG_PRINT("Setpoint temp: ");
    DEBUG_PRINTLN(setpointTemp);
    DEBUG_PRINT("Setpoint progress: ");
    DEBUG_PRINTLN(setpointProgress);
    DEBUG_PRINT("Fan chamber temp: ");
    DEBUG_PRINTLN(fanTemp);
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

        // Save profile BEFORE starting
        profile.flattenProfile(profileBuffer);
        preferences.putBytes("profile", profileBuffer, 200);

        // Start roasting
        roasterState = ROASTING;
        profile.startProfile((int)currentTemp, millis());
        heaterPID.run();
        heaterRelay.setPWM(heaterOutputVal);
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

      if (currentTemp >= profile.getFinalTargetTemp())
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
        DEBUG_PRINTLN("Roast complete -> cooling");
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
      DEBUG_PRINTLN("Cooling");

      // Check for cooling timeout (30 minutes max)
      unsigned long coolingDuration = millis() - coolingStartTime;
      if (coolingDuration > MAX_COOLING_TIME)
      {
        DEBUG_PRINTLN("Cooling timeout - forcing IDLE");
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

        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        myNex.writeStr("page Start");
        DEBUG_PRINTLN("Cooling - stopped");
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

      DEBUG_PRINTLN("ERROR state - system locked. Reset required.");

      // Only exit ERROR state through hardware reset
      // No automatic recovery to ensure user acknowledges the fault
      break;

    default:
      DEBUG_PRINTLN("Hit default case!!");
      break;
    }
    stateMachineTimer.reset();
  }

  // Broadcast system state via WebSocket to debug console
  if (wsBroadcastTimer.isReady())
  {
    broadcastSystemState();
    wsBroadcastTimer.reset();
  }
}

void trigger0()
{ // Start roast command received
  // Read all profile values with retry logic
  int temp1 = readNextionWithRetry("ConfigSetpoint.spTemp1.val");
  int temp2 = readNextionWithRetry("ConfigSetpoint.spTemp2.val");
  int temp3 = readNextionWithRetry("ConfigSetpoint.spTemp3.val");
  int time1 = readNextionWithRetry("ConfigSetpoint.spTime1.val");
  int time2 = readNextionWithRetry("ConfigSetpoint.spTime2.val");
  int time3 = readNextionWithRetry("ConfigSetpoint.spTime3.val");
  int power1 = readNextionWithRetry("ConfigSetpoint.spFan1.val");
  int power2 = readNextionWithRetry("ConfigSetpoint.spFan2.val");
  int power3 = readNextionWithRetry("ConfigSetpoint.spFan3.val");

  // Check if any reads failed - use current profile if so
  if (temp1 == NEXTION_READ_ERROR || temp2 == NEXTION_READ_ERROR || temp3 == NEXTION_READ_ERROR ||
      time1 == NEXTION_READ_ERROR || time2 == NEXTION_READ_ERROR || time3 == NEXTION_READ_ERROR ||
      power1 == NEXTION_READ_ERROR || power2 == NEXTION_READ_ERROR || power3 == NEXTION_READ_ERROR)
  {
    DEBUG_PRINTLN("Failed to read profile from Nextion - using existing profile");
    // Don't update profile, just start roast with current values
  }
  else
  {
    // Validate all inputs are within safe ranges before adding to profile
    temp1 = constrain(temp1, 0, 500);
    temp2 = constrain(temp2, 0, 500);
    temp3 = constrain(temp3, 0, 500);
    power1 = constrain(power1, 0, 100);
    power2 = constrain(power2, 0, 100);
    power3 = constrain(power3, 0, 100);
    time1 = max(0, time1);
    time2 = max(0, time2);
    time3 = max(0, time3);

    profile.clearSetpoints();
    profile.addSetpoint(time1 * 1000, temp1, power1);
    profile.addSetpoint(time2 * 1000, temp2, power2);
    profile.addSetpoint(time3 * 1000, temp3, power3);
  }

  roasterState = START_ROAST;
  myNex.writeStr("page Roasting");
  myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
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
