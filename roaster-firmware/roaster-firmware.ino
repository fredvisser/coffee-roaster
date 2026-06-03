#include "Arduino.h"
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
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
#include "BoardConfig.hpp"
#include "DisplayBackendConfig.hpp"
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
#include <EasyNextionLibrary.h>
#endif
#include "DebugLog.hpp"
#include "DisplayAdapter.hpp"
#include "PIDController.hpp"
#include "Profiles.hpp"
#include "ProfileManager.hpp"
#include "ProfileEditor.hpp"
#include "StepResponseTuner.hpp"
#include "PIDRuntimeController.hpp"
#include "PIDValidation.hpp"
#include "SystemLink.hpp"
#include "Network.hpp"

Preferences preferences;

#define HW_SERIAL Serial0

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

#define VERSION "2025-12-24"

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

// Create a roast profile object
Profiles profile;
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

// Roaster state variable (enum defined in Types.hpp)
RoasterState roasterState = IDLE;

MAX6675 thermocouple(THERMOCOUPLE_SCK, TC1_CS, THERMOCOUPLE_MISO);
MAX6675 thermocoupleFan(THERMOCOUPLE_SCK, TC2_CS, THERMOCOUPLE_MISO);
PIDController heaterPID(&currentTemp, &setpointTemp, &heaterPidTrimVal, 0, 255, kp, ki, kd);
StepResponseTuner stepTuner;
bool autoValidateAfterCooling = false;
PIDRuntimeController pidRuntimeController;
PIDValidationSession pidValidation;

// Create a Nextion display connection
#if ROASTER_DISPLAY_BACKEND == ROASTER_DISPLAY_BACKEND_NEXTION
EasyNex myNex(HW_SERIAL);
#endif

uint8_t profileBuffer[200];
int finalTempOverride = -1; // Nextion override for final target temp (F)
Profiles validationSavedProfile;
bool validationProfileLoaded = false;
int savedValidationFinalTempOverride = -1;
unsigned long roastStartedAtMs = 0;
double appliedKp = kp;
double appliedKi = ki;
double appliedKd = kd;
bool pidScheduleConfigured = false;
bool pidScheduleActive = false;
int activePidBandIndex = -1;

