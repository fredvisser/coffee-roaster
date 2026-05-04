/**
 * Sawtooth PID Tuning Workflow Unit Tests
 *
 * Tests for the sawtooth-based PID tuning configuration and startup guards.
 */

#include <AUnit.h>
#include "../../PIDAutotuner.hpp"
#include "../../PIDValidation.hpp"

using namespace aunit;

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ;
  delay(1000);
}

void loop()
{
  TestRunner::run();
}

test(SawtoothTuner_StartConfiguresWorkflow)
{
  PIDAutotuner tuner;
  tuner.start(80.0, 4.0, 0.05, 0.25, 4);

  PIDAutotuner::CharacterizationSummary summary = tuner.getSummary();
  assertTrue(tuner.isRunning());
  assertEqual(String("PREHEAT_RAMP"), String(tuner.getPhaseName()));
  assertEqual((uint8_t)4, summary.maxCycles);
  assertEqual(150.0, summary.sawtoothStartTemp);
  assertEqual(200.0, summary.sawtoothEndTemp);
  assertEqual(0.5, summary.rampRateFps);
}

test(PIDValidation_BuildsOvershootCheckProfile)
{
  Profiles profile;
  PIDValidationSession::buildValidationProfile(profile, 70);

  assertEqual(6, profile.getSetpointCount());
  assertEqual((uint32_t)120, profile.getTargetTempAtTime(0));
  assertEqual((uint32_t)120, profile.getTargetTempAtTime(15000));
  assertEqual((uint32_t)150, profile.getTargetTempAtTime(16000));
  assertEqual((uint32_t)175, profile.getTargetTempAtTime(66000));
  assertEqual((uint32_t)200, profile.getTargetTempAtTime(116000));
  assertEqual((uint32_t)200, profile.getTargetTempAtTime(131000));
}

test(SawtoothTuner_ClampsCycleCountRange)
{
  PIDAutotuner lowCycles;
  lowCycles.start(80.0, 4.0, 0.05, 0.25, 1);
  assertEqual((uint8_t)3, lowCycles.getSummary().maxCycles);

  PIDAutotuner highCycles;
  highCycles.start(80.0, 4.0, 0.05, 0.25, 9);
  assertEqual((uint8_t)5, highCycles.getSummary().maxCycles);
}

test(SawtoothTuner_RejectsHotStart)
{
  PIDAutotuner tuner;
  tuner.start(170.0, 4.0, 0.05, 0.25, 4);

  assertFalse(tuner.isRunning());
  assertEqual(String("start_temp_too_high"), String(tuner.getLastError()));
}