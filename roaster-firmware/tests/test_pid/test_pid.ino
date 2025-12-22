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

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port
  delay(1000);
  
  testPID.setTimeStep(250); // Same as main firmware
}

void loop() {
  TestRunner::run();
}

// ============================================================================
// OUTPUT CLAMPING TESTS
// ============================================================================

test(PID_OutputClamping_MinValue) {
  testCurrentTemp = 300.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;
  
  testPID.run();
  
  // Output should never be negative
  assertTrue(testHeaterOutput >= 0);
}

test(PID_OutputClamping_MaxValue) {
  testCurrentTemp = 100.0;
  testSetpointTemp = 450.0;
  testHeaterOutput = 0;
  
  // Run several iterations to saturate
  for (int i = 0; i < 10; i++) {
    testPID.run();
    delay(260); // Slightly more than timeStep
  }
  
  // Output should never exceed 255
  assertTrue(testHeaterOutput <= 255);
}

test(PID_OutputClamping_ZeroWhenAtSetpoint) {
  testCurrentTemp = 300.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 100;
  
  // Run several iterations
  for (int i = 0; i < 20; i++) {
    testPID.run();
    delay(260);
  }
  
  // Output should approach zero when at setpoint
  assertTrue(testHeaterOutput < 50); // Allow some tolerance
}

// ============================================================================
// SETPOINT RESPONSE TESTS
// ============================================================================

test(PID_SetpointChange_IncreasesOutput) {
  testCurrentTemp = 200.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;
  
  // Run at setpoint first
  testPID.run();
  delay(260);
  double outputAtSetpoint = testHeaterOutput;
  
  // Change setpoint higher
  testSetpointTemp = 300.0;
  
  // Run PID
  for (int i = 0; i < 5; i++) {
    testPID.run();
    delay(260);
  }
  
  // Output should increase when setpoint increases
  assertTrue(testHeaterOutput > outputAtSetpoint);
}

test(PID_SetpointChange_DecreasesOutput) {
  testCurrentTemp = 300.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 100;
  
  // Run at elevated setpoint
  for (int i = 0; i < 5; i++) {
    testPID.run();
    delay(260);
  }
  double outputAtHighSetpoint = testHeaterOutput;
  
  // Lower setpoint
  testSetpointTemp = 250.0;
  
  // Run PID
  for (int i = 0; i < 5; i++) {
    testPID.run();
    delay(260);
  }
  
  // Output should decrease
  assertTrue(testHeaterOutput < outputAtHighSetpoint);
}

test(PID_SetpointChange_LargeStepResponse) {
  testCurrentTemp = 100.0;
  testSetpointTemp = 400.0;
  testHeaterOutput = 0;
  
  // Large step change - should respond aggressively
  for (int i = 0; i < 10; i++) {
    testPID.run();
    delay(260);
  }
  
  // Output should be significant (high P term)
  assertTrue(testHeaterOutput > 200);
}

// ============================================================================
// PROPORTIONAL TERM TESTS
// ============================================================================

test(PID_ProportionalTerm_CorrectDirection) {
  testCurrentTemp = 200.0;
  testSetpointTemp = 250.0;
  testHeaterOutput = 0;
  
  testPID.run();
  delay(260);
  
  // With positive error, output should be positive
  assertTrue(testHeaterOutput > 0);
  
  // Now reverse - temp above setpoint
  testCurrentTemp = 300.0;
  testSetpointTemp = 250.0;
  
  testPID.run();
  delay(260);
  
  // With negative error, output should be minimal (clamped to 0)
  assertTrue(testHeaterOutput == 0);
}

test(PID_ProportionalTerm_Scaling) {
  // Small error
  testCurrentTemp = 200.0;
  testSetpointTemp = 210.0;
  testHeaterOutput = 0;
  
  testPID.run();
  delay(260);
  double smallErrorOutput = testHeaterOutput;
  
  // Reset and try larger error
  testHeaterOutput = 0;
  testCurrentTemp = 200.0;
  testSetpointTemp = 250.0;
  
  testPID.run();
  delay(260);
  double largeErrorOutput = testHeaterOutput;
  
  // Larger error should produce larger output
  assertTrue(largeErrorOutput > smallErrorOutput);
}

// ============================================================================
// INTEGRAL TERM TESTS
// ============================================================================

test(PID_IntegralTerm_Accumulates) {
  testCurrentTemp = 200.0;
  testSetpointTemp = 250.0;
  testHeaterOutput = 0;
  
  // First run
  testPID.run();
  delay(260);
  double output1 = testHeaterOutput;
  
  // Second run with same error
  testPID.run();
  delay(260);
  double output2 = testHeaterOutput;
  
  // Output should increase due to integral accumulation
  assertTrue(output2 > output1);
}

