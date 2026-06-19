#include "Arduino.h"
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <SPI.h>
#include <Preferences.h>
#include <ElegantOTA.h> // v3.1.7+ with async mode enabled for ESPAsyncWebServer compatibility
#include <ESP32Servo.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h> // Hardware watchdog timer

// Debug macros (must be defined before including src/network/Network.hpp)
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

#include "src/platform/RoasterTypes.hpp"
#include "src/platform/BoardConfig.hpp"
#include "src/display/DisplayBackendConfig.hpp"
#include "src/support/DebugLog.hpp"
#include "src/display/DisplayAdapter.hpp"
#include "src/display/DisplayActionRouter.hpp"
#include "src/control/PIDController.hpp"
#include "src/profiles/RoastProfile.hpp"
#include "src/profiles/ProfileManager.hpp"
#include "src/profiles/ProfileEditor.hpp"
#include "src/control/StepResponseTuner.hpp"
#include "src/control/PIDRuntimeController.hpp"
#include "src/control/PIDValidation.hpp"
#include "src/control/RoastControlLoop.hpp"
#include "src/control/RoastSessionLifecycle.hpp"
#include "src/integrations/SystemLink.hpp"

extern char activeFaultCode[32];
extern char activeFaultMessage[96];
bool canClearErrorState();

#include "src/network/Network.hpp"

Preferences preferences;

// Pin definitions
constexpr int TC1_CS = BoardConfig::BeanThermocoupleChipSelectPin;
constexpr int TC2_CS = BoardConfig::FanThermocoupleChipSelectPin;
constexpr int THERMOCOUPLE_SCK = BoardConfig::ThermocoupleClockPin;
constexpr int THERMOCOUPLE_MISO = BoardConfig::ThermocoupleDataPin;
constexpr int HEATER = BoardConfig::HeaterPwmPin;
constexpr int FAN = BoardConfig::FanPwmPin;
constexpr int BDCFAN = BoardConfig::BdcFanServoPin;

// PID settings and gains
double kp = 8.0;
double ki = 0.46;
double kd = 0;

// Preferences namespace
#define PREFS_NAMESPACE "roaster"

#ifndef VERSION
#define VERSION "2025-12-24"
#endif

// Use timers for simple multitasking
SimpleTimer checkTempTimer(125);
SimpleTimer tickTimer(5);
SimpleTimer controlLoopTimer(250);
SimpleTimer stateMachineTimer(500);
SimpleTimer wsBroadcastTimer(1000);  // WebSocket broadcast every 1 second
SimpleTimer roastTraceTimer(1000);   // Roast trace capture every 1 second

// PWM is used to control fan and heater outputs
PWMrelay heaterRelay(HEATER, HIGH);
PWMrelay fanRelay(FAN, HIGH);

Servo bdcFan;

static void configureJcHeapForPsram() {
#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
  if (!psramFound()) {
    LOG_WARN("PSRAM not detected; leaving default heap allocation policy in place");
    return;
  }

  // Prefer PSRAM for general allocations so internal DRAM remains available for TLS.
  heap_caps_malloc_extmem_enable(0);
  LOG_INFO("Configured heap allocator to prefer PSRAM for general allocations");
#endif
}

// Create a roast profile object
RoastProfile profile;
ProfileManager profileManager;

// Roaster state variables
// NOTE: All temperature values throughout this codebase are in Fahrenheit (°F)
double currentTemp = 0;     // Current bean temperature (°F)
double setpointTemp = 0;    // Target temperature from profile (°F)
double heaterOutputVal = 0; // Final heater command (0-255)
double heaterPidTrimVal = 0;
double heaterFeedforwardVal = 0;
byte setpointFanSpeed = 0;  // Target fan speed (0-255)
int setpointProgress = 0;   // Roast time in seconds
int bdcFanMs = 800;         // BDC fan servo pulse width (800-2000 µs)
double fanTemp = 0;         // Inlet/fan temperature sensor (°F)
int badReadingCount = 0;    // Track consecutive bad thermocouple readings
char lastRejectedBeanReadReason[16] = "none";
char activeFaultCode[32] = "none";
char activeFaultMessage[96] = "";

// Restart handling
bool restartRequested = false;
unsigned long restartAt = 0;

// Fan ramp state variables (for non-blocking START_ROAST)
unsigned long fanRampStartTime = 0;
int fanRampStep = 0;

// Cooling state variables
unsigned long coolingStartTime = 0; // Track cooling duration

// WiFi credentials (loaded from preferences in setup())
WifiCredentials wifiCredentials;

// Roaster state variable (enum defined in RoasterTypes.hpp)
RoasterState roasterState = IDLE;

inline bool shouldFilterBeanTempSpikes()
{
  return roasterState == START_ROAST || roasterState == ROASTING || roasterState == COOLING;
}

inline bool shouldFilterFanTempSpikes()
{
  return roasterState == START_ROAST || roasterState == ROASTING || roasterState == COOLING;
}

