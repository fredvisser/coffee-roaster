/**
 * Profile Management Unit Tests
 * 
 * Tests for the Profiles class including:
 * - Setpoint interpolation accuracy
 * - Profile serialization/deserialization
 * - Boundary conditions
 * - Profile state transitions
 */

#include <AUnit.h>
#include "../../Profiles.hpp"

using namespace aunit;

// Test fixtures
Profiles testProfile;
uint8_t buffer[200];

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for serial port to connect
  delay(1000);
}

void loop() {
  TestRunner::run();
}

// ============================================================================
// SETPOINT MANAGEMENT TESTS
// ============================================================================

test(Profile_AddSetpoint_Basic) {
  Profiles profile;
  profile.clearSetpoints();
  
  profile.addSetpoint(60000, 200, 50);
  profile.addSetpoint(120000, 300, 75);
  profile.addSetpoint(180000, 400, 100);
  
  assertEqual(4, profile.getSetpointCount()); // 3 added + 1 default (0,0,0)
  
  auto sp1 = profile.getSetpoint(1);
  assertEqual((uint32_t)60000, sp1.time);
  assertEqual((uint32_t)200, sp1.temp);
  assertEqual((uint32_t)50, sp1.fanSpeed);
}

test(Profile_AddSetpoint_MaxCapacity) {
  Profiles profile;
  profile.clearSetpoints();
  
  // Try to add 11 setpoints (max is 10)
  for (int i = 0; i < 11; i++) {
    profile.addSetpoint(i * 10000, 100 + i * 10, 50);
  }
  
  // Should only have 10 setpoints (including the default one)
  assertEqual(10, profile.getSetpointCount());
}

test(Profile_ClearSetpoints) {
  Profiles profile;
  profile.addSetpoint(60000, 200, 50);
  profile.addSetpoint(120000, 300, 75);
  
  profile.clearSetpoints();
  
  // After clear, should have 1 default setpoint (0,0,0)
  assertEqual(1, profile.getSetpointCount());
  
  auto sp0 = profile.getSetpoint(0);
  assertEqual((uint32_t)0, sp0.time);
  assertEqual((uint32_t)0, sp0.temp);
}

// ============================================================================
// TEMPERATURE INTERPOLATION TESTS
// ============================================================================

test(Profile_GetTargetTemp_BeforeStart) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  // Start at time 10000, temp 150
  profile.startProfile(150, 10000);
  
  // At exactly start time, should return start temp
  uint32_t temp = profile.getTargetTemp(10000);
  assertEqual((uint32_t)150, temp);
}

test(Profile_GetTargetTemp_LinearInterpolation) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  // Halfway between start (200°F at 10000ms) and first setpoint (300°F at 70000ms)
  // At 40000ms: 30000ms into 60000ms range = 50%
  uint32_t temp = profile.getTargetTemp(40000);
  assertEqual((uint32_t)250, temp); // 200 + (300-200)*0.5 = 250
}

test(Profile_GetTargetTemp_BetweenSetpoints) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  // Between setpoint 1 (300°F at 70000ms) and setpoint 2 (400°F at 130000ms)
  // At 100000ms: 30000ms into 60000ms range = 50%
  uint32_t temp = profile.getTargetTemp(100000);
  assertEqual((uint32_t)350, temp); // 300 + (400-300)*0.5 = 350
}

test(Profile_GetTargetTemp_AfterLastSetpoint) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  // After last setpoint, should return final temp
  uint32_t temp = profile.getTargetTemp(200000);
  assertEqual((uint32_t)400, temp);
}

test(Profile_GetFinalTargetTemp) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  profile.addSetpoint(180000, 450, 90);
  
  assertEqual((uint32_t)450, profile.getFinalTargetTemp());
}

// ============================================================================
// FAN SPEED INTERPOLATION TESTS
// ============================================================================

test(Profile_GetTargetFanSpeed_Start) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 80);
  
  profile.startProfile(200, 10000);
  
  // At start, should use first setpoint's fan speed
  uint32_t fanSpeed = profile.getTargetFanSpeed(10000);
  // 50% = 127.5, should be 127 or 128 (50 * 255 / 100)
  assertTrue(fanSpeed >= 127 && fanSpeed <= 128);
}

test(Profile_GetTargetFanSpeed_LinearInterpolation) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 40);
  profile.addSetpoint(120000, 400, 80);
  
  profile.startProfile(200, 10000);
  
  // Halfway between start (40% at 10000ms) and first setpoint (40% at 70000ms)
  // At 40000ms: 30000ms into 60000ms range = 50%
  // Fan: 40 + (40-40)*0.5 = 40% = 102 (40 * 255 / 100)
  uint32_t fanSpeed = profile.getTargetFanSpeed(40000);
  assertTrue(fanSpeed >= 101 && fanSpeed <= 103);
}

test(Profile_GetTargetFanSpeed_BetweenSetpoints) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 40);
  profile.addSetpoint(120000, 400, 80);
  
  profile.startProfile(200, 10000);
  
  // Between setpoint 1 (40% at 70000ms) and setpoint 2 (80% at 130000ms)
  // At 100000ms: 30000ms into 60000ms range = 50%
  // Fan: 40 + (80-40)*0.5 = 60% = 153 (60 * 255 / 100)
  uint32_t fanSpeed = profile.getTargetFanSpeed(100000);
  assertTrue(fanSpeed >= 152 && fanSpeed <= 154);
}

