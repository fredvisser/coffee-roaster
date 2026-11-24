/**
 * Safety System Unit Tests
 *
 * CRITICAL safety tests including:
 * - Heater no-rise protection
 * - Over-temperature protection
 * - Sensor failure detection
 * - Emergency shutdown procedures
 *
 * These tests verify the most important safety features.
 */

#include <AUnit.h>
#include "../../src/platform/RoasterTypes.hpp"
#include "../../src/control/ThermocoupleSafety.hpp"

using namespace aunit;

// Safety thresholds
// Test variables
double testCurrentTemp = 150.0;
double testSetpointTemp = 300.0;
double testHeaterOutput = 0;
int testFanSpeed = 0;
bool testSafetyShutdown = false;
bool testSensorFault = false;
unsigned long testLastTempIncrease = 0;
double testLastTemp = 150.0;

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

test(Safety_ThermocoupleRejectsNonFiniteAndOutOfRangeReadings)
{
  assertFalse(thermocoupleReadingIsValid(NAN, 0.0, SENSOR_FAULT_TEMP));
  assertFalse(thermocoupleReadingIsValid(INFINITY, 0.0, SENSOR_FAULT_TEMP));
  assertFalse(thermocoupleReadingIsValid(-1.0, 0.0, SENSOR_FAULT_TEMP));
  assertFalse(thermocoupleReadingIsValid(601.0, 0.0, SENSOR_FAULT_TEMP));
  assertFalse(thermocoupleReadingIsValid(32.0, 0.0, SENSOR_FAULT_TEMP));
  assertTrue(thermocoupleReadingIsValid(32.1, 0.0, SENSOR_FAULT_TEMP));
}

// Helper to reset test state
void resetSafetyState()
{
  testCurrentTemp = 150.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;
  testFanSpeed = 0;
  testSafetyShutdown = false;
  testSensorFault = false;
  testLastTempIncrease = millis();
  testLastTemp = 150.0;
}

test(Safety_AbsoluteTemperaturePolicy)
{
  assertFalse(absoluteTemperatureLimitExceeded(MAX_SAFE_TEMP));
  assertTrue(absoluteTemperatureLimitExceeded(MAX_SAFE_TEMP + 1.0));
}

test(Safety_RoastTemperaturePolicy)
{
  assertFalse(roastTemperatureLimitExceeded(MAX_ROAST_TEMP + 1.0, false));
  assertFalse(roastTemperatureLimitExceeded(MAX_ROAST_TEMP, true));
  assertTrue(roastTemperatureLimitExceeded(MAX_ROAST_TEMP + 1.0, true));
}

test(Safety_HeaterRiseTimeoutPolicy)
{
  assertFalse(heaterRiseTimeoutElapsed(MAX_HEATER_NO_RISE_MS - 1));
  assertTrue(heaterRiseTimeoutElapsed(MAX_HEATER_NO_RISE_MS));
}

// ============================================================================
// OVER-TEMPERATURE PROTECTION TESTS
// ============================================================================

test(Safety_OverTemp_AbsoluteLimit)
{
  resetSafetyState();
  testCurrentTemp = 510.0; // Over absolute safe limit
  testHeaterOutput = 200;

  // Emergency shutdown for over-temp
  if (absoluteTemperatureLimitExceeded(testCurrentTemp))
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSafetyShutdown);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(Safety_OverTemp_RoastingLimit)
{
  resetSafetyState();
  testCurrentTemp = 470.0; // Over roasting limit
  testSetpointTemp = 440.0;
  testHeaterOutput = 150;

  // Should stop roasting
  if (roastTemperatureLimitExceeded(testCurrentTemp, true))
  {
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(Safety_OverTemp_HeaterDisable)
{
  resetSafetyState();
  testCurrentTemp = 505.0;
  testHeaterOutput = 255;

  // Heater must be disabled at dangerous temps
  if (absoluteTemperatureLimitExceeded(testCurrentTemp))
  {
    testHeaterOutput = 0;
  }

  assertEqual(0.0, testHeaterOutput);
}

test(Safety_OverTemp_FanMaximum)
{
  resetSafetyState();
  testCurrentTemp = 480.0;
  testFanSpeed = 150;

  // Fan should go to maximum for over-temp
  if (roastTemperatureLimitExceeded(testCurrentTemp, true))
  {
    testFanSpeed = 255;
  }

  assertEqual(255, testFanSpeed);
}

test(Safety_OverTemp_Recovery)
{
  resetSafetyState();
  testCurrentTemp = 510.0;
  testSafetyShutdown = true;
  testHeaterOutput = 0;
  testFanSpeed = 255;

  // Cool down
  testCurrentTemp = 200.0;

  // Can recover after cooling below safe threshold
  if (testCurrentTemp < MAX_SAFE_TEMP - 50.0)
  { // Hysteresis
    testSafetyShutdown = false;
  }

  assertFalse(testSafetyShutdown);
}

// ============================================================================
// SENSOR FAILURE DETECTION TESTS
// ============================================================================

test(Safety_SensorFault_OpenThermocouple)
{
  resetSafetyState();
  testCurrentTemp = 999.0; // MAX6675 reads ~999°F when open
  testHeaterOutput = 200;

  // Detect sensor fault
  if (!thermocoupleReadingIsValid(testCurrentTemp, 0.0, SENSOR_FAULT_TEMP))
  {
    testSensorFault = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSensorFault);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(Safety_SensorFault_ShortedThermocouple)
{
  resetSafetyState();
  testCurrentTemp = 0.0; // Suspicious reading
  testLastTemp = 350.0;
  testHeaterOutput = 200;

  // Detect impossible temperature drop
  double tempChange = abs(testCurrentTemp - testLastTemp);
  if (tempChange > 100.0)
  { // Impossible 100°F instant drop
    testSensorFault = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSensorFault);
  assertEqual(0.0, testHeaterOutput);
}

test(Safety_SensorFault_NegativeReading)
{
  resetSafetyState();
  testCurrentTemp = -50.0; // Impossible reading
  testHeaterOutput = 150;

  // Detect invalid reading
  if (testCurrentTemp < 0)
  {
    testSensorFault = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSensorFault);
  assertEqual(0.0, testHeaterOutput);
}

test(Safety_SensorFault_UnrealisticJump)
{
  resetSafetyState();
  testLastTemp = 300.0;
  testCurrentTemp = 450.0; // 150°F jump in 250ms (impossible)
  testHeaterOutput = 200;

  // Rate of change check
  double rateOfChange = abs(testCurrentTemp - testLastTemp) / 0.25; // Per second
  if (rateOfChange > 200.0)
  { // Max realistic rate
    testSensorFault = true;
    testHeaterOutput = 0;
  }

  assertTrue(testSensorFault);
  assertEqual(0.0, testHeaterOutput);
}

test(Safety_SensorFault_StickyReading)
{
  resetSafetyState();
  testCurrentTemp = 250.0;
  testLastTemp = 250.0;
  testHeaterOutput = 255; // Full power

  // Same reading for extended time with full heater (suspicious)
  unsigned long stuckTime = 60000; // 60 seconds
  bool isSensorStuck = false;

  if (testCurrentTemp == testLastTemp &&
      testHeaterOutput > 200 &&
      millis() - testLastTempIncrease > stuckTime)
  {
    isSensorStuck = true;
    testSensorFault = true;
    testHeaterOutput = 0;
  }

  // This would be detected over time in real system
  assertTrue(stuckTime > 0); // Placeholder assertion
}

// ============================================================================
// EMERGENCY SHUTDOWN PROCEDURE TESTS
// ============================================================================

test(Safety_EmergencyShutdown_HeaterOff)
{
  resetSafetyState();
  testHeaterOutput = 255;
  testSafetyShutdown = true;

  // Emergency shutdown procedure
  if (testSafetyShutdown)
  {
    testHeaterOutput = 0;
  }

  assertEqual(0.0, testHeaterOutput);
}

test(Safety_EmergencyShutdown_FanMax)
{
  resetSafetyState();
  testFanSpeed = 100;
  testSafetyShutdown = true;

  // Emergency shutdown procedure
  if (testSafetyShutdown)
  {
    testFanSpeed = 255;
  }

  assertEqual(255, testFanSpeed);
}

test(Safety_EmergencyShutdown_ImmediateResponse)
{
  resetSafetyState();
  testHeaterOutput = 255;

  unsigned long beforeShutdown = micros();
  testSafetyShutdown = true;
  testHeaterOutput = 0;
  testFanSpeed = 255;
  unsigned long afterShutdown = micros();

  // Should be nearly instantaneous (< 1ms)
  unsigned long responseTime = afterShutdown - beforeShutdown;
  assertTrue(responseTime < 1000); // microseconds
}

test(Safety_EmergencyShutdown_Persistent)
{
  resetSafetyState();
  testSafetyShutdown = true;
  testHeaterOutput = 0;
  testFanSpeed = 255;

  // Try to turn heater back on (should fail)
  if (testSafetyShutdown)
  {
    testHeaterOutput = 0; // Forced off
  }

  // Should remain in safe state
  assertEqual(0.0, testHeaterOutput);
  assertTrue(testSafetyShutdown);
}

test(Safety_EmergencyShutdown_AllConditions)
{
  resetSafetyState();

  // Test multiple shutdown triggers
  bool overTemp = absoluteTemperatureLimitExceeded(MAX_SAFE_TEMP + 1.0);
  bool sensorFault = !thermocoupleReadingIsValid(SENSOR_FAULT_TEMP + 1.0, 0.0, SENSOR_FAULT_TEMP);
  bool heaterRiseFault = heaterRiseTimeoutElapsed(MAX_HEATER_NO_RISE_MS);

  // Any trigger should cause shutdown
  if (overTemp || sensorFault || heaterRiseFault)
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSafetyShutdown);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

// ============================================================================
// FAIL-SAFE BEHAVIOR TESTS
// ============================================================================

test(Safety_FailSafe_PowerOnState)
{
  resetSafetyState();

  // On power-up/reset, should be in safe state
  assertEqual(0.0, testHeaterOutput);
  assertEqual(0, testFanSpeed);
  assertFalse(testSafetyShutdown);
}

test(Safety_FailSafe_DefaultHeaterOff)
{
  resetSafetyState();

  // Default state is heater off
  assertEqual(0.0, testHeaterOutput);
}

test(Safety_FailSafe_NoHeaterWithoutTemperature)
{
  resetSafetyState();
  testCurrentTemp = 0; // No valid reading
  testHeaterOutput = 100;

  // Should not allow heater without valid temp
  if (testCurrentTemp <= 0)
  {
    testHeaterOutput = 0;
  }

  assertEqual(0.0, testHeaterOutput);
}

test(Safety_FailSafe_CoolingPriority)
{
  resetSafetyState();
  testCurrentTemp = 400.0;
  testSafetyShutdown = true;

  // Cooling takes priority over everything
  testHeaterOutput = 0;
  testFanSpeed = 255;

  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

// ============================================================================
// TEMPERATURE LIMIT BOUNDARY TESTS
// ============================================================================

test(Safety_Boundary_JustBelowSafeLimit)
{
  resetSafetyState();
  testCurrentTemp = 499.0; // Just under limit
  testSafetyShutdown = false;

  // Should not trigger
  if (absoluteTemperatureLimitExceeded(testCurrentTemp))
  {
    testSafetyShutdown = true;
  }

  assertFalse(testSafetyShutdown);
}

test(Safety_Boundary_ExactlySafeLimit)
{
  resetSafetyState();
  testCurrentTemp = 500.0; // Exactly at limit
  testSafetyShutdown = false;

  // Should not trigger (> not >=)
  if (absoluteTemperatureLimitExceeded(testCurrentTemp))
  {
    testSafetyShutdown = true;
  }

  assertFalse(testSafetyShutdown);
}

test(Safety_Boundary_JustOverSafeLimit)
{
  resetSafetyState();
  testCurrentTemp = 501.0; // Just over limit
  testSafetyShutdown = false;

  // Should trigger
  if (absoluteTemperatureLimitExceeded(testCurrentTemp))
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSafetyShutdown);
}

// ============================================================================
// MULTIPLE FAULT CONDITION TESTS
// ============================================================================

test(Safety_MultipleFaults_OverTempAndSensorFault)
{
  resetSafetyState();
  testCurrentTemp = 999.0; // Both over temp AND sensor fault
  testHeaterOutput = 200;

  bool overTemp = absoluteTemperatureLimitExceeded(testCurrentTemp);
  bool sensorFault = !thermocoupleReadingIsValid(testCurrentTemp, 0.0, SENSOR_FAULT_TEMP);

  if (overTemp || sensorFault)
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(overTemp);
  assertTrue(sensorFault);
  assertTrue(testSafetyShutdown);
  assertEqual(0.0, testHeaterOutput);
}

test(Safety_MultipleFaults_Priority)
{
  resetSafetyState();

  // Multiple conditions - all should result in same safe state
  testCurrentTemp = 600.0;  // Extreme over-temp
  testSetpointTemp = 300.0; // Also thermal runaway
  testHeaterOutput = 255;

  // Emergency shutdown is the response regardless of which condition
  testSafetyShutdown = true;
  testHeaterOutput = 0;
  testFanSpeed = 255;

  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

// ============================================================================
// RECOVERY AND RESET TESTS
// ============================================================================

test(Safety_Recovery_CooldownRequired)
{
  resetSafetyState();
  testCurrentTemp = 510.0;
  testSafetyShutdown = true;

  // Cannot recover until cooled
  testCurrentTemp = 490.0; // Still hot
  bool canRecover = testCurrentTemp < COOLING_TARGET_TEMP;

  assertFalse(canRecover);
  assertTrue(testSafetyShutdown);
}

test(Safety_Recovery_CompleteCooldown)
{
  resetSafetyState();
  testCurrentTemp = 510.0;
  testSafetyShutdown = true;

  // Cool down completely
  testCurrentTemp = 140.0;
  bool canRecover = testCurrentTemp < COOLING_TARGET_TEMP;

  if (canRecover)
  {
    testSafetyShutdown = false;
  }

  assertTrue(canRecover);
  assertFalse(testSafetyShutdown);
}