inline bool shouldEnforceFanTempSafety()
{
  if (roasterState == COOLING) {
    return true;
  }

  if (roasterState != ROASTING) {
    return false;
  }

  // During the roast start transition the heater is still off and the exhaust
  // sensor should remain near ambient. Treat large fan-sensor excursions during
  // that phase as implausible until heating has actually begun.
  if (heaterOutputVal <= 0.0 && currentTemp < 140.0) {
    return false;
  }

  if (roastStartedAtMs == 0) {
    return false;
  }

  if (millis() - roastStartedAtMs < FAN_TEMP_SAFETY_ARM_DELAY_MS) {
    return false;
  }

  return true;
}

inline void setLastRejectedBeanReadReason(const char *reason)
{
  strlcpy(lastRejectedBeanReadReason, reason ? reason : "unknown", sizeof(lastRejectedBeanReadReason));
}

inline bool isRoastActiveState()
{
  return roasterState == START_ROAST || roasterState == ROASTING;
}

inline void setActiveFault(const char *faultCode, const char *displayMessage)
{
  strlcpy(activeFaultCode, faultCode ? faultCode : "unknown_fault", sizeof(activeFaultCode));
  strlcpy(activeFaultMessage, displayMessage ? displayMessage : "Controller fault detected.", sizeof(activeFaultMessage));
}

inline String formatBeanSensorFaultMessage(double reading)
{
  char buffer[96];
  const char *reason = strcmp(lastRejectedBeanReadReason, "spike") == 0 ? "unstable" : "range error";
  snprintf(buffer, sizeof(buffer), "Bean sensor %s (raw %.1fF)", reason, reading);
  return String(buffer);
}

inline bool canClearErrorState()
{
  return badReadingCount == 0 && currentTemp < COOLING_TARGET_TEMP && fanTemp < MAX_SAFE_FAN_TEMP;
}

inline String formatErrorRecoveryBlockedMessage()
{
  if (badReadingCount > 0)
  {
    return String("Sensor fault still active. Fix wiring or sensor signal first.");
  }

  if (currentTemp >= COOLING_TARGET_TEMP)
  {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "Bean temp %.1fF is still above the 140F safe-idle limit.", currentTemp);
    return String(buffer);
  }

  if (fanTemp >= MAX_SAFE_FAN_TEMP)
  {
    char buffer[96];
    snprintf(buffer, sizeof(buffer), "Fan temp %.1fF is still above the %.0fF safety limit.", fanTemp, MAX_SAFE_FAN_TEMP);
    return String(buffer);
  }

  return String("Fault lockout remains active until the controller reports a safe state.");
}

inline void clearEmergencyErrorState()
{
  LOG_INFOF("Clearing error state: fault=%s bean=%.1fF fan=%.1fF", activeFaultCode, currentTemp, fanTemp);

  roasterState = IDLE;
  coolingStartTime = 0;
  roastStartedAtMs = 0;
  setpointTemp = 0;
  setpointFanSpeed = 0;
  heaterOutputVal = 0;
  heaterPidTrimVal = 0;
  heaterFeedforwardVal = 0;
  badReadingCount = 0;
  setActiveFault("none", "");

  digitalWrite(HEATER, LOW);
  resetRoastControllerState();
  heaterRelay.setPWM(0);
  fanRelay.setPWM(0);
  bdcFan.writeMicroseconds(800);
  bdcFanMs = 800;

  systemLinkUpdateLastFault("none");
  displayShowScreen(DisplayScreen::Start);
}

inline void enterEmergencyErrorState(const char *faultCode, const char *displayMessage)
{
  setActiveFault(faultCode, displayMessage);
  roasterState = ERROR;
  digitalWrite(HEATER, LOW);
  resetRoastControllerState();
  heaterRelay.setPWM(0);
  fanRelay.setPWM(255);
  bdcFan.writeMicroseconds(2000);
  bdcFanMs = 2000;
  systemLinkUpdateLastFault(faultCode);
  systemLinkFinishRoast(SYSTEMLINK_OUTCOME_ERRORED, faultCode);
  displayShowErrorMessage(activeFaultMessage);
}

MAX6675 thermocouple(THERMOCOUPLE_SCK, TC1_CS, THERMOCOUPLE_MISO);
MAX6675 thermocoupleFan(THERMOCOUPLE_SCK, TC2_CS, THERMOCOUPLE_MISO);
PIDController heaterPID(&currentTemp, &setpointTemp, &heaterPidTrimVal, 0, 255, kp, ki, kd);
StepResponseTuner stepTuner;
bool autoValidateAfterCooling = false;
PIDRuntimeController pidRuntimeController;
PIDValidationSession pidValidation;

uint8_t profileBuffer[200];
int finalTempOverride = -1; // UI override for final target temp (F)
RoastProfile validationSavedProfile;
bool validationProfileLoaded = false;
int savedValidationFinalTempOverride = -1;
unsigned long roastStartedAtMs = 0;
double appliedKp = kp;
double appliedKi = ki;
double appliedKd = kd;
bool pidScheduleConfigured = false;
bool pidScheduleActive = false;
int activePidBandIndex = -1;

// Helper function for reliable final-target reads from the active display.
int readDisplayFinalTargetTempWithRetry(int retries = 2)
{
  for (int i = 0; i < retries; i++)
  {
    int value = displayReadFinalTargetTemp();
    if (value != DISPLAY_READ_ERROR)
    {
      return value;
    }
    delay(10); // Small delay before retry
  }
  LOG_WARN("Display final target read failed");
  return DISPLAY_READ_ERROR;
}

uint32_t getEffectiveFinalTargetTemp()
{
  // Use the UI override if set; otherwise fall back to the profile final target.
  if (finalTempOverride > 0)
  {
    return constrain(finalTempOverride, 0, 500);
  }
  return profile.getFinalTargetTemp();
}

void updateCalibrationControl(unsigned long now)
{
  if (roasterState != CALIBRATING || fanRampStep < 2000)
  {
    return;
  }

  updateStepResponseCalibration(now);
}

void updateStepResponseCalibration(unsigned long now)
{
  heaterOutputVal = stepTuner.getOutput(currentTemp, fanTemp, setpointFanSpeed);
  setpointTemp = stepTuner.getSetpoint();
  heaterPidTrimVal = heaterOutputVal;
  heaterFeedforwardVal = 0.0;

  int requestedFanPwm = stepTuner.isRecoveryCooling() ? 255 : setpointFanSpeed;
  fanRelay.setPWM(requestedFanPwm);
  int calibrationBdcValue = stepTuner.isRecoveryCooling() ? 2000 : constrain(5 * requestedFanPwm + 700, 800, 2000);
  bdcFan.writeMicroseconds(calibrationBdcValue);
  bdcFanMs = calibrationBdcValue;
  heaterRelay.setPWM(heaterOutputVal);

  if (stepTuner.isComplete())
  {
    double newKp, newKi, newKd;
    stepTuner.getPID(newKp, newKi, newKd);

    setManualPIDGains(newKp, newKi, newKd);
    pidRuntimeController.clearPreferences(preferences);
    pidScheduleConfigured = false;
    activePidBandIndex = -1;

    // Load per-band models into runtime controller
    Calibration::CharacterizationSummary cs = stepTuner.getCharacterizationSummary();
    pidRuntimeController.loadFromSummary(cs);
    if (pidRuntimeController.isEnabled()) {
      pidRuntimeController.saveToPreferences(preferences);
      pidScheduleConfigured = true;
      LOG_INFO("Step-response band models loaded into gain scheduler");
    }

    LOG_INFOF("Step-response tuning saved: Kp=%.4f, Ki=%.6f, Kd=%.4f", kp, ki, kd);

    // Publish calibration data to SystemLink
    systemLinkPublishCalibration(stepTuner);

    resetRoastControllerState();
    heaterRelay.setPWM(0);
    autoValidateAfterCooling = true;
    enterCoolingState();
    sendWsMessage("{ \"pushMessage\": \"pidTuningComplete\" }");
    return;
  }

  if (!stepTuner.isRunning())
  {
    LOG_WARNF("Step-response tuning ended: %s", stepTuner.getLastError());
    resetRoastControllerState();
    heaterRelay.setPWM(0);
    enterCoolingState();
    if (strcmp(stepTuner.getLastError(), "cancelled") == 0)
    {
      sendWsMessage("{ \"pushMessage\": \"pidTuningCancelled\" }");
    }
    else
    {
      sendWsMessage("{ \"pushMessage\": \"pidTuningFailed\" }");
    }
  }
}