test(Profile_GetTargetFanSpeed_AfterLastSetpoint) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 100);
  
  profile.startProfile(200, 10000);
  
  // After last setpoint, should return final fan speed
  uint32_t fanSpeed = profile.getTargetFanSpeed(200000);
  assertEqual((uint32_t)255, fanSpeed); // 100% = 255
}

// ============================================================================
// PROFILE PROGRESS TESTS
// ============================================================================

test(Profile_GetProfileProgress_Start) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  uint32_t progress = profile.getProfileProgress(10000);
  assertEqual((uint32_t)0, progress);
}

test(Profile_GetProfileProgress_Halfway) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  // 120000ms total, halfway = 70000ms from start (10000 + 60000)
  uint32_t progress = profile.getProfileProgress(70000);
  assertEqual((uint32_t)50, progress);
}

test(Profile_GetProfileProgress_Complete) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  profile.addSetpoint(120000, 400, 75);
  
  profile.startProfile(200, 10000);
  
  // At or after final time
  uint32_t progress = profile.getProfileProgress(130000);
  assertEqual((uint32_t)100, progress);
}

// ============================================================================
// SERIALIZATION/DESERIALIZATION TESTS
// ============================================================================

test(Profile_Serialization_RoundTrip) {
  Profiles profile1;
  profile1.clearSetpoints();
  profile1.addSetpoint(60000, 300, 50);
  profile1.addSetpoint(120000, 400, 75);
  profile1.addSetpoint(180000, 450, 90);
  
  uint8_t buffer[200];
  profile1.flattenProfile(buffer);
  
  Profiles profile2;
  profile2.unflattenProfile(buffer);
  
  // Verify setpoint count
  assertEqual(profile1.getSetpointCount(), profile2.getSetpointCount());
  
  // Verify each setpoint
  for (int i = 0; i < profile1.getSetpointCount(); i++) {
    auto sp1 = profile1.getSetpoint(i);
    auto sp2 = profile2.getSetpoint(i);
    
    assertEqual(sp1.time, sp2.time);
    assertEqual(sp1.temp, sp2.temp);
    assertEqual(sp1.fanSpeed, sp2.fanSpeed);
  }
}

test(Profile_Serialization_EmptyProfile) {
  Profiles profile1;
  profile1.clearSetpoints();
  
  uint8_t buffer[200];
  profile1.flattenProfile(buffer);
  
  Profiles profile2;
  profile2.clearSetpoints();
  profile2.addSetpoint(99999, 999, 99); // Add garbage
  profile2.unflattenProfile(buffer);
  
  // Should have 1 setpoint (the default 0,0,0)
  assertEqual(1, profile2.getSetpointCount());
}

test(Profile_Deserialization_InvalidData) {
  uint8_t badBuffer[200];
  // Set count to 20 (invalid, max is 10)
  badBuffer[0] = 0;
  badBuffer[1] = 0;
  badBuffer[2] = 0;
  badBuffer[3] = 20;
  
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  int originalCount = profile.getSetpointCount();
  
  profile.unflattenProfile(badBuffer);
  
  // Should not change when data is invalid
  assertEqual(originalCount, profile.getSetpointCount());
}

test(Profile_Deserialization_ZeroCount) {
  uint8_t badBuffer[200];
  // Set count to 0
  badBuffer[0] = 0;
  badBuffer[1] = 0;
  badBuffer[2] = 0;
  badBuffer[3] = 0;
  
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 50);
  int originalCount = profile.getSetpointCount();
  
  profile.unflattenProfile(badBuffer);
  
  // Should not change when count is 0
  assertEqual(originalCount, profile.getSetpointCount());
}

// ============================================================================
// BOUNDARY CONDITION TESTS
// ============================================================================

test(Profile_BoundaryCondition_VeryShortProfile) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(1000, 250, 50); // Very short 1 second profile
  
  profile.startProfile(200, 0);
  
  // Should interpolate correctly even for short times
  uint32_t temp = profile.getTargetTemp(500);
  assertTrue(temp >= 200 && temp <= 250);
}

test(Profile_BoundaryCondition_VeryLongProfile) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(3600000, 300, 50); // 1 hour profile
  profile.addSetpoint(7200000, 450, 100); // 2 hour profile
  
  profile.startProfile(200, 0);
  
  // Should handle long times without overflow
  uint32_t temp = profile.getTargetTemp(3600000);
  assertEqual((uint32_t)300, temp);
}

test(Profile_BoundaryCondition_HighTemperatures) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 500, 50);
  profile.addSetpoint(120000, 600, 75);
  
  profile.startProfile(450, 10000);
  
  // Should handle high temperatures correctly
  uint32_t temp = profile.getTargetTemp(70000);
  assertTrue(temp >= 450 && temp <= 600);
}

test(Profile_BoundaryCondition_ZeroFanSpeed) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 0);
  
  profile.startProfile(200, 10000);
  
  uint32_t fanSpeed = profile.getTargetFanSpeed(70000);
  assertEqual((uint32_t)0, fanSpeed);
}

test(Profile_BoundaryCondition_MaxFanSpeed) {
  Profiles profile;
  profile.clearSetpoints();
  profile.addSetpoint(60000, 300, 100);
  
  profile.startProfile(200, 10000);
  
  uint32_t fanSpeed = profile.getTargetFanSpeed(70000);
  assertEqual((uint32_t)255, fanSpeed);
}