// Helper function for reliable Nextion reads with retry logic
int readNextionWithRetry(const char *component, int retries = 2)
{
  for (int i = 0; i < retries; i++)
  {
    int value = displayReadNumber(component);
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

void applyHeaterPIDGains(double newKp, double newKi, double newKd)
{
  if (fabs(appliedKp - newKp) < 0.0001 && fabs(appliedKi - newKi) < 0.0001 && fabs(appliedKd - newKd) < 0.0001)
  {
    return;
  }

  heaterPID.setGains(newKp, newKi, newKd);
  appliedKp = newKp;
  appliedKi = newKi;
  appliedKd = newKd;
}

void resetRoastControllerState()
{
  heaterPID.stop();
  heaterPidTrimVal = 0;
  heaterFeedforwardVal = 0;
  heaterOutputVal = 0;
  pidScheduleActive = false;
  activePidBandIndex = -1;
  pidRuntimeController.resetForRoast();
  applyHeaterPIDGains(kp, ki, kd);
}

void setManualPIDGains(double newKp, double newKi, double newKd)
{
  kp = newKp;
  ki = newKi;
  kd = newKd;

  preferences.putDouble("kp", kp);
  preferences.putDouble("ki", ki);
  preferences.putDouble("kd", kd);

  pidRuntimeController.setFallbackGains(kp, ki, kd);
  pidRuntimeController.clearPreferences(preferences);
  pidScheduleConfigured = false;
  resetRoastControllerState();
}

void updateRoastControl(unsigned long now)
{
  if (roasterState != ROASTING)
  {
    return;
  }

  setpointTemp = profile.getTargetTemp(now);
  setpointFanSpeed = profile.getTargetFanSpeed(now);
  setpointProgress = profile.getProfileProgress(now);

  PIDRuntimeController::ControlDecision decision = pidRuntimeController.decide(now, currentTemp, setpointTemp, fanTemp);
  pidScheduleActive = decision.scheduleActive;
  activePidBandIndex = decision.bandIndex;
  applyHeaterPIDGains(decision.kp, decision.ki, decision.kd);

  heaterPID.run();

  heaterFeedforwardVal = decision.feedforward;
  heaterOutputVal = constrain(heaterPidTrimVal + heaterFeedforwardVal, 0.0, 255.0);
  fanRelay.setPWM(setpointFanSpeed);

  int bdcValue = constrain(5 * setpointFanSpeed + 700, 800, 2000);
  bdcFan.writeMicroseconds(bdcValue);
  bdcFanMs = bdcValue;

  heaterRelay.setPWM(heaterOutputVal);
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

bool currentRoastUsesValidationProfile()
{
  return validationProfileLoaded;
}

void restoreValidationProfileIfNeeded()
{
  if (!validationProfileLoaded)
  {
    return;
  }

  profile = validationSavedProfile;
  finalTempOverride = savedValidationFinalTempOverride;
  validationProfileLoaded = false;
}

void finalizeValidationIfRunning(bool completed, const char *reason)
{
  if (!pidValidation.isActive())
  {
    return;
  }

  double durationSeconds = 0.0;
  if (roastStartedAtMs > 0)
  {
    durationSeconds = (millis() - roastStartedAtMs) / 1000.0;
  }

  pidValidation.finish(completed, durationSeconds, reason);
  restoreValidationProfileIfNeeded();
}

bool roastShouldCompleteNow()
{
  if (currentRoastUsesValidationProfile())
  {
    return profile.getProfileProgress(millis()) >= 100;
  }

  return currentTemp >= getEffectiveFinalTargetTemp();
}

void startRoastSession()
{
  roastStartedAtMs = 0;
  roasterState = START_ROAST;
  systemLinkMarkRoastStarted();
  displaySetTargetTemp((int)lround(setpointTemp));
  displayShowScreen(DisplayScreen::Roasting);
}

bool startValidationRoast(double finalTargetTemp, uint32_t fanPercent)
{
  if (roasterState != IDLE)
  {
    return false;
  }

  if (currentTemp > 180.0)
  {
    return false;
  }

  validationSavedProfile = profile;
  savedValidationFinalTempOverride = finalTempOverride;
  pidValidation.start(profile, currentTemp, finalTargetTemp, fanPercent);
  validationProfileLoaded = true;
  finalTempOverride = constrain((int)lround(finalTargetTemp), 0, 500);
  startRoastSession();
  return true;
}

void enterCoolingState()
{
  setpointTemp = COOLING_TARGET_TEMP;
  roasterState = COOLING;
  coolingStartTime = millis();
  resetRoastControllerState();

  setpointFanSpeed = 255;
  fanRelay.setPWM(setpointFanSpeed);
  bdcFan.writeMicroseconds(2000);
  bdcFanMs = 2000;

  displaySetTargetTemp(COOLING_TARGET_TEMP);
  displayShowScreen(DisplayScreen::Cooling);
}

DisplayScreen displayScreenForCurrentState()
{
  switch (roasterState)
  {
  case START_ROAST:
  case ROASTING:
    return DisplayScreen::Roasting;
  case COOLING:
    return DisplayScreen::Cooling;
  case ERROR:
    return DisplayScreen::Error;
  case IDLE:
  case CALIBRATING:
  default:
    return DisplayScreen::Start;
  }
}

bool cycleActiveProfile(int direction)
{
  if (roasterState != IDLE)
  {
    LOG_WARN("Ignoring profile change request while roaster is not idle");
    return false;
  }

  std::vector<String> profileIds = getProfileIds();
  if (profileIds.empty())
  {
    LOG_WARN("No profiles available to cycle");
    return false;
  }

  String activeId = getActiveProfileId();
  size_t activeIndex = 0;
  bool foundActive = false;
  for (size_t index = 0; index < profileIds.size(); ++index)
  {
    if (profileIds[index] == activeId)
    {
      activeIndex = index;
      foundActive = true;
      break;
    }
  }

  if (!foundActive)
  {
    activeIndex = 0;
  }

  int nextIndex = static_cast<int>(activeIndex) + direction;
  if (nextIndex < 0)
  {
    nextIndex = static_cast<int>(profileIds.size()) - 1;
  }
  else if (nextIndex >= static_cast<int>(profileIds.size()))
  {
    nextIndex = 0;
  }

  String nextId = profileIds[nextIndex];
  if (!profileManager.loadProfile(nextId))
  {
    LOG_WARNF("Failed to load profile id=%s", nextId.c_str());
    return false;
  }

  setActiveProfileId(nextId);
  finalTempOverride = profile.getFinalTargetTemp();
  displaySetFinalTargetTemp(getEffectiveFinalTargetTemp());
  onProfileActivePageEnter();
  displayShowScreen(DisplayScreen::ProfileActive);
  LOG_INFOF("Cycled active profile to id=%s", nextId.c_str());
  return true;
}

void handleDisplayAction(DisplayAction action)
{
  switch (action)
  {
  case DisplayAction::StartRoast:
    trigger0();
    break;
  case DisplayAction::OpenNetwork:
    displayShowScreen(DisplayScreen::Network);
    break;
  case DisplayAction::StopRoast:
    trigger1();
    break;
  case DisplayAction::StopCooling:
    trigger2();
    break;
  case DisplayAction::ApplyWifi:
    trigger3();
    break;
  case DisplayAction::OpenActiveProfile:
    trigger4();
    break;
  case DisplayAction::PreviousProfile:
    cycleActiveProfile(-1);
    break;
  case DisplayAction::NextProfile:
    cycleActiveProfile(1);
    break;
  case DisplayAction::ReturnToStateScreen:
    displayShowScreen(displayScreenForCurrentState());
    break;
  case DisplayAction::None:
  default:
    break;
  }
}

void handleDisplayActions()
{
  DisplayAction action = DisplayAction::None;
  while (displayPopAction(action))
  {
    handleDisplayAction(action);
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
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL);
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

  // Update Nextion display with current profile values
  // Send final target temp override (moved from reloadActiveProfile to avoid blocking)
  // If no override is set (-1), this will send the profile's default final target
  LOG_INFOF("Original globals.setTempNum.val value is %dF", displayReadNumber("globals.setTempNum.val"));
  displaySetFinalTargetTemp(getEffectiveFinalTargetTemp());
  LOG_INFOF("Set Nextion final target temp to %dF", getEffectiveFinalTargetTemp());
  LOG_INFOF("Final globals.setTempNum.val value is %dF", displayReadNumber("globals.setTempNum.val"));

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
    displayTick();
    handleDisplayActions();
    heaterRelay.tick();
    fanRelay.tick();
    wsCleanup();
    ElegantOTA.loop(); // Handle OTA updates
    tickTimer.reset();
  }

  if (checkTempTimer.isReady())
  {
    static int readCycle = 0; // 0-7 counter for 1Hz fan updates
    static double lastValidTemp = 0;
    static bool firstReading = true;

    // Cycle 0, 2, 4, 6: Read Main Sensor (4 Hz)
    // Cycle 1, 3, 5, 7: Read Fan Sensor (4 Hz)
    // This ensures we never read both sensors in the same 125ms window
    if (readCycle % 2 == 0)
    {
      // Read thermocouple with failure detection
      double reading = thermocouple.readFarenheit();

      // 1. Range Check: MAX6675 returns ~2048°F when disconnected
      // We also check for negative values which are invalid for this application
      bool isRangeError = (reading < 0 || reading > SENSOR_FAULT_TEMP);
      
      // 2. Spike Check: Ignore physically impossible temperature jumps
      bool isSpike = false;
      if (!isRangeError && !firstReading && abs(reading - lastValidTemp) > MAX_TEMP_JUMP) {
        isSpike = true;
        LOG_WARNF("Temp spike ignored: %.1f -> %.1f", lastValidTemp, reading);
      }

      if (isRangeError || isSpike)
      {
        badReadingCount++;
        if (badReadingCount >= MAX_BAD_READINGS)
        {
          // SENSOR FAILURE - EMERGENCY STOP
          roasterState = ERROR;
          digitalWrite(HEATER, LOW);
          resetRoastControllerState();
          heaterRelay.setPWM(0);
          fanRelay.setPWM(255); // Full fan for safety
          bdcFan.writeMicroseconds(2000);
          bdcFanMs = 2000;
          systemLinkUpdateLastFault("sensor_failed");
          systemLinkFinishRoast(SYSTEMLINK_OUTCOME_ERRORED, "sensor_failed");
          DEBUG_PRINTLN("EMERGENCY: Thermocouple failure detected!");
          displayShowErrorMessage("Sensor Failed");
        }
      }
      else
      {
        currentTemp = reading;
        lastValidTemp = reading;
        firstReading = false;
        badReadingCount = 0; // Reset counter on good reading

        // THERMAL RUNAWAY PROTECTION
        if (currentTemp > MAX_SAFE_TEMP)
        {
          // EMERGENCY SHUTDOWN
          roasterState = ERROR;
          digitalWrite(HEATER, LOW);
          resetRoastControllerState();
          heaterRelay.setPWM(0);
          fanRelay.setPWM(255); // Full fan to cool down
          bdcFan.writeMicroseconds(2000);
          bdcFanMs = 2000;
          systemLinkUpdateLastFault("over_temperature");
          systemLinkFinishRoast(SYSTEMLINK_OUTCOME_ERRORED, "over_temperature");
          DEBUG_PRINTLN("EMERGENCY: Thermal runaway detected!");
          displayShowErrorMessage("Over Temp");
        }
      }
    }
    // Odd cycles: Read Fan Sensor (4 Hz)
    else
    {
      double fReading = thermocoupleFan.readFarenheit();
      // Simple range check for fan sensor
      if (fReading > 0 && fReading < SENSOR_FAULT_TEMP) {
        fanTemp = fReading;
        
        // Safety check for fan sensor (cold inlet temp)
        if (fanTemp > MAX_SAFE_FAN_TEMP) {
          // EMERGENCY SHUTDOWN
          roasterState = ERROR;
          digitalWrite(HEATER, LOW);
          resetRoastControllerState();
          heaterRelay.setPWM(0);
          fanRelay.setPWM(255); // Full fan to cool down
          bdcFan.writeMicroseconds(2000);
          bdcFanMs = 2000;
          systemLinkUpdateLastFault("fan_over_temperature");
          systemLinkFinishRoast(SYSTEMLINK_OUTCOME_ERRORED, "fan_over_temperature");
          DEBUG_PRINTLN("EMERGENCY: Fan/Exhaust Over Temp!");
          displayShowErrorMessage("Exhaust Over Temp");
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

      // Only exit ERROR state through hardware reset
      // No automatic recovery to ensure user acknowledges the fault
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
  if (wsBroadcastTimer.isReady())
  {
    broadcastSystemState();
    broadcastLogs(50);  // Send last 50 log entries
    wsBroadcastTimer.reset();
  }

  if (roastTraceTimer.isReady())
  {
    if (roasterState == ROASTING && pidValidation.isActive())
    {
      pidValidation.recordSample(currentTemp, setpointTemp);
    }
    systemLinkRecordRoastSample();
    roastTraceTimer.reset();
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
    displayShowErrorMessage("No Profile");
    return;
  }
  
  int uiFinalTemp = readNextionWithRetry("globals.setTempNum.val");
  if (uiFinalTemp != NEXTION_READ_ERROR && uiFinalTemp > 0) {
    finalTempOverride = constrain(uiFinalTemp, 0, 500);
    LOG_INFOF("Using Nextion final target override: %dF", finalTempOverride);
    // Also update the active profile's final setpoint so heater control uses the override
    profile.setFinalTargetTemp(finalTempOverride);
  } else {
    finalTempOverride = profile.getFinalTargetTemp();
    LOG_WARN("Nextion final target not available - using profile final temp");
  }
  
  startRoastSession();
  LOG_INFO("trigger0() complete - state set to START_ROAST");
}

void trigger1()
{ // Stop roast command received
  finalizeValidationIfRunning(false, "user_stop");
  systemLinkMarkCoolingPhaseStarted(SYSTEMLINK_OUTCOME_TERMINATED, "user_stop");

  resetRoastControllerState();
  heaterRelay.setPWM(heaterOutputVal);
  digitalWrite(HEATER, LOW);

  enterCoolingState();
}

void trigger2()
{ // Stop cooling command received
  finalizeValidationIfRunning(false, "cooling_skipped");
  restoreValidationProfileIfNeeded();
  roasterState = IDLE;
  displayShowScreen(DisplayScreen::Start);
}

void trigger3()
{ // Apply WiFi credentials
  // Update global WiFi credentials
  DisplayWifiFormState form = displayReadWifiFormState();
  WifiCredentials requestedCredentials;
  requestedCredentials.ssid = form.ssid;
  requestedCredentials.password = form.password;
  wifiCredentials = normalizeWifiCredentials(requestedCredentials);
  displaySetWifiIp("Connecting...");
  String ip = initializeWifi(wifiCredentials);
  displaySetWifiIp(ip);
  if (WiFi.status() == WL_CONNECTED)
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }
}

void trigger4()
{ // ProfileActive page button - plot current profile
  LOG_INFO("trigger4() called - ProfileActive plot button");
  onProfileActivePageEnter();
  displayShowScreen(DisplayScreen::ProfileActive);
}