void setup()
{
  DEBUG_SERIALBEGIN(115200);
  delay(1000); // Give Serial Monitor time to connect
  Serial.println("\n\n--- ROASTER BOOTING ---");
  displayBegin(BoardConfig::DisplayBaudRate);

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
  pinMode(THERMOCOUPLE_SCK, OUTPUT);
  pinMode(THERMOCOUPLE_MISO, INPUT);

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

  esp_err_t wdtStatus = esp_task_wdt_status(NULL);
  if (wdtStatus == ESP_ERR_INVALID_STATE) {
    esp_err_t initResult = esp_task_wdt_init(&wdt_config);
    if (initResult != ESP_OK) {
      LOG_ERRORF("Failed to initialize task watchdog (%d)", static_cast<int>(initResult));
    }
  } else {
    esp_err_t reconfigureResult = esp_task_wdt_reconfigure(&wdt_config);
    if (reconfigureResult != ESP_OK) {
      LOG_ERRORF("Failed to reconfigure task watchdog (%d)", static_cast<int>(reconfigureResult));
    }
  }

  if (wdtStatus == ESP_ERR_NOT_FOUND) {
    esp_err_t addResult = esp_task_wdt_add(NULL);
    if (addResult != ESP_OK) {
      LOG_ERRORF("Failed to subscribe loop task to watchdog (%d)", static_cast<int>(addResult));
    }
  }

  configureJcHeapForPsram();

  // Load WiFi credentials from preferences
  // Initialize preferences with error checking
  if (!preferences.begin(PREFS_NAMESPACE, false))
  {
    LOG_ERROR("Preferences initialization failed - using defaults");
    // Continue with defaults - don't halt system
  }

  // Load PID values
  kp = preferences.getDouble("kp", 8.0);
  ki = preferences.getDouble("ki", 0.46);
  kd = preferences.getDouble("kd", 0.0);
  loadSystemLinkConfig();
  pidRuntimeController.setFallbackGains(kp, ki, kd);
  pidRuntimeController.loadFromPreferences(preferences);
  pidScheduleConfigured = pidRuntimeController.isEnabled();
  applyHeaterPIDGains(kp, ki, kd);
  LOG_INFOF("PID Loaded: Kp=%.4f, Ki=%.4f, Kd=%.4f", kp, ki, kd);
  LOG_INFOF("PID runtime schedule %s (%u valid bands)", pidRuntimeController.isEnabled() ? "enabled" : "disabled", pidRuntimeController.getValidBandCount());

  // --- BOOT LOOP PROTECTION ---
  // If we crash repeatedly during startup (e.g. due to corrupt NVS), purge profiles
  int bootCount = preferences.getInt("boot_count", 0);
  Serial.printf("Boot count: %d\n", bootCount); // Force print to Serial
  if (bootCount > 5) {
    LOG_ERRORF("CRITICAL: Boot loop detected (count=%d)! Purging all profiles to recover system.", bootCount);
    Serial.println("CRITICAL: Purging profiles due to boot loop!");
    systemLinkUpdateLastFault("boot_loop_detected");
    profileManager.deleteAllProfiles();
    preferences.putInt("boot_count", 0);
    delay(2000); // Allow time for serial log to be seen
  } else {
    preferences.putInt("boot_count", bootCount + 1);
    LOG_INFOF("Boot count: %d", bootCount + 1);
  }
  // ----------------------------

  systemLinkSetBootContext(bootCount + 1);
  systemLinkPrepareRecoveryPublish();

  wifiCredentials.ssid = preferences.getString("ssid", "");
  wifiCredentials.password = preferences.getString("password", "");
  displaySetWifiFormState(DisplayWifiFormState{wifiCredentials.ssid, wifiCredentials.password});

  String ipAddress = initializeWifi(wifiCredentials);
  if (ipAddress != "Failed to connect to WiFi")
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }

  // Initialize profile system: ensure default exists, then load active profile
  LOG_INFO("Initializing profile system...");
  profileManager.ensureDefault();
  
  String activeId = profileManager.getActiveProfileId();
  if (activeId.length() > 0) {
    if (!profileManager.loadProfile(activeId)) {
       LOG_WARN("Failed to load active profile");
    }
  } else {
     // Fallback to first available
     auto ids = profileManager.getProfileIds();
     if (!ids.empty()) {
         profileManager.setActiveProfileId(ids.front());
         profileManager.loadProfile(ids.front());
     }
  }
  
  LOG_INFOF("Profile system initialized - using profile with %d setpoints", profile.getSetpointCount());

  String activeProfileName;
  if (activeId.length() > 0 && profileManager.loadProfileMeta(activeId, activeProfileName))
  {
    displaySetActiveProfileLabel(activeProfileName);
  }
  displaySetStoredProfileFinalTarget(static_cast<int>(profile.getFinalTargetTemp()));

  // Update the active display with current profile values.
  // If no override is set (-1), this sends the profile's default final target.
  LOG_INFOF("Original display final target value is %dF", displayReadFinalTargetTemp());
  displaySetFinalTargetTemp(getEffectiveFinalTargetTemp());
  LOG_INFOF("Set display final target temp to %dF", getEffectiveFinalTargetTemp());
  LOG_INFOF("Final display final target value is %dF", displayReadFinalTargetTemp());

  displaySetWifiIp(ipAddress);
  displaySetRevision(VERSION);

  displayShowScreen(DisplayScreen::Start);
  initSystemLinkTagTask();
  initSystemLinkPublishTask();
  LOG_INFO("Setup complete - entering main loop");
}

