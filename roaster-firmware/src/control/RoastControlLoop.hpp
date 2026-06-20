#ifndef ROAST_CONTROL_LOOP_HPP
#define ROAST_CONTROL_LOOP_HPP

#include <Arduino.h>
#include <Preferences.h>
#include <PWMrelay.h>
#include <ESP32Servo.h>

#include "PIDController.hpp"
#include "PIDRuntimeController.hpp"
#include "../platform/RoasterTypes.hpp"
#include "../profiles/RoastProfile.hpp"

extern Preferences preferences;

extern double kp;
extern double ki;
extern double kd;
extern double currentTemp;
extern double setpointTemp;
extern double heaterOutputVal;
extern double heaterPidTrimVal;
extern double heaterFeedforwardVal;
extern double fanTemp;
extern double appliedKp;
extern double appliedKi;
extern double appliedKd;

extern byte setpointFanSpeed;
extern int setpointProgress;
extern int bdcFanMs;
extern int activePidBandIndex;

extern bool pidScheduleConfigured;
extern bool pidScheduleActive;

extern RoasterState roasterState;

extern PWMrelay heaterRelay;
extern PWMrelay fanRelay;
extern Servo bdcFan;

extern RoastProfile profile;
extern PIDController heaterPID;
extern PIDRuntimeController pidRuntimeController;

inline void applyHeaterPIDGains(double newKp, double newKi, double newKd)
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

inline void resetRoastControllerState()
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

inline void setManualPIDGains(double newKp, double newKi, double newKd)
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

inline void updateRoastControl(unsigned long now)
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

#endif // ROAST_CONTROL_LOOP_HPP