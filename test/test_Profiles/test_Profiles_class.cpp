#include <unity.h>
#include "Profiles.hpp"
#include <stdio.h>

Profiles test_profile;

void setUp(void)
{
}

void tearDown(void)
{
  test_profile.clearSetpoints();
}

void test_profile_constructor(void)
{
  TEST_ASSERT_EQUAL_INT(1, test_profile.getSetpointCount());
}

void test_profile_addSetpoint(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 100);
  test_profile.addSetpoint(300000, 380, 100);
  test_profile.addSetpoint(480000, 440, 90);
  TEST_ASSERT_EQUAL_INT(4, test_profile.getSetpointCount());
}

void test_profile_getProfileProgress(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 100);
  test_profile.addSetpoint(300000, 380, 100);
  test_profile.addSetpoint(480000, 440, 90);
  test_profile.startProfile(75, 0);
  TEST_ASSERT_EQUAL_INT(0, test_profile.getProfileProgress(0));
  TEST_ASSERT_EQUAL_INT(50, test_profile.getProfileProgress(240000));
  TEST_ASSERT_EQUAL_INT(100, test_profile.getProfileProgress(480000));
}

void test_profile_getTargetTemp(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 100);
  test_profile.addSetpoint(300000, 380, 100);
  test_profile.addSetpoint(480000, 440, 90);
  test_profile.startProfile(75, 0);
  TEST_ASSERT_EQUAL_INT(75, test_profile.getTargetTemp(0));
  TEST_ASSERT_EQUAL_INT(187, test_profile.getTargetTemp(75000));
  TEST_ASSERT_EQUAL_INT(348, test_profile.getTargetTemp(240000));
  TEST_ASSERT_EQUAL_INT(440, test_profile.getTargetTemp(480000));
  TEST_ASSERT_EQUAL_INT(440, test_profile.getTargetTemp(500000));
}

void test_profile_getTargetFanSpeed(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 90);
  test_profile.addSetpoint(300000, 380, 80);
  test_profile.addSetpoint(480000, 440, 70);
  test_profile.startProfile(75, 0);
  TEST_ASSERT_EQUAL_INT(229, test_profile.getTargetFanSpeed(0));
  TEST_ASSERT_EQUAL_INT(229, test_profile.getTargetFanSpeed(1));
  TEST_ASSERT_EQUAL_INT(229, test_profile.getTargetFanSpeed(75000));
  TEST_ASSERT_EQUAL_INT(229, test_profile.getTargetFanSpeed(150000));
  TEST_ASSERT_EQUAL_INT(226, test_profile.getTargetFanSpeed(170000));
  TEST_ASSERT_EQUAL_INT(214, test_profile.getTargetFanSpeed(240000));
  TEST_ASSERT_EQUAL_INT(178, test_profile.getTargetFanSpeed(480000));
  TEST_ASSERT_EQUAL_INT(178, test_profile.getTargetFanSpeed(500000));
}

void test_profile_getFinalTargetTemp(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 90);
  test_profile.addSetpoint(300000, 380, 80);
  test_profile.addSetpoint(480000, 440, 70);
  test_profile.startProfile(75, 0);
  TEST_ASSERT_EQUAL_INT(440, test_profile.getFinalTargetTemp());
}

void test_profile_startProfile(void)
{
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 90);
  test_profile.addSetpoint(300000, 380, 80);
  test_profile.addSetpoint(480000, 440, 70);
  test_profile.startProfile(75, 0);
  TEST_ASSERT_EQUAL_INT(0, test_profile.getProfileProgress(0));
  TEST_ASSERT_EQUAL_INT(15, test_profile.getProfileProgress(75000));
  TEST_ASSERT_EQUAL_INT(50, test_profile.getProfileProgress(240000));
  TEST_ASSERT_EQUAL_INT(100, test_profile.getProfileProgress(480000));
  TEST_ASSERT_EQUAL_INT(100, test_profile.getProfileProgress(500000));
}

void test_profile_flattenProfile(void)
{
  uint8_t buffer[200];
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 90);
  test_profile.addSetpoint(300000, 380, 80);
  test_profile.addSetpoint(480000, 440, 70);
  test_profile.flattenProfile(buffer);
  TEST_ASSERT_EQUAL_INT(4, buffer[3]);
  test_profile.clearSetpoints();
  TEST_ASSERT_EQUAL_INT(1, test_profile.getSetpointCount());
  test_profile.unflattenProfile(buffer);
  TEST_ASSERT_EQUAL_INT(4, test_profile.getSetpointCount());
  TEST_ASSERT_EQUAL_INT(440, test_profile.getFinalTargetTemp());
}

void test_profile_unflattenBadProfile(void)
{
  uint8_t buffer[200];
  test_profile.clearSetpoints();
  test_profile.addSetpoint(150000, 300, 90);
  test_profile.addSetpoint(300000, 380, 80);
  test_profile.addSetpoint(480000, 440, 70);
  TEST_ASSERT_EQUAL_INT(1, test_profile.getSetpointCount());
  test_profile.unflattenProfile(buffer);
  TEST_ASSERT_EQUAL_INT(4, test_profile.getSetpointCount());
  TEST_ASSERT_EQUAL_INT(440, test_profile.getFinalTargetTemp());
}

void RUN_UNITY_TESTS()
{
  UNITY_BEGIN();

  RUN_TEST(test_profile_constructor);
  RUN_TEST(test_profile_addSetpoint);
  RUN_TEST(test_profile_getProfileProgress);
  RUN_TEST(test_profile_getTargetTemp);
  RUN_TEST(test_profile_getTargetFanSpeed);
  RUN_TEST(test_profile_getFinalTargetTemp);
  RUN_TEST(test_profile_startProfile);
  RUN_TEST(test_profile_flattenProfile);

  UNITY_END(); // stop unit testing
}

#ifdef ARDUINO

#include <Arduino.h>
void setup()
{
  // NOTE!!! Wait for >2 secs
  // if board doesn't support software reset via Serial.DTR/RTS
  delay(2000);

  RUN_UNITY_TESTS();
}

void loop()
{
  digitalWrite(13, HIGH);
  delay(100);
  digitalWrite(13, LOW);
  delay(500);
}

#else

int main(int argc, char **argv)
{
  RUN_UNITY_TESTS();
  return 0;
}

#endif