void loop()
{
  // Reset watchdog timer every loop iteration
  // If loop hangs for >10 seconds, system will reset
  esp_task_wdt_reset();

  // Reset boot count if system has been stable for 10 seconds
  static bool bootCountReset = false;
  if (!bootCountReset && millis() > 10000) {
    preferences.putInt("boot_count", 0);
    bootCountReset = true;
    LOG_INFO("System stable - boot count reset");
  }

  // Check WiFi connection and auto-reconnect if needed
  checkWiFiConnection(wifiCredentials);

  if (tickTimer.isReady())
  {
    if (!otaUpdateInProgress)
    {
      displayTick();
      handleDisplayActions();
    }
    heaterRelay.tick();
    fanRelay.tick();
    if (!otaUpdateInProgress)
    {
      wsCleanup();
    }
    ElegantOTA.loop(); // Handle OTA updates
    tickTimer.reset();
  }

  if (checkTempTimer.isReady())
  {
    static int readCycle = 0; // 0-7 counter for 1Hz fan updates
    static double lastValidTemp = 0;
    static bool firstReading = true;
    static double lastValidFanTemp = 0;
    static bool firstFanReading = true;
    static int fanOverTempCount = 0;
    static bool fanTempSafetyArmedLogged = false;
    static bool fanTempSafetyDelayLogged = false;

    // Cycle 0, 2, 4, 6: Read Main Sensor (4 Hz)
    // Cycle 1, 3, 5, 7: Read Fan Sensor (4 Hz)
    // This ensures we never read both sensors in the same 125ms window
    if (readCycle % 2 == 0)
    {
      // Read thermocouple with failure detection
      double reading = thermocouple.readFarenheit();
      double previousAcceptedTemp = currentTemp;

      // 1. Range Check: MAX6675 returns ~2048°F when disconnected
      // We also check for negative values which are invalid for this application
      bool isRangeError = (reading < 0 || reading > SENSOR_FAULT_TEMP);
      if (isRangeError)
      {
        setLastRejectedBeanReadReason("range");
        LOG_WARNF("Temp range error ignored: raw=%.1fF accepted=%.1fF lastValid=%.1fF", reading, previousAcceptedTemp, lastValidTemp);
      }
      
      // 2. Spike Check: Ignore physically impossible temperature jumps
      bool isSpike = false;
      if (!isRangeError && !firstReading && shouldFilterBeanTempSpikes() && abs(reading - lastValidTemp) > MAX_TEMP_JUMP) {
        isSpike = true;
        setLastRejectedBeanReadReason("spike");
        LOG_WARNF("Temp spike ignored: lastValid=%.1fF raw=%.1fF accepted=%.1fF", lastValidTemp, reading, previousAcceptedTemp);
      }

      if (isRangeError || isSpike)
      {
        badReadingCount++;
        if (badReadingCount >= MAX_BAD_READINGS)
        {
          // SENSOR FAILURE - EMERGENCY STOP
          DEBUG_PRINTLN("EMERGENCY: Thermocouple failure detected!");
          String message = formatBeanSensorFaultMessage(reading);
          enterEmergencyErrorState("sensor_failed", message.c_str());
        }
      }
      else
      {
        currentTemp = reading;
        lastValidTemp = reading;
        firstReading = false;
        badReadingCount = 0; // Reset counter on good reading

        if (previousAcceptedTemp > 0.0) {
          bool suspiciousLowLatch = reading <= 40.0 && previousAcceptedTemp >= 80.0;
          bool largeAcceptedDrop = abs(reading - previousAcceptedTemp) >= 20.0;
          if (suspiciousLowLatch || largeAcceptedDrop) {
            LOG_WARNF("Bean temp accepted: prev=%.1fF raw=%.1fF new=%.1fF lastValid=%.1fF state=%d heater=%.1f", previousAcceptedTemp, reading, currentTemp, lastValidTemp, roasterState, heaterOutputVal);
          }
        }

        if (isRoastActiveState() && currentTemp > MAX_ROAST_TEMP)
        {
          DEBUG_PRINTLN("EMERGENCY: Roast temperature limit exceeded!");
          enterEmergencyErrorState("roast_over_temperature", "Roast Over Temp");
        }

        // THERMAL RUNAWAY PROTECTION
        if (roasterState != ERROR && currentTemp > MAX_SAFE_TEMP)
        {
          // EMERGENCY SHUTDOWN
          DEBUG_PRINTLN("EMERGENCY: Thermal runaway detected!");
          enterEmergencyErrorState("over_temperature", "Over Temp");
        }
      }
    }
    // Odd cycles: Read Fan Sensor (4 Hz)
    else
    {
      double fReading = thermocoupleFan.readFarenheit();
      const bool fanTempSafetyArmed = shouldEnforceFanTempSafety();
      bool isFanRangeError = (fReading <= 0 || fReading > SENSOR_FAULT_TEMP);
      if (isFanRangeError) {
        fanOverTempCount = 0;
        LOG_WARNF("Fan temp range error ignored: %.1f", fReading);
      }

      bool isFanSpike = false;
      if (!isFanRangeError && !firstFanReading && shouldFilterFanTempSpikes() && abs(fReading - lastValidFanTemp) > MAX_TEMP_JUMP) {
        isFanSpike = true;
        fanOverTempCount = 0;
        LOG_WARNF("Fan temp spike ignored: %.1f -> %.1f", lastValidFanTemp, fReading);
      }

      bool isImplausibleFanReading = false;
      if (!isFanRangeError && !isFanSpike && heaterOutputVal <= 0.0 && currentTemp < 140.0) {
        if (fReading > currentTemp + 30.0) {
          isImplausibleFanReading = true;
          fanOverTempCount = 0;
          LOG_WARNF("Fan temp implausible with heater off ignored: bean=%.1fF fan=%.1fF", currentTemp, fReading);
        }
      }

      if (!isFanRangeError && !isFanSpike && !isImplausibleFanReading) {
        fanTemp = fReading;
        lastValidFanTemp = fReading;
        firstFanReading = false;

        if (roasterState == ROASTING && !fanTempSafetyArmed) {
          if (roastStartedAtMs > 0 && !fanTempSafetyDelayLogged) {
            unsigned long warmupRemainingMs = FAN_TEMP_SAFETY_ARM_DELAY_MS;
            unsigned long elapsedRoastMs = millis() - roastStartedAtMs;
            if (elapsedRoastMs < FAN_TEMP_SAFETY_ARM_DELAY_MS) {
              warmupRemainingMs = FAN_TEMP_SAFETY_ARM_DELAY_MS - elapsedRoastMs;
            } else {
              warmupRemainingMs = 0;
            }

            LOG_INFOF("Fan temp safety delayed: remaining=%lums bean=%.1fF fan=%.1fF heater=%.1f",
                      warmupRemainingMs,
                      currentTemp,
                      fanTemp,
                      heaterOutputVal);
            fanTempSafetyDelayLogged = true;
          }
          fanTempSafetyArmedLogged = false;
        } else if (fanTempSafetyArmed && !fanTempSafetyArmedLogged) {
          LOG_INFOF("Fan temp safety armed: elapsed=%lus bean=%.1fF fan=%.1fF heater=%.1f threshold=%.1fF",
                    roastStartedAtMs > 0 ? (millis() - roastStartedAtMs) / 1000UL : 0UL,
                    currentTemp,
                    fanTemp,
                    heaterOutputVal,
                    MAX_SAFE_FAN_TEMP);
          fanTempSafetyArmedLogged = true;
        }

        // Require the over-temp reading to persist briefly so one noisy sample
        // cannot immediately trip the roaster into an error state.
        if (fanTempSafetyArmed && fanTemp > MAX_SAFE_FAN_TEMP) {
          fanOverTempCount++;
          LOG_WARNF("Fan temp over threshold (%d/3): %.1fF bean=%.1fF heater=%.1f elapsed=%lus",
                    fanOverTempCount,
                    fanTemp,
                    currentTemp,
                    heaterOutputVal,
                    roastStartedAtMs > 0 ? (millis() - roastStartedAtMs) / 1000UL : 0UL);
          if (fanOverTempCount >= 3) {
            DEBUG_PRINTLN("EMERGENCY: Fan/Exhaust Over Temp!");
            LOG_ERRORF("Fan temp safety trip: bean=%.1fF fan=%.1fF heater=%.1f elapsed=%lus threshold=%.1fF",
                       currentTemp,
                       fanTemp,
                       heaterOutputVal,
                       roastStartedAtMs > 0 ? (millis() - roastStartedAtMs) / 1000UL : 0UL,
                       MAX_SAFE_FAN_TEMP);
            enterEmergencyErrorState("fan_over_temperature", "Exhaust Over Temp");
          }
        } else {
          fanOverTempCount = 0;
        }
      }
    }

    readCycle = (readCycle + 1) % 8;
    // Metrics now available via debug console at /console
    checkTempTimer.reset();
  }

  if (controlLoopTimer.isReady())
  {
    unsigned long now = millis();
    updateRoastControl(now);
    updateCalibrationControl(now);
    systemLinkRecordHighRateSample();
    controlLoopTimer.reset();
  }

  if (stateMachineTimer.isReady())
  {
    static RoasterState lastState = IDLE;
    if (roasterState != lastState) {
        LOG_INFOF("State transition: %d -> %d", lastState, roasterState);
        
        // Handle state entry logic
        if (roasterState == CALIBRATING) {
             // Stop PID to prevent interference
             resetRoastControllerState();
             
             // Initialize fan ramp
             fanRampStep = 0;
             
             LOG_INFO("Calibration: Starting fan ramp sequence");
        }
        
        lastState = roasterState;
    }

    switch (roasterState)
    {
    case IDLE:
      digitalWrite(HEATER, LOW);
      digitalWrite(FAN, LOW);
      bdcFan.writeMicroseconds(800); // Ensure BDC stays at low speed
      bdcFanMs = 800;
      resetRoastControllerState();
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
        roastStartedAtMs = millis();
        profile.startProfile((int)currentTemp, millis());
        systemLinkMarkRoastingPhaseStarted();
        resetRoastControllerState();
        updateRoastControl(millis());
        
        // Log active PID parameters
        PIDRuntimeController::ControlDecision decision = pidRuntimeController.getLastDecision();
        LOG_INFOF("PID Active: Kp=%.4f, Ki=%.4f, Kd=%.4f, Schedule=%s, Band=%d, FF=%.1f",
                  decision.kp,
                  decision.ki,
                  decision.kd,
                  decision.scheduleActive ? "banded" : "single",
                  decision.bandIndex,
                  decision.feedforward);
        LOG_INFOF("Roast started: Target=%.0fF, Setpoints=%d", (float)getEffectiveFinalTargetTemp(), profile.getSetpointCount());
        LOG_INFOF("Fan temp safety warmup: delay=%lums threshold=%.1fF", FAN_TEMP_SAFETY_ARM_DELAY_MS, MAX_SAFE_FAN_TEMP);
        sendWsMessage("{ \"pushMessage\": \"startRoasting\" }");
      }

      // Set initial PWM fan speed
      fanRelay.setPWM(profile.getTargetFanSpeed(millis()));
      break;
    }

    case ROASTING:
    {
      if (roastShouldCompleteNow())
      {
        bool validationRun = currentRoastUsesValidationProfile();
        if (validationRun)
        {
          finalizeValidationIfRunning(true, "profile_complete");
        }

        resetRoastControllerState();
        heaterRelay.setPWM(heaterOutputVal);

        systemLinkMarkCoolingPhaseStarted(SYSTEMLINK_OUTCOME_PASSED,
                                          validationRun ? "validation_profile_complete" : "final_target_reached");
        enterCoolingState();

        setpointProgress = 0;
        LOG_INFOF("Roast complete at %.1fF - entering cooling phase", currentTemp);
      }

      DisplayTelemetry telemetry;
      telemetry.roasterState = roasterState;
      telemetry.currentTempF = (int)currentTemp;
      telemetry.targetTempF = (int)lround(setpointTemp);
      telemetry.fanPercent = (int)round(setpointFanSpeed * 100 / 255);
      telemetry.progressSeconds = setpointProgress;
      telemetry.elapsedSeconds = roastStartedAtMs > 0 ? static_cast<int>((millis() - roastStartedAtMs) / 1000UL) : -1;
      telemetry.heaterOutput = (int)lround(heaterOutputVal);
      telemetry.bdcFanMicros = bdcFanMs;
      telemetry.fanTempF = (int)lround(fanTemp);
      displayUpdateTelemetry(telemetry);
      break;
    }

    case COOLING:
    {
      digitalWrite(HEATER, LOW);

      DisplayTelemetry telemetry;
      telemetry.roasterState = roasterState;
      telemetry.currentTempF = (int)currentTemp;
      telemetry.targetTempF = COOLING_TARGET_TEMP;
      telemetry.fanPercent = 100;
      telemetry.bdcFanMicros = bdcFanMs;
      displayUpdateTelemetry(telemetry);

      // Check for cooling timeout (30 minutes max)
      unsigned long coolingDuration = millis() - coolingStartTime;
      if (coolingDuration > MAX_COOLING_TIME)
      {
        autoValidateAfterCooling = false;
        finalizeValidationIfRunning(false, "cooling_timeout");
        LOG_WARNF("Cooling timeout after %lu minutes - forcing IDLE", coolingDuration / 60000);
        systemLinkFinishRoast(SYSTEMLINK_OUTCOME_TERMINATED, "cooling_timeout");
        fanRelay.setPWM(0);
        bdcFan.writeMicroseconds(800);
        digitalWrite(FAN, LOW);
        roasterState = IDLE;
        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        displayShowScreen(DisplayScreen::Start);
        break;
      }

      if (currentTemp <= COOLING_TARGET_TEMP)
      {
        restoreValidationProfileIfNeeded();
        systemLinkFinishRoast(SYSTEMLINK_OUTCOME_NONE, "cooling_complete");
        fanRelay.setPWM(0);
        bdcFan.writeMicroseconds(800);
        digitalWrite(FAN, LOW);
        roasterState = IDLE;

        LOG_INFOF("Cooling complete at %.1fF - returning to IDLE", currentTemp);
        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        displayShowScreen(DisplayScreen::Start);

        // Auto-validate after step-response tuning
        if (autoValidateAfterCooling) {
          autoValidateAfterCooling = false;
          LOG_INFO("Starting auto-validation after step-response tuning");
          if (startValidationRoast(200.0, 70)) {
            sendWsMessage("{ \"pushMessage\": \"pidValidationStarted\" }");
          } else {
            LOG_WARN("Auto-validation could not start (temp or state issue)");
          }
        }
      }
      break;
    }

    case ERROR:
    {
      finalizeValidationIfRunning(false, "error_state");
      // ERROR state: Keep system in safe mode until manual reset
      // Heater must stay OFF, cooling fan at safe speed
      digitalWrite(HEATER, LOW);
      heaterRelay.setPWM(0);
      resetRoastControllerState();

      // Run cooling fan at safe speed (not maximum to avoid mechanical stress)
      fanRelay.setPWM(200);           // ~78% speed for sustained cooling
      bdcFan.writeMicroseconds(1500); // Mid-range for BDC fan
      bdcFanMs = 1500;

      // Update display with current temperature
      DisplayTelemetry telemetry;
      telemetry.roasterState = roasterState;
      telemetry.currentTempF = (int)currentTemp;
      telemetry.fanPercent = (int)round(200.0 * 100.0 / 255.0);
      telemetry.bdcFanMicros = bdcFanMs;
      displayUpdateTelemetry(telemetry);

      // ERROR state logged when entered, not every loop iteration

      // ERROR remains latched until the user explicitly requests a reset and
      // the controller reports safe temperatures with valid sensor data.
      break;
    }

    case CALIBRATING:
    {
      // Fan Ramp Logic (Reuse logic from START_ROAST)
      if (fanRampStep < 2000) 
      {
        if (fanRampStep == 0) {
             fanRampStartTime = millis();
             fanRampStep = 800;
             LOG_INFO("Calibration: Fan ramp initiated");
        }

        unsigned long elapsed = millis() - fanRampStartTime;
        int targetStep = 800 + (elapsed / 500) * 100;

        if (targetStep > fanRampStep)
        {
          fanRampStep = targetStep;
          if (fanRampStep > 2000) fanRampStep = 2000;
          
          bdcFan.writeMicroseconds(fanRampStep);
          // Ramp PWM fan proportionally
          int pwmFan = map(fanRampStep, 800, 2000, 50, 255);
          fanRelay.setPWM(pwmFan);
          LOG_INFOF("Calib Ramp: BDC=%d", fanRampStep);
        }
        
        // Ensure heater is OFF during ramp
        heaterRelay.setPWM(0);
        break; 
      }

      LOG_INFO("State: CALIBRATING loop");

      fanRelay.setPWM(setpointFanSpeed);
      int calibrationBdcValue = constrain(5 * setpointFanSpeed + 700, 800, 2000);
      bdcFan.writeMicroseconds(calibrationBdcValue);
      bdcFanMs = calibrationBdcValue;

      // Update UI
      double calSetpoint = stepTuner.getSetpoint();
      DisplayTelemetry telemetry;
      telemetry.roasterState = roasterState;
      telemetry.currentTempF = (int)currentTemp;
      telemetry.targetTempF = (int)round(calSetpoint);
      telemetry.fanPercent = (int)round(setpointFanSpeed * 100.0 / 255.0);
      telemetry.bdcFanMicros = bdcFanMs;
      displayUpdateTelemetry(telemetry);
      break;
    }

    default:
      LOG_WARNF("Unknown state: %d - returning to IDLE", roasterState);
      roasterState = IDLE;
      break;
    }
    stateMachineTimer.reset();
  }

  // Handle deferred restart (used after calibration)
  if (restartRequested && millis() >= restartAt)
  {
    LOG_INFO("Restarting controller after calibration...");
    delay(100);
    ESP.restart();
  }

  // Broadcast system state via WebSocket to debug console
  if (!otaUpdateInProgress && wsBroadcastTimer.isReady())
  {
    broadcastSystemState();
    broadcastLogs(50);  // Send last 50 log entries
    wsBroadcastTimer.reset();
  }

  if (!otaUpdateInProgress && roastTraceTimer.isReady())
  {
    if (roasterState == ROASTING && pidValidation.isActive())
    {
      pidValidation.recordSample(currentTemp, setpointTemp);
    }
    systemLinkRecordRoastSample();
    roastTraceTimer.reset();
  }
}