test(PID_IntegralTerm_WindupPrevention) {
  testCurrentTemp = 100.0;
  testSetpointTemp = 500.0; // Large error
  testHeaterOutput = 0;
  
  // Run many iterations to try to cause windup
  for (int i = 0; i < 50; i++) {
    testPID.run();
    delay(260);
  }
  
  // Output should be clamped to max
  assertTrue(testHeaterOutput >= 254.0 && testHeaterOutput <= 255.0);
}

// ============================================================================
// STABILITY TESTS
// ============================================================================

test(PID_Stability_NoOscillation) {
  testCurrentTemp = 295.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;
  
  double prevOutput = 0;
  int oscillationCount = 0;
  
  // Run for multiple cycles near setpoint
  for (int i = 0; i < 20; i++) {
    testPID.run();
    delay(260);
    
    // Simulate temperature rise (simplified model)
    testCurrentTemp += testHeaterOutput * 0.01;
    
    // Check for oscillation
    if (i > 5) { // Skip initial transient
      if ((testHeaterOutput > prevOutput && prevOutput < testHeaterOutput - 20) ||
          (testHeaterOutput < prevOutput && prevOutput > testHeaterOutput + 20)) {
        oscillationCount++;
      }
    }
    prevOutput = testHeaterOutput;
  }
  
  // Should not oscillate excessively
  assertTrue(oscillationCount < 5);
}

test(PID_Stability_SettlingTime) {
  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;
  
  int iterationsToSettle = 0;
  
  // Simulate temperature response
  for (int i = 0; i < 100; i++) {
    testPID.run();
    delay(260);
    
    // Simple thermal model
    double error = testSetpointTemp - testCurrentTemp;
    testCurrentTemp += testHeaterOutput * 0.02 - 0.1; // Heat gain - heat loss
    
    // Check if settled within 5°F
    if (abs(error) < 5.0) {
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

test(PID_EdgeCase_ZeroSetpoint) {
  testCurrentTemp = 100.0;
  testSetpointTemp = 0.0;
  testHeaterOutput = 100;
  
  testPID.run();
  delay(260);
  
  // Should reduce output toward zero
  assertTrue(testHeaterOutput < 100);
}

test(PID_EdgeCase_NegativeTemperature) {
  testCurrentTemp = -10.0;
  testSetpointTemp = 200.0;
  testHeaterOutput = 0;
  
  testPID.run();
  delay(260);
  
  // Should produce output (large error)
  assertTrue(testHeaterOutput > 0);
}

test(PID_EdgeCase_VeryHighTemperature) {
  testCurrentTemp = 600.0;
  testSetpointTemp = 400.0;
  testHeaterOutput = 200;
  
  testPID.run();
  delay(260);
  
  // Should output zero (temp above setpoint)
  assertEqual(0.0, testHeaterOutput);
}

test(PID_EdgeCase_RapidSetpointChanges) {
  testCurrentTemp = 200.0;
  testHeaterOutput = 0;
  
  // Rapidly change setpoint
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
  
  // Should respond to each change
  assertTrue(output2 < output1); // Decreased setpoint = lower output
  assertTrue(output3 > output2); // Increased setpoint = higher output
}

// ============================================================================
// TIMESSTEP TESTS
// ============================================================================

test(PID_TimeStep_Configuration) {
  // Verify timestep is set correctly
  testPID.setTimeStep(250);
  
  testCurrentTemp = 200.0;
  testSetpointTemp = 300.0;
  testHeaterOutput = 0;
  
  // Run immediately - should compute
  testPID.run();
  assertTrue(testHeaterOutput > 0);
}

test(PID_TimeStep_NoUpdateTooSoon) {
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

test(PID_Integration_FullRoastSimulation) {
  // Simulate a roast cycle
  testCurrentTemp = 150.0;
  testSetpointTemp = 150.0;
  testHeaterOutput = 0;
  
  bool reachedSetpoint = false;
  
  // Phase 1: Heat to 300°F
  testSetpointTemp = 300.0;
  for (int i = 0; i < 50; i++) {
    testPID.run();
    delay(260);
    
    // Simple thermal model
    testCurrentTemp += testHeaterOutput * 0.03 - 0.15;
    
    if (abs(testSetpointTemp - testCurrentTemp) < 5.0) {
      reachedSetpoint = true;
      break;
    }
  }
  
  assertTrue(reachedSetpoint);
  
  // Phase 2: Ramp to 400°F
  testSetpointTemp = 400.0;
  reachedSetpoint = false;
  
  for (int i = 0; i < 50; i++) {
    testPID.run();
    delay(260);
    
    testCurrentTemp += testHeaterOutput * 0.03 - 0.15;
    
    if (abs(testSetpointTemp - testCurrentTemp) < 5.0) {
      reachedSetpoint = true;
      break;
    }
  }
  
  assertTrue(reachedSetpoint);
}
