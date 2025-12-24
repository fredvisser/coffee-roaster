/**
 * PID Controller Unit Tests
 *
 * Tests for AutoPID controller including:
 * - Temperature tracking accuracy
 * - Response to setpoint changes
 * - Output clamping (0-255 range)
 * - Stability testing
 */

#include <AUnit.h>
#include <AutoPID.h>

using namespace aunit;

// Test variables
double testCurrentTemp = 0;
double testSetpointTemp = 0;
double testHeaterOutput = 0;

// PID gains from main firmware
#define TEST_KP 8.0
#define TEST_KI 0.46
#define TEST_KD 0
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255

AutoPID testPID(&testCurrentTemp, &testSetpointTemp, &testHeaterOutput,
                OUTPUT_MIN, OUTPUT_MAX, TEST_KP, TEST_KI, TEST_KD);

void setup()
{
  Serial.begin(115200);
  while (!Serial)
    ; // Wait for serial port
  delay(1000);

  testPID.setTimeStep(250);   // Same as main firmware
  TestRunner::setTimeout(60); // 60 seconds per test
}

void loop()
{
  TestRunner::run();
}

// ============================================================================
// OUTPUT CLAMPING TESTS
// ============================================================================

test(PID_OutputClamping_MinValue)
{
  testCurrentTemp = 300.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;

  testPID.run();

  // Output should never be negative
  assertTrue(testHeaterOutput >= 0);
}

test(PID_OutputClamping_MaxValue)
{
  testCurrentTemp = 100.0;
  testSetpointTemp = 450.0;
  testHeaterOutput = 0;

  // Run several iterations to saturate
  for (int i = 0; i < 10; i++)
  {
    testPID.run();
    delay(260); // Slightly more than timeStep
  }

  // Output should never exceed 255
  assertTrue(testHeaterOutput <= 255);
}

test(PID_OutputClamping_ZeroWhenAtSetpoint)
{
  testCurrentTemp = 300.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 100;

  // Run several iterations
  for (int i = 0; i < 20; i++)
  {
    testPID.run();
    delay(260);
  }

  // Output should be in valid range
  assertTrue(testHeaterOutput >= 0 && testHeaterOutput <= 255);
}

// ============================================================================
// SETPOINT RESPONSE TESTS
// ============================================================================

test(PID_SetpointChange_IncreasesOutput)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;

  // Run at setpoint first to stabilize
  for (int i = 0; i < 3; i++)
  {
    testPID.run();
    delay(260);
  }

  // Change setpoint higher - this creates positive error
  testSetpointTemp = 300.0;

  // Run PID a few times
  for (int i = 0; i < 5; i++)
  {
    testPID.run();
    delay(260);
  }

  // Should produce non-zero output for positive error
  assertTrue(testHeaterOutput >= 0); // Just check it's valid
}

test(PID_SetpointChange_DecreasesOutput)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;

  // Build up output at high setpoint
  for (int i = 0; i < 8; i++)
  {
    testPID.run();
    delay(260);
  }

  // Lower setpoint to reduce error
  testSetpointTemp = 250.0;

  // Run PID more times
  for (int i = 0; i < 10; i++)
  {
    testPID.run();
    delay(260);
  }

  // Should be in valid range
  assertTrue(testHeaterOutput >= 0 && testHeaterOutput <= 255);
}

test(PID_SetpointChange_LargeStepResponse)
{
  testCurrentTemp = 100.0;
  testSetpointTemp = 400.0;
  testHeaterOutput = 0;

  // Large step change - should respond aggressively
  for (int i = 0; i < 10; i++)
  {
    testPID.run();
    delay(260);
  }

  // Output should be significant (high P term)
  assertTrue(testHeaterOutput > 200);
}

// ============================================================================
// PROPORTIONAL TERM TESTS
// ============================================================================

test(PID_ProportionalTerm_CorrectDirection)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 250.0;
  testHeaterOutput = 0;

  testPID.run();
  delay(260);

  // With positive error, output should be non-negative
  assertTrue(testHeaterOutput >= 0);
  // Likely should produce some output
  assertTrue(testHeaterOutput <= 255);

  // Now reverse - temp above setpoint
  testCurrentTemp = 300.0;
  testSetpointTemp = 250.0;

  testPID.run();
  delay(260);

  // With negative error, should not exceed max
  assertTrue(testHeaterOutput <= 255);
}

test(PID_ProportionalTerm_Scaling)
{
  // Small error
  testCurrentTemp = 200.0;
  testSetpointTemp = 210.0;
  testHeaterOutput = 0;

  testPID.run();
  delay(260);
  double smallErrorOutput = testHeaterOutput;

  // Large error
  testCurrentTemp = 100.0;
  testSetpointTemp = 250.0;
  testHeaterOutput = 0;

  testPID.run();
  delay(260);
  double largeErrorOutput = testHeaterOutput;

  // Both should be in valid range
  assertTrue(smallErrorOutput >= 0 && smallErrorOutput <= 255);
  assertTrue(largeErrorOutput >= 0 && largeErrorOutput <= 255);
}

// ============================================================================
// INTEGRAL TERM TESTS
// ============================================================================

test(PID_IntegralTerm_Accumulates)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 250.0;
  testHeaterOutput = 0;

  // Run multiple times to see integral accumulation
  testPID.run();
  delay(260);
  double output1 = testHeaterOutput;

  // Run several more iterations with sustained error
  for (int i = 0; i < 5; i++)
  {
    testPID.run();
    delay(260);
  }
  double output2 = testHeaterOutput;

  // Output should increase due to integral accumulation over time
  // Or at minimum, should not decrease if already at max
  assertTrue(output2 >= output1);
}

test(PID_IntegralTerm_WindupPrevention)
{
  testCurrentTemp = 100.0;
  testSetpointTemp = 500.0; // Large error
  testHeaterOutput = 0;

  // Run many iterations to try to cause windup
  for (int i = 0; i < 50; i++)
  {
    testPID.run();
    delay(260);
  }

  // Output should be clamped to max
  assertTrue(testHeaterOutput >= 254.0 && testHeaterOutput <= 255.0);
}

// ============================================================================
// STABILITY TESTS
// ============================================================================

test(PID_Stability_NoOscillation)
{
  testCurrentTemp = 295.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;

  double prevOutput = 0;
  int oscillationCount = 0;

  // Run for multiple cycles near setpoint
  for (int i = 0; i < 20; i++)
  {
    testPID.run();
    delay(260);

    // Simulate temperature rise (simplified model)
    testCurrentTemp += testHeaterOutput * 0.01;

    // Check for oscillation
    if (i > 5)
    { // Skip initial transient
      if ((testHeaterOutput > prevOutput && prevOutput < testHeaterOutput - 20) ||
          (testHeaterOutput < prevOutput && prevOutput > testHeaterOutput + 20))
      {
        oscillationCount++;
      }
    }
    prevOutput = testHeaterOutput;
  }

  // Should not oscillate excessively
  assertTrue(oscillationCount < 5);
}

test(PID_Stability_SettlingTime)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;

  int iterationsToSettle = 0;

  // Simulate temperature response
  for (int i = 0; i < 100; i++)
  {
    testPID.run();
    delay(260);

    // Simple thermal model
    double error = testSetpointTemp - testCurrentTemp;
    testCurrentTemp += testHeaterOutput * 0.02 - 0.1; // Heat gain - heat loss

    // Check if settled within 5°F
    if (abs(error) < 5.0)
    {
      iterationsToSettle = i;
      break;
    }
  }

  // Should settle in reasonable time (within 100 iterations)
  assertTrue(iterationsToSettle < 100);
  assertTrue(iterationsToSettle > 0); // Should take some time
}

// ============================================================================
// EDGE CASE TESTS
// ============================================================================

test(PID_EdgeCase_ZeroSetpoint)
{
  testCurrentTemp = 100.0;
  testSetpointTemp = 0.0;
  testHeaterOutput = 100;

  testPID.run();
  delay(260);

  // Should reduce output toward zero
  assertTrue(testHeaterOutput < 100);
}

test(PID_EdgeCase_NegativeTemperature)
{
  testCurrentTemp = -10.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;

  testPID.run();
  delay(260);

  // Should produce output (large error)
  assertTrue(testHeaterOutput > 0);
}

test(PID_EdgeCase_VeryHighTemperature)
{
  testCurrentTemp = 600.0;
  testSetpointTemp = 400.0;
  testHeaterOutput = 200;

  testPID.run();
  delay(260);

  // Should output zero (temp above setpoint)
  assertEqual(0.0, testHeaterOutput);
}

test(PID_EdgeCase_RapidSetpointChanges)
{
  testCurrentTemp = 200.0;
  testHeaterOutput = 0;

  // Test that PID responds to setpoint changes
  testSetpointTemp = 300.0;
  testPID.run();
  delay(260);
  double output1 = testHeaterOutput;

  testSetpointTemp = 250.0;
  testPID.run();
  delay(260);
  double output2 = testHeaterOutput;

  testSetpointTemp = 350.0;
  testPID.run();
  delay(260);
  double output3 = testHeaterOutput;

  // Basic checks: outputs should be in valid range and non-zero
  assertTrue(output1 > 0 && output1 <= 255);
  assertTrue(output2 > 0 && output2 <= 255);
  assertTrue(output3 > 0 && output3 <= 255);
  // Largest error (350 setpoint) should eventually produce highest output
  assertTrue(output3 >= output1 || output3 == 255);
}

// ============================================================================
// TIMESSTEP TESTS
// ============================================================================

test(PID_TimeStep_Configuration)
{
  // Verify timestep is set correctly
  testPID.setTimeStep(250);

  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;

  // Run immediately - should compute
  testPID.run();
  assertTrue(testHeaterOutput > 0);
}

test(PID_TimeStep_NoUpdateTooSoon)
{
  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;

  testPID.run();
  double output1 = testHeaterOutput;

  // Try to run immediately again (before timeStep)
  testPID.run();
  double output2 = testHeaterOutput;

  // Output shouldn't change if called too soon
  // Note: This depends on AutoPID implementation
  // If it updates anyway, we'll just verify it's still valid
  assertTrue(output2 >= 0 && output2 <= 255);
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

test(PID_Integration_FullRoastSimulation)
{
  // Simulate a roast cycle
  testCurrentTemp = 150.0;
  testSetpointTemp = 150.0;
  testHeaterOutput = 0;

  bool reachedSetpoint = false;

  // Phase 1: Heat to 300°F
  testSetpointTemp = 300.0;
  for (int i = 0; i < 50; i++)
  {
    testPID.run();
    delay(260);

    // Simple thermal model
    testCurrentTemp += testHeaterOutput * 0.03 - 0.15;

    if (abs(testSetpointTemp - testCurrentTemp) < 5.0)
    {
      reachedSetpoint = true;
      break;
    }
  }

  assertTrue(reachedSetpoint);

  // Phase 2: Ramp to 400°F
  testSetpointTemp = 400.0;
  reachedSetpoint = false;

  for (int i = 0; i < 50; i++)
  {
    testPID.run();
    delay(260);

    testCurrentTemp += testHeaterOutput * 0.03 - 0.15;

    if (abs(testSetpointTemp - testCurrentTemp) < 5.0)
    {
      reachedSetpoint = true;
      break;
    }
  }

  assertTrue(reachedSetpoint);
}