void handleStartRoastCommand()
{
  LOG_INFO("Start roast command received");
  
  // Use the currently active profile (managed by web UI)
  int spCount = profile.getSetpointCount();
  LOG_INFOF("Starting roast with active profile (%d setpoints)", spCount);
  
  if (spCount == 0) {
    LOG_ERROR("Cannot start roast - profile has no setpoints!");
    displayShowErrorMessage("No Profile");
    return;
  }
  
  int uiFinalTemp = readDisplayFinalTargetTempWithRetry();
  if (uiFinalTemp != DISPLAY_READ_ERROR && uiFinalTemp > 0) {
    finalTempOverride = constrain(uiFinalTemp, 0, 500);
    LOG_INFOF("Using display final target override: %dF", finalTempOverride);
    // Also update the active profile's final setpoint so heater control uses the override
    profile.setFinalTargetTemp(finalTempOverride);
  } else {
    finalTempOverride = profile.getFinalTargetTemp();
    LOG_WARN("Display final target not available - using profile final temp");
  }
  
  startRoastSession();
  LOG_INFO("Start roast command complete - state set to START_ROAST");
}

void handleStopRoastCommand()
{
  if (roasterState == ERROR)
  {
    if (canClearErrorState())
    {
      clearEmergencyErrorState();
    }
    else
    {
      String blockedMessage = formatErrorRecoveryBlockedMessage();
      setActiveFault(activeFaultCode, blockedMessage.c_str());
      displayShowErrorMessage(activeFaultMessage);
      LOG_WARNF("Error reset blocked: fault=%s bean=%.1fF fan=%.1fF badReadings=%d", activeFaultCode, currentTemp, fanTemp, badReadingCount);
    }
    return;
  }

  finalizeValidationIfRunning(false, "user_stop");
  systemLinkMarkCoolingPhaseStarted(SYSTEMLINK_OUTCOME_TERMINATED, "user_stop");

  resetRoastControllerState();
  heaterRelay.setPWM(heaterOutputVal);
  digitalWrite(HEATER, LOW);

  enterCoolingState();
}

void handleStopCoolingCommand()
{
  finalizeValidationIfRunning(false, "cooling_skipped");
  restoreValidationProfileIfNeeded();
  roasterState = IDLE;
  displayShowScreen(DisplayScreen::Start);
}

void handleApplyWifiCommand()
{
  // Update global WiFi credentials
  DisplayWifiFormState form = displayReadWifiFormState();
  WifiCredentials requestedCredentials;
  requestedCredentials.ssid = form.ssid;
  requestedCredentials.password = form.password;
  wifiCredentials = normalizeWifiCredentials(requestedCredentials);
  displaySetWifiFormState(DisplayWifiFormState{wifiCredentials.ssid, wifiCredentials.password});
  displaySetWifiIp("Connecting...");
  String ip = initializeWifi(wifiCredentials);
  displaySetWifiIp(ip);
  if (WiFi.status() == WL_CONNECTED)
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }
}

void handleOpenActiveProfileCommand()
{
  LOG_INFO("Open active profile command received");
  onProfileActivePageEnter();
  displayShowScreen(DisplayScreen::ProfileActive);
}
