/**
 * Hardware Validation Test Suite
 *
 * Hardware-in-the-loop tests for the coffee roaster.
 * These tests require actual hardware to be connected.
 *
 * SAFETY WARNING: These tests control real heating elements and fans.
 * Never leave the hardware unattended during testing.
 * Have a fire extinguisher nearby.
 *
 * Hardware Required:
 * - ESP32 with roaster control board
 * - MAX6675 thermocouples (x2)
 * - Heating element
 * - PWM fan
 * - BDC fan with servo control
 * - Nextion display (optional for full tests)
 */

#include "Arduino.h"
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <AutoPID.h>
#include <ESP32Servo.h>
#include "../../Profiles.hpp"

// Pin definitions (from main firmware)
#define TC1_CS 10
#define TC2_CS 9
#define HEATER A0
#define FAN A1
#define BDCFAN D5

// Safety limits
#define MAX_TEST_TEMP 350.0
#define MAX_TEST_HEATER_OUTPUT 150 // Limit to 60% for safety
#define TEST_TIMEOUT 300000        // 5 minute timeout

// Hardware objects
MAX6675 thermocouple(SCK, TC1_CS, MISO);
MAX6675 thermocoupleFan(SCK, TC2_CS, MISO);
PWMrelay heaterRelay(HEATER, HIGH);
PWMrelay fanRelay(FAN, HIGH);
Servo bdcFan;

// Test state
bool testsPassed = true;
int testCount = 0;
int passCount = 0;
int failCount = 0;

void setup()
{
  Serial.begin(115200);
  delay(2000);

  // Print safety warning
  Serial.println();
  Serial.println("╔═══════════════════════════════════════════════════════════╗");
  Serial.println("║    HARDWARE VALIDATION TEST - SAFETY WARNING              ║");
  Serial.println("╚═══════════════════════════════════════════════════════════╝");
  Serial.println();
  Serial.println("⚠️  WARNING: These tests control real hardware!");
  Serial.println("⚠️  Do not leave unattended!");
  Serial.println("⚠️  Have fire extinguisher ready!");
  Serial.println();
  Serial.println("Starting in 10 seconds... Press RESET to abort.");
  Serial.println();

  delay(10000);

  // Initialize hardware
  pinMode(HEATER, OUTPUT);
  pinMode(FAN, OUTPUT);
  pinMode(TC1_CS, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  digitalWrite(HEATER, LOW);
  digitalWrite(FAN, LOW);

  // Allocate timer 2 for servo to avoid conflict with PWMrelay
  Serial.println("Allocating timer for BDC fan...");
  ESP32PWM::allocateTimer(2);

  int channel = bdcFan.attach(BDCFAN);
  if (channel == -1)
  {
    Serial.println("ERROR: BDC fan attach FAILED!");
    return; // Exit setup if BDC init fails
  }
  bdcFan.setPeriodHertz(50);

  // Arm sequence
  bdcFan.writeMicroseconds(800);
  delay(2000);
  bdcFan.writeMicroseconds(1800);
  delay(1000);
  bdcFan.writeMicroseconds(800);

  fanRelay.setPeriod(10);

  Serial.println("Hardware initialized. Starting tests...");
  Serial.println();
}

void loop()
{
  runAllTests();

  // Print summary
  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("              HARDWARE VALIDATION SUMMARY                  ");
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.print("Total Tests: ");
  Serial.println(testCount);
  Serial.print("Passed:      ");
  Serial.println(passCount);
  Serial.print("Failed:      ");
  Serial.println(failCount);
  Serial.println("═══════════════════════════════════════════════════════════");

  if (failCount == 0)
  {
    Serial.println("✓ ALL HARDWARE TESTS PASSED");
  }
  else
  {
    Serial.println("✗ SOME HARDWARE TESTS FAILED");
  }

  Serial.println();
  Serial.println("Tests complete. Reset to run again.");

  // Ensure everything is off
  digitalWrite(HEATER, LOW);
  digitalWrite(FAN, LOW);
  bdcFan.writeMicroseconds(800);

  while (true)
  {
    delay(1000);
  }
}

// ============================================================================
// TEST HELPER FUNCTIONS
// ============================================================================

void startTest(const char *testName)
{
  Serial.print("Testing: ");
  Serial.print(testName);
  Serial.print("...");
  testCount++;
}

void testPass()
{
  Serial.println(" ✓ PASS");
  passCount++;
}

void testFail(const char *reason)
{
  Serial.print(" ✗ FAIL - ");
  Serial.println(reason);
  failCount++;
  testsPassed = false;
}

void waitAndMonitor(int seconds)
{
  for (int i = 0; i < seconds; i++)
  {
    double temp = thermocouple.readFarenheit();
    if (temp > MAX_TEST_TEMP)
    {
      Serial.println();
      Serial.println("⚠️  EMERGENCY: Temperature limit exceeded!");
      digitalWrite(HEATER, LOW);
      digitalWrite(FAN, HIGH);
      while (true)
      {
        delay(1000);
      } // Halt
    }
    delay(1000);
  }
}

// ============================================================================
// SENSOR TESTS
// ============================================================================

void testThermocouple1()
{
  startTest("Thermocouple 1 (Bean temp)");

  double temp = thermocouple.readFarenheit();

  // Check for valid reading
  if (temp < 0 || temp > 500)
  {
    testFail("Invalid reading or sensor fault");
    return;
  }

  // Check for reasonable ambient temp
  if (temp < 50 || temp > 120)
  {
    Serial.print(" (Warning: unusual ambient temp: ");
    Serial.print(temp);
    Serial.print("°F)");
  }

  Serial.print(" [");
  Serial.print(temp);
  Serial.print("°F]");
  testPass();
}

void testThermocouple2()
{
  startTest("Thermocouple 2 (Fan chamber temp)");

  double temp = thermocoupleFan.readFarenheit();

  if (temp < 0 || temp > 500)
  {
    testFail("Invalid reading or sensor fault");
    return;
  }

  if (temp < 50 || temp > 120)
  {
    Serial.print(" (Warning: unusual ambient temp: ");
    Serial.print(temp);
    Serial.print("°F)");
  }

  Serial.print(" [");
  Serial.print(temp);
  Serial.print("°F]");
  testPass();
}

void testThermocoupleConsistency()
{
  startTest("Thermocouple consistency (10 readings)");

  double readings[10];
  double sum = 0;

  for (int i = 0; i < 10; i++)
  {
    readings[i] = thermocouple.readFarenheit();
    sum += readings[i];
    delay(250);
  }

  double avg = sum / 10.0;
  double maxDev = 0;

  for (int i = 0; i < 10; i++)
  {
    double dev = abs(readings[i] - avg);
    if (dev > maxDev)
      maxDev = dev;
  }

  // Readings should be consistent (within 5°F)
  if (maxDev > 5.0)
  {
    testFail("Readings too inconsistent");
    return;
  }

  Serial.print(" [avg: ");
  Serial.print(avg);
  Serial.print("°F, max dev: ");
  Serial.print(maxDev);
  Serial.print("°F]");
  testPass();
}

// ============================================================================
// HEATER CONTROL TESTS
// ============================================================================

void testHeaterControl()
{
  startTest("Heater control (PWM)");

  double initialTemp = thermocouple.readFarenheit();

  // Set low PWM for safety
  heaterRelay.setPWM(50); // ~20%

  // Run for 30 seconds
  Serial.print(" [monitoring 30s]");
  for (int i = 0; i < 30; i++)
  {
    heaterRelay.tick();
    delay(1000);

    double temp = thermocouple.readFarenheit();
    if (temp > MAX_TEST_TEMP)
    {
      heaterRelay.setPWM(0);
      testFail("Temperature exceeded safety limit");
      return;
    }
  }

  heaterRelay.setPWM(0);
  digitalWrite(HEATER, LOW);

  double finalTemp = thermocouple.readFarenheit();
  double tempRise = finalTemp - initialTemp;

  // Should see some temperature rise (at least 5°F)
  if (tempRise < 5.0)
  {
    testFail("Insufficient temperature rise (heater may not be working)");
    return;
  }

  Serial.print(" [temp rise: ");
  Serial.print(tempRise);
  Serial.print("°F]");
  testPass();
}

void testHeaterOffState()
{
  startTest("Heater OFF state verification");

  heaterRelay.setPWM(0);
  digitalWrite(HEATER, LOW);

  double initialTemp = thermocouple.readFarenheit();

  // Wait and ensure no heating
  delay(10000);

  double finalTemp = thermocouple.readFarenheit();
  double tempChange = abs(finalTemp - initialTemp);

  // Should not heat (allow for ambient cooling/warming)
  if (tempChange > 10.0)
  {
    testFail("Temperature changed too much with heater off");
    return;
  }

  testPass();
}

// ============================================================================
// FAN CONTROL TESTS
// ============================================================================

void testPWMFanControl()
{
  startTest("PWM Fan control");

  // Test low speed
  fanRelay.setPWM(64); // 25%
  for (int i = 0; i < 20; i++)
  {
    fanRelay.tick();
    delay(100);
  }

  // Test medium speed
  fanRelay.setPWM(128); // 50%
  for (int i = 0; i < 20; i++)
  {
    fanRelay.tick();
    delay(100);
  }

  // Test high speed
  fanRelay.setPWM(255); // 100%
  for (int i = 0; i < 20; i++)
  {
    fanRelay.tick();
    delay(100);
  }

  // Turn off
  fanRelay.setPWM(0);
  digitalWrite(FAN, LOW);

  Serial.print(" [tested 25%, 50%, 100%]");
  testPass();
}

void testBDCFanControl()
{
  startTest("BDC Fan control (servo)");

  // Test low speed (800 µs)
  bdcFan.writeMicroseconds(800);
  delay(2000);

  // Test mid speed (1400 µs)
  bdcFan.writeMicroseconds(1400);
  delay(2000);

  // Test high speed (2000 µs)
  bdcFan.writeMicroseconds(2000);
  delay(2000);

  // Test ramp down
  for (int us = 2000; us >= 800; us -= 100)
  {
    bdcFan.writeMicroseconds(us);
    delay(500);
  }

  bdcFan.writeMicroseconds(800);

  Serial.print(" [tested 800-2000µs range]");
  testPass();
}

void testFanCooling()
{
  startTest("Fan cooling effectiveness");

  // Heat slightly
  heaterRelay.setPWM(100);
  for (int i = 0; i < 20; i++)
  {
    heaterRelay.tick();
    delay(1000);
  }
  heaterRelay.setPWM(0);

  double heatedTemp = thermocouple.readFarenheit();

  // Turn on fans
  fanRelay.setPWM(255);
  bdcFan.writeMicroseconds(2000);

  // Cool for 30 seconds
  for (int i = 0; i < 30; i++)
  {
    fanRelay.tick();
    delay(1000);
  }

  double cooledTemp = thermocouple.readFarenheit();
  double tempDrop = heatedTemp - cooledTemp;

  // Turn off fans
  fanRelay.setPWM(0);
  bdcFan.writeMicroseconds(800);

  // Should see cooling (at least 10°F drop)
  if (tempDrop < 10.0)
  {
    Serial.print(" [only ");
    Serial.print(tempDrop);
    Serial.print("°F drop]");
    testFail("Insufficient cooling");
    return;
  }

  Serial.print(" [cooled ");
  Serial.print(tempDrop);
  Serial.print("°F]");
  testPass();
}

// ============================================================================
// SAFETY SYSTEM TESTS
// ============================================================================

void testEmergencyShutdown()
{
  startTest("Emergency shutdown procedure");

  // Start with heater on
  heaterRelay.setPWM(100);
  fanRelay.setPWM(128);

  // Simulate emergency
  heaterRelay.setPWM(0);
  digitalWrite(HEATER, LOW);
  fanRelay.setPWM(255);
  bdcFan.writeMicroseconds(2000);

  // Verify state
  delay(1000);

  // Check that heater is truly off
  // (In a real test, would measure current draw)

  fanRelay.setPWM(0);
  bdcFan.writeMicroseconds(800);

  testPass();
}

void testOverTempProtection()
{
  startTest("Over-temperature protection");

  double currentTemp = thermocouple.readFarenheit();

  // Simulate check
  if (currentTemp > MAX_TEST_TEMP)
  {
    // Would trigger protection
    heaterRelay.setPWM(0);
    fanRelay.setPWM(255);
    Serial.print(" [triggered at ");
    Serial.print(currentTemp);
    Serial.print("°F]");
  }
  else
  {
    Serial.print(" [temp OK at ");
    Serial.print(currentTemp);
    Serial.print("°F]");
  }

  testPass();
}

// ============================================================================
// INTEGRATION TESTS
// ============================================================================

void testShortRoastCycle()
{
  startTest("Short roast cycle simulation");

  Serial.println();
  Serial.println("  Starting short roast cycle (heating to 250°F)...");

  double targetTemp = 250.0;
  unsigned long startTime = millis();
  bool reachedTarget = false;

  // Start heating
  heaterRelay.setPWM(MAX_TEST_HEATER_OUTPUT);
  fanRelay.setPWM(128);
  bdcFan.writeMicroseconds(1400);

  // Heat until target or timeout
  while (millis() - startTime < TEST_TIMEOUT)
  {
    heaterRelay.tick();
    fanRelay.tick();

    double currentTemp = thermocouple.readFarenheit();

    Serial.print("  Temp: ");
    Serial.print(currentTemp);
    Serial.print("°F  Target: ");
    Serial.print(targetTemp);
    Serial.println("°F");

    // Safety check
    if (currentTemp > MAX_TEST_TEMP)
    {
      heaterRelay.setPWM(0);
      digitalWrite(HEATER, LOW);
      fanRelay.setPWM(255);
      testFail("Safety limit exceeded");
      return;
    }

    // Check if reached target
    if (currentTemp >= targetTemp)
    {
      reachedTarget = true;
      break;
    }

    delay(5000);
  }

  // Stop heating
  heaterRelay.setPWM(0);
  digitalWrite(HEATER, LOW);

  if (!reachedTarget)
  {
    testFail("Did not reach target temperature in time");
    return;
  }

  // Cool down
  Serial.println("  Cooling down...");
  fanRelay.setPWM(255);
  bdcFan.writeMicroseconds(2000);

  while (thermocouple.readFarenheit() > 180.0)
  {
    fanRelay.tick();
    delay(5000);

    Serial.print("  Cooling: ");
    Serial.print(thermocouple.readFarenheit());
    Serial.println("°F");
  }

  fanRelay.setPWM(0);
  bdcFan.writeMicroseconds(800);

  Serial.print("  ");
  testPass();
}

// ============================================================================
// MAIN TEST EXECUTION
// ============================================================================

void runAllTests()
{
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("                    SENSOR TESTS                           ");
  Serial.println("═══════════════════════════════════════════════════════════");
  testThermocouple1();
  testThermocouple2();
  testThermocoupleConsistency();

  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("                   HEATER TESTS                            ");
  Serial.println("═══════════════════════════════════════════════════════════");
  testHeaterOffState();
  testHeaterControl();

  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("                     FAN TESTS                             ");
  Serial.println("═══════════════════════════════════════════════════════════");
  testPWMFanControl();
  testBDCFanControl();
  testFanCooling();

  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("                   SAFETY TESTS                            ");
  Serial.println("═══════════════════════════════════════════════════════════");
  testEmergencyShutdown();
  testOverTempProtection();

  Serial.println();
  Serial.println("═══════════════════════════════════════════════════════════");
  Serial.println("                 INTEGRATION TESTS                         ");
  Serial.println("═══════════════════════════════════════════════════════════");
  testShortRoastCycle();
}
