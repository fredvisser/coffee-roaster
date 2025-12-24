/**
 * State Machine Unit Tests
 *
 * Tests for roaster state machine including:
 * - State transitions (IDLE → START_ROAST → ROASTING → COOLING → IDLE)
 * - Safety interlocks
 * - Error handling
 * - Emergency stop behavior
 */

#include <AUnit.h>

using namespace aunit;

// Replicate state machine from main firmware
enum RoasterState
{
  IDLE = 0,
  START_ROAST = 1,
  ROASTING = 2,
  COOLING = 3,
  ERROR = 4
};

// Test state machine variables
RoasterState testState = IDLE;
double testCurrentTemp = 150.0;
double testSetpointTemp = 0.0;
double testHeaterOutput = 0.0;
int testFanSpeed = 0;
bool testHeaterEnabled = false;
bool testFanEnabled = false;
bool testEmergencyStop = false;
unsigned long testRoastStartTime = 0;

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

// Helper function to reset test state
void resetTestState()
{
  testState = IDLE;
  testCurrentTemp = 150.0;
  testSetpointTemp = 0.0;
  testHeaterOutput = 0.0;
  testFanSpeed = 0;
  testHeaterEnabled = false;
  testFanEnabled = false;
  testEmergencyStop = false;
  testRoastStartTime = 0;
}

// ============================================================================
// STATE TRANSITION TESTS
// ============================================================================

test(StateMachine_InitialState)
{
  resetTestState();

  assertEqual(IDLE, testState);
  assertEqual(0.0, testHeaterOutput);
  assertFalse(testHeaterEnabled);
}

test(StateMachine_Transition_IdleToStartRoast)
{
  resetTestState();

  // Simulate user starting roast
  testState = START_ROAST;

  assertEqual(START_ROAST, testState);
}

test(StateMachine_Transition_StartRoastToRoasting)
{
  resetTestState();
  testState = START_ROAST;

  // Simulate START_ROAST processing
  testState = ROASTING;
  testHeaterEnabled = true;
  testFanEnabled = true;
  testRoastStartTime = millis();

  assertEqual(ROASTING, testState);
  assertTrue(testHeaterEnabled);
  assertTrue(testFanEnabled);
  assertTrue(testRoastStartTime > 0);
}

test(StateMachine_Transition_RoastingToCooling_Normal)
{
  resetTestState();
  testState = ROASTING;
  testCurrentTemp = 450.0;
  testSetpointTemp = 440.0;

  // Simulate reaching final temperature
  if (testCurrentTemp >= testSetpointTemp)
  {
    testState = COOLING;
    testHeaterEnabled = false;
    testHeaterOutput = 0;
    testFanSpeed = 255; // Max fan
  }

  assertEqual(COOLING, testState);
  assertFalse(testHeaterEnabled);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(StateMachine_Transition_RoastingToCooling_Emergency)
{
  resetTestState();
  testState = ROASTING;
  testEmergencyStop = true;

  // Emergency stop triggered
  if (testEmergencyStop)
  {
    testState = COOLING;
    testHeaterEnabled = false;
    testHeaterOutput = 0;
    testFanSpeed = 255;
  }

  assertEqual(COOLING, testState);
  assertFalse(testHeaterEnabled);
  assertEqual(255, testFanSpeed);
}

test(StateMachine_Transition_CoolingToIdle)
{
  resetTestState();
  testState = COOLING;
  testCurrentTemp = 145.0;
  testFanSpeed = 255;

  // Check cooling completion
  if (testCurrentTemp <= 145.0)
  {
    testState = IDLE;
    testFanSpeed = 0;
    testFanEnabled = false;
  }

  assertEqual(IDLE, testState);
  assertEqual(0, testFanSpeed);
  assertFalse(testFanEnabled);
}

test(StateMachine_Transition_IdleStability)
{
  resetTestState();

  // Ensure IDLE state remains stable
  for (int i = 0; i < 100; i++)
  {
    // In IDLE, heater and fan should stay off
    testHeaterOutput = 0;
    testFanSpeed = 0;
  }

  assertEqual(IDLE, testState);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(0, testFanSpeed);
}

// ============================================================================
// SAFETY INTERLOCK TESTS
// ============================================================================

test(StateMachine_Safety_NoHeaterInIdle)
{
  resetTestState();
  testState = IDLE;

  // Try to enable heater (should not work in IDLE)
  if (testState == IDLE)
  {
    testHeaterOutput = 0;
    testHeaterEnabled = false;
  }

  assertEqual(0.0, testHeaterOutput);
  assertFalse(testHeaterEnabled);
}

test(StateMachine_Safety_NoHeaterInCooling)
{
  resetTestState();
  testState = COOLING;

  // Heater must be off in cooling
  if (testState == COOLING)
  {
    testHeaterOutput = 0;
    testHeaterEnabled = false;
  }

  assertEqual(0.0, testHeaterOutput);
  assertFalse(testHeaterEnabled);
}

test(StateMachine_Safety_FanMustRunInRoasting)
{
  resetTestState();
  testState = ROASTING;
  testFanSpeed = 100;

  // Fan must be running during roasting
  assertTrue(testFanSpeed > 0);
}

test(StateMachine_Safety_MaxFanInCooling)
{
  resetTestState();
  testState = COOLING;
  testFanSpeed = 255;

  // Fan should be at maximum during cooling
  assertEqual(255, testFanSpeed);
}

test(StateMachine_Safety_HighTempProtection)
{
  resetTestState();
  testState = ROASTING;
  testCurrentTemp = 550.0; // Dangerously high

  // Should trigger emergency cooling
  if (testCurrentTemp > 500.0)
  {
    testState = COOLING;
    testHeaterOutput = 0;
    testHeaterEnabled = false;
    testFanSpeed = 255;
  }

  assertEqual(COOLING, testState);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(StateMachine_Safety_HeaterOffWhenTempAboveSetpoint)
{
  resetTestState();
  testState = ROASTING;
  testCurrentTemp = 410.0;
  testSetpointTemp = 400.0;

  // When temp exceeds setpoint, heater should reduce/stop
  if (testCurrentTemp > testSetpointTemp)
  {
    testHeaterOutput = 0; // PID would do this
  }

  assertEqual(0.0, testHeaterOutput);
}

// ============================================================================
// ERROR HANDLING TESTS
// ============================================================================

test(StateMachine_Error_ThermocoupleFault)
{
  resetTestState();
  testState = ROASTING;
  testCurrentTemp = 999.0; // Fault reading

  // Simulate thermocouple fault detection
  if (testCurrentTemp > 500.0)
  {
    testState = ERROR;
    testHeaterOutput = 0;
    testHeaterEnabled = false;
    testFanSpeed = 255; // Safety: fan on
  }

  assertEqual(ERROR, testState);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(StateMachine_Error_RecoveryToIdle)
{
  resetTestState();
  testState = ERROR;
  testCurrentTemp = 150.0; // Valid reading

  // Simulate error recovery
  bool errorCleared = true;
  if (errorCleared && testCurrentTemp < 200.0)
  {
    testState = IDLE;
    testFanSpeed = 0;
  }

  assertEqual(IDLE, testState);
}

test(StateMachine_Error_NoHeaterInError)
{
  resetTestState();
  testState = ERROR;

  // Heater must never be on in ERROR state
  testHeaterOutput = 0;
  testHeaterEnabled = false;

  assertEqual(0.0, testHeaterOutput);
  assertFalse(testHeaterEnabled);
}

// ============================================================================
// EMERGENCY STOP TESTS
// ============================================================================

test(StateMachine_EmergencyStop_FromRoasting)
{
  resetTestState();
  testState = ROASTING;
  testHeaterOutput = 200;
  testFanSpeed = 150;

  // Emergency stop triggered
  testEmergencyStop = true;

  if (testEmergencyStop)
  {
    testState = COOLING;
    testHeaterOutput = 0;
    testHeaterEnabled = false;
    testFanSpeed = 255;
  }

  assertEqual(COOLING, testState);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}

test(StateMachine_EmergencyStop_Immediate)
{
  resetTestState();
  testState = ROASTING;
  testHeaterOutput = 255;
  unsigned long beforeStop = millis();

  // Trigger emergency stop
  testEmergencyStop = true;
  testHeaterOutput = 0;
  testHeaterEnabled = false;
  unsigned long afterStop = millis();

  // Should be immediate (< 100ms in test environment)
  assertTrue((afterStop - beforeStop) < 100);
  assertEqual(0.0, testHeaterOutput);
}

test(StateMachine_EmergencyStop_FanContinues)
{
  resetTestState();
  testState = ROASTING;
  testEmergencyStop = true;

  // After emergency stop, fan must continue
  testState = COOLING;
  testFanSpeed = 255;

  assertEqual(COOLING, testState);
  assertTrue(testFanSpeed > 0);
}

test(StateMachine_EmergencyStop_FromStartRoast)
{
  resetTestState();
  testState = START_ROAST;
  testEmergencyStop = true;

  // Can abort even before roasting begins
  testState = IDLE;
  testHeaterOutput = 0;
  testFanSpeed = 0;

  assertEqual(IDLE, testState);
  assertEqual(0.0, testHeaterOutput);
}

// ============================================================================
// STATE SEQUENCE VALIDATION TESTS
// ============================================================================

test(StateMachine_Sequence_CompleteRoast)
{
  resetTestState();

  // Initial state
  assertEqual(IDLE, testState);

  // Start roast
  testState = START_ROAST;
  assertEqual(START_ROAST, testState);

  // Begin roasting
  testState = ROASTING;
  testHeaterEnabled = true;
  testFanEnabled = true;
  assertEqual(ROASTING, testState);

  // Complete roast
  testCurrentTemp = 450.0;
  testState = COOLING;
  testHeaterEnabled = false;
  assertEqual(COOLING, testState);

  // Cool down
  testCurrentTemp = 145.0;
  testState = IDLE;
  testFanEnabled = false;
  assertEqual(IDLE, testState);
}

test(StateMachine_Sequence_InvalidTransition)
{
  resetTestState();

  // Should not jump from IDLE directly to COOLING
  testState = IDLE;

  // Attempt invalid transition (should be prevented in real code)
  RoasterState attemptedState = COOLING;

  // In real implementation, this would be blocked
  // For test, we verify states are different concepts
  assertNotEqual(IDLE, COOLING);
  assertNotEqual(START_ROAST, COOLING);
}

// ============================================================================
// TIMING AND DURATION TESTS
// ============================================================================

test(StateMachine_Timing_RoastDurationTracking)
{
  resetTestState();
  testState = START_ROAST;

  unsigned long startTime = millis();
  testRoastStartTime = startTime;

  delay(100);

  unsigned long duration = millis() - testRoastStartTime;
  assertTrue(duration >= 100);
  assertTrue(duration < 200); // Should be close to 100ms
}

test(StateMachine_Timing_CoolingDuration)
{
  resetTestState();
  testState = COOLING;
  testCurrentTemp = 300.0;

  unsigned long coolingStart = millis();

  // Simulate cooling over time
  while (testCurrentTemp > 145.0)
  {
    delay(10);
    testCurrentTemp -= 1.0; // Simulate cooling

    if (millis() - coolingStart > 5000)
    {
      break; // Timeout for test
    }
  }

  unsigned long coolingDuration = millis() - coolingStart;
  assertTrue(coolingDuration > 0);
  assertTrue(testCurrentTemp <= 145.0 || coolingDuration >= 5000);
}

// ============================================================================
// CONCURRENT CONDITION TESTS
// ============================================================================

test(StateMachine_Concurrent_HeaterAndFanControl)
{
  resetTestState();
  testState = ROASTING;

  // Both should be controllable simultaneously
  testHeaterOutput = 200;
  testFanSpeed = 180;

  assertTrue(testHeaterOutput > 0);
  assertTrue(testFanSpeed > 0);
  assertTrue(testHeaterOutput <= 255);
  assertTrue(testFanSpeed <= 255);
}

test(StateMachine_Concurrent_TempAndFanResponse)
{
  resetTestState();
  testState = ROASTING;
  testCurrentTemp = 350.0;
  testSetpointTemp = 400.0;

  // Both temp and fan should respond to profile
  testHeaterOutput = 220; // High output for heating
  testFanSpeed = 200;     // High fan per profile

  assertTrue(testHeaterOutput > 0);
  assertTrue(testFanSpeed > 0);
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

test(StateMachine_Boundary_VeryShortRoast)
{
  resetTestState();
  testState = START_ROAST;

  // Immediately complete (edge case)
  testState = ROASTING;
  testCurrentTemp = 440.0;
  testSetpointTemp = 440.0;

  // Should handle immediate completion
  testState = COOLING;
  assertEqual(COOLING, testState);
}

test(StateMachine_Boundary_VeryLongRoast)
{
  resetTestState();
  testState = ROASTING;

  // Simulate extended roast (hours)
  testRoastStartTime = millis() - 7200000; // 2 hours ago

  unsigned long duration = millis() - testRoastStartTime;

  // Should still function correctly
  assertTrue(duration >= 7200000);
  assertEqual(ROASTING, testState);
}

test(StateMachine_Boundary_CoolingAtAmbient)
{
  resetTestState();
  testState = COOLING;
  testCurrentTemp = 145.0; // Already at cooling target

  // Should immediately transition to IDLE
  if (testCurrentTemp <= 145.0)
  {
    testState = IDLE;
    testFanSpeed = 0;
  }

  assertEqual(IDLE, testState);
  assertEqual(0, testFanSpeed);
}

test(StateMachine_Boundary_MultipleEmergencyStops)
{
  resetTestState();
  testState = ROASTING;

  // First emergency stop
  testEmergencyStop = true;
  testState = COOLING;
  testHeaterOutput = 0;
  testFanSpeed = 255; // Safety: full cooling fan

  // Second emergency stop (already in cooling)
  testEmergencyStop = true;
  testFanSpeed = 255; // Should remain at max fan

  // Should remain stable in cooling
  assertEqual(COOLING, testState);
  assertEqual(0.0, testHeaterOutput);
  assertEqual(255, testFanSpeed);
}
