#ifndef ROAST_SESSION_LIFECYCLE_HPP
#define ROAST_SESSION_LIFECYCLE_HPP

#include <Arduino.h>
#include <atomic>
#include <Preferences.h>
#include <PWMrelay.h>
#include <ESP32Servo.h>

#include "../display/DisplayAdapter.hpp"
#include "../platform/RoasterTypes.hpp"
#include "../profiles/RoastProfile.hpp"
#include "PIDValidation.hpp"

extern Preferences preferences;
extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern int bdcFanMs;
extern int finalTempOverride;
extern unsigned long coolingStartTime;
extern unsigned long roastStartedAtMs;

extern bool validationProfileLoaded;
extern bool beanSensorHasValidReading;
extern bool fanSensorHasValidReading;

extern std::atomic<bool> roastDisplayRefreshRequested;

extern PWMrelay fanRelay;
extern PWMrelay heaterRelay;
extern Servo bdcFan;

extern RoastProfile profile;
extern RoastProfile validationSavedProfile;
extern PIDValidationSession pidValidation;

extern int savedValidationFinalTempOverride;

uint32_t getEffectiveFinalTargetTemp();
void resetRoastControllerState();
static void systemLinkMarkRoastStarted(bool validationRun);

inline bool currentRoastUsesValidationProfile()
{
  return validationProfileLoaded;
}

inline void restoreValidationProfileIfNeeded()
{
  if (!validationProfileLoaded)
  {
    return;
  }

  profile = validationSavedProfile;
  finalTempOverride = savedValidationFinalTempOverride;
  validationProfileLoaded = false;
}

inline void finalizeValidationIfRunning(bool completed, const char *reason)
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

inline bool roastShouldCompleteNow()
{
  if (currentRoastUsesValidationProfile())
  {
    return profile.getProfileProgress(millis()) >= 100;
  }

  return currentTemp >= getEffectiveFinalTargetTemp();
}

static bool systemLinkCanStartRoast();

inline bool startRoastSession(bool idleMutationReserved = false)
{
  if (!systemLinkCanStartRoast() ||
      !beanSensorHasValidReading || !fanSensorHasValidReading ||
  !(idleMutationReserved ? transitionIdleMutationTo(START_ROAST) : trySetRoasterState(IDLE, START_ROAST)))
  {
    return false;
  }

  roastStartedAtMs = 0;
  preferences.putBool("roast_active", true);
  systemLinkMarkRoastStarted(pidValidation.isActive());
  roastDisplayRefreshRequested.store(true, std::memory_order_release);
  return true;
}

inline bool startValidationRoast(double finalTargetTemp, uint32_t fanPercent, bool idleMutationReserved = false)
{
  if (getRoasterStateSnapshot() != IDLE)
  {
    return false;
  }

  if (currentTemp > 180.0)
  {
    return false;
  }

  if (!beanSensorHasValidReading || !fanSensorHasValidReading)
  {
    return false;
  }

  validationSavedProfile = profile;
  savedValidationFinalTempOverride = finalTempOverride;
  pidValidation.start(profile, currentTemp, finalTargetTemp, fanPercent);
  validationProfileLoaded = true;
  finalTempOverride = constrain((int)lround(finalTargetTemp), 0, 500);
  if (!startRoastSession(idleMutationReserved))
  {
    restoreValidationProfileIfNeeded();
    pidValidation.finish(false, 0.0, "start_rejected");
    return false;
  }
  return true;
}

inline void enterCoolingState()
{
  heaterRelay.setPWM(0);
  setpointTemp = COOLING_TARGET_TEMP;
  setRoasterState(COOLING);
  coolingStartTime = millis();
  resetRoastControllerState();

  setpointFanSpeed = 255;
  fanRelay.setPWM(setpointFanSpeed);
  bdcFan.writeMicroseconds(2000);
  bdcFanMs = 2000;

  displaySetTargetTemp(COOLING_TARGET_TEMP);
  displayShowScreen(DisplayScreen::Cooling);
}

inline DisplayScreen displayScreenForCurrentState()
{
  switch (getRoasterStateSnapshot())
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

#endif // ROAST_SESSION_LIFECYCLE_HPP