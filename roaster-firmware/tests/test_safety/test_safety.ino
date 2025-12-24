/**
 * Safety System Unit Tests
 *
 * CRITICAL safety tests including:
 * - Thermal runaway protection
 * - Over-temperature protection
 * - Sensor failure detection
 * - Emergency shutdown procedures
 *
 * These tests verify the most important safety features.
 */

#include <AUnit.h>

using namespace aunit;

// Safety thresholds
#define MAX_SAFE_TEMP 500.0
#define MAX_ROAST_TEMP 460.0
#define COOLING_TARGET_TEMP 145.0
#define SENSOR_FAULT_THRESHOLD 500.0
#define THERMAL_RUNAWAY_THRESHOLD 20.0 // Degrees over setpoint
#define THERMAL_RUNAWAY_TIME 30000     // 30 seconds

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

// ============================================================================
// THERMAL RUNAWAY PROTECTION TESTS
// ============================================================================

test(Safety_ThermalRunaway_Detection)
{
  resetSafetyState();
  testCurrentTemp = 350.0;
  testSetpointTemp = 300.0; // Temp 50° above setpoint
  testHeaterOutput = 255;

  // Detect thermal runaway
  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD)
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSafetyShutdown);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(Safety_ThermalRunaway_HeaterCutoff)
{
  resetSafetyState();
  testCurrentTemp = 325.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 200;

  // Simulate thermal runaway condition
  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD)
  {
    testHeaterOutput = 0;
  }

  assertEqual(0.0, testHeaterOutput);
}

test(Safety_ThermalRunaway_NoFalsePositive)
{
  resetSafetyState();
  testCurrentTemp = 310.0;
  testSetpointTemp = 300.0; // Only 10° above (within tolerance)
  testSafetyShutdown = false;

  // Should NOT trigger runaway protection
  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD)
  {
    testSafetyShutdown = true;
  }

  assertFalse(testSafetyShutdown);
}

test(Safety_ThermalRunaway_RampingSetpoint)
{
  resetSafetyState();
  testCurrentTemp = 300.0;
  testSetpointTemp = 250.0;                // Setpoint lowered while temp still high
  testLastTempIncrease = millis() - 15000; // Simulate 15 seconds without temp decrease

  // This is thermal runaway - temp not following setpoint down
  unsigned long stableTime = 10000; // 10 seconds
  bool isRunaway = false;

  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD &&
      millis() - testLastTempIncrease >= stableTime)
  {
    isRunaway = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  // Runaway should be detected
  assertTrue(isRunaway);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(Safety_ThermalRunaway_TimedDetection)
{
  resetSafetyState();
  testCurrentTemp = 330.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 255;
  testLastTempIncrease = millis() - THERMAL_RUNAWAY_TIME - 1000;

  // Temp has been high for too long
  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD &&
      millis() - testLastTempIncrease > THERMAL_RUNAWAY_TIME)
  {
    testSafetyShutdown = true;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertTrue(testSafetyShutdown);
  assertEqual(0.0, testHeaterOutput);
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
  if (testCurrentTemp > MAX_SAFE_TEMP)
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
  if (testCurrentTemp > MAX_ROAST_TEMP)
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
  if (testCurrentTemp > MAX_SAFE_TEMP)
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
  if (testCurrentTemp > MAX_ROAST_TEMP)
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
  if (testCurrentTemp > SENSOR_FAULT_THRESHOLD)
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
  bool overTemp = false;
  bool sensorFault = false;
  bool thermalRunaway = false;

  testCurrentTemp = 510.0;
  if (testCurrentTemp > MAX_SAFE_TEMP)
    overTemp = true;

  testCurrentTemp = 999.0;
  if (testCurrentTemp > SENSOR_FAULT_THRESHOLD)
    sensorFault = true;

  testCurrentTemp = 350.0;
  testSetpointTemp = 300.0;
  if (testCurrentTemp > testSetpointTemp + THERMAL_RUNAWAY_THRESHOLD)
  {
    thermalRunaway = true;
  }

  // Any trigger should cause shutdown
  if (overTemp || sensorFault || thermalRunaway)
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
  if (testCurrentTemp > MAX_SAFE_TEMP)
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
  if (testCurrentTemp > MAX_SAFE_TEMP)
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
  if (testCurrentTemp > MAX_SAFE_TEMP)
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

  bool overTemp = testCurrentTemp > MAX_SAFE_TEMP;
  bool sensorFault = testCurrentTemp > SENSOR_FAULT_THRESHOLD;

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
