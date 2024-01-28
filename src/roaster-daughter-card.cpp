#ifdef NATIVE

int main(int argc, char **argv)
{
  int i = 0;
  return 0;
}

#else

#include "Arduino.h"
#include <EasyNextionLibrary.h>
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <AutoPID.h>
#include <SPI.h>
#include <EEPROM.h>
#include <Settings.h>
#include <Profiles.hpp>
#ifdef ARDUINO_ARCH_ESP32
#include <Network.hpp>
#include "settings.h"
#include <Preferences.h>
Preferences preferences;
#endif

// #define DEBUG

#ifdef DEBUG
#define DEBUG_SERIALBEGIN(x) Serial.begin(x)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)

#else
#define DEBUG_SERIALBEGIN(x) // blank line
#define DEBUG_PRINT(x)       // blank line
#define DEBUG_PRINTLN(x)     // blank line
#endif

#ifdef ARDUINO_ARCH_ESP32
#define HW_SERIAL Serial0
#else
#define HW_SERIAL Serial
#endif

// Pin definitions
#define TC1_CS 10
#define HEATER A0
#define FAN A1

// PID settings and gains
#define OUTPUT_MIN 0
#define OUTPUT_MAX 255
#define KP 8.0
#define KI 0.46
#define KD 0

#define VERSION "2024-01-28"

// Use timers for simple multitasking
SimpleTimer checkTempTimer(250);
SimpleTimer tickTimer(5);
SimpleTimer stateMachineTimer(500);

// PWM is used to control fan and heater outputs
PWMrelay heaterRelay(HEATER, HIGH);
PWMrelay fanRelay(FAN, HIGH);

// Create a roast profile object
Profiles profile;

// Roaster state variables
double currentTemp = 0;
double setpointTemp = 0;
double heaterOutputVal = 0;
byte setpointFanSpeed = 0;
int setpointProgress = 0;

enum RoasterState
{
  IDLE = 0,
  START_ROAST = 1,
  ROASTING = 2,
  COOLING = 3,
  ERROR = 4
};

RoasterState roasterState = IDLE;

MAX6675 thermocouple(SCK, TC1_CS, MISO);
AutoPID heaterPID(&currentTemp, &setpointTemp, &heaterOutputVal, 0, 255, KP, KI, KD);

// Create a Nextion display connection
EasyNex myNex(HW_SERIAL);

// Default roast profile. Any changes stored in EEPROM will override these values.
void setDefaultRoastProfile()
{
  profile.clearSetpoints();
  profile.addSetpoint(150000, 300, 100);
  profile.addSetpoint(300000, 380, 90);
  profile.addSetpoint(480000, 440, 90);
}
uint8_t profileBuffer[200];

void setup()
{
  myNex.begin(115200);

  // Set heater and fan control pins to output
  pinMode(HEATER, OUTPUT);
  pinMode(FAN, OUTPUT);

  // Explicitly enable the pinMode for the SPI pins because the library doesn't appear to do this correctly when running on an ESP32
  pinMode(TC1_CS, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  // Set output pins to safe state (LOW)
  digitalWrite(HEATER, LOW);
  heaterPID.setTimeStep(250);
  digitalWrite(FAN, LOW);
  fanRelay.setPeriod(10);

  setDefaultRoastProfile();
#ifdef ARDUINO_ARCH_ESP32
#elif ARDUINO_ARCH_AVR
  EEPROM.begin();
#endif

#ifdef ARDUINO_ARCH_ESP32
  WifiCredentials wifiCredentials = {SSID_VALUE, PASSWORD_VALUE};
  preferences.begin("roaster", false);
  wifiCredentials.ssid = preferences.getString("ssid", "");
  wifiCredentials.password = preferences.getString("password", "");
  String ipAddress = initializeWifi(wifiCredentials);
  if (ipAddress != "Failed to connect to WiFi")
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }
#endif

  // EEPROM.get(0, profileBuffer);
  preferences.getBytes("profile", profileBuffer, 200);
  profile.unflattenProfile(profileBuffer);

  myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
  myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
  myNex.writeNum("ConfigSetpoint.spTemp2.val", profile.getSetpoint(2).temp);
  myNex.writeNum("ConfigSetpoint.spTemp3.val", profile.getSetpoint(3).temp);
  myNex.writeNum("ConfigSetpoint.spTime1.val", profile.getSetpoint(1).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime2.val", profile.getSetpoint(2).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime3.val", profile.getSetpoint(3).time / 1000);
  myNex.writeNum("ConfigSetpoint.spFan1.val", profile.getSetpoint(1).fanSpeed);
  myNex.writeNum("ConfigSetpoint.spFan2.val", profile.getSetpoint(2).fanSpeed);
  myNex.writeNum("ConfigSetpoint.spFan3.val", profile.getSetpoint(3).fanSpeed);
  myNex.writeStr("ConfigWifi.ip.txt", ipAddress);
  myNex.writeStr("ConfigNav.rev.txt", VERSION);

  myNex.writeStr("page Start");

  DEBUG_PRINTLN("Setup complete");
}

void loop()
{
  if (tickTimer.isReady())
  {
    myNex.NextionListen();
    heaterRelay.tick();
    fanRelay.tick();
    heaterPID.run();
    wsCleanup();
    tickTimer.reset();
  }

  if (checkTempTimer.isReady())
  {
    currentTemp = thermocouple.readFarenheit();
    DEBUG_PRINT("Temp: ");
    DEBUG_PRINTLN(currentTemp);
    DEBUG_PRINT("Fan speed: ");
    DEBUG_PRINTLN(setpointFanSpeed);
    DEBUG_PRINT("Heater output: ");
    DEBUG_PRINTLN(heaterOutputVal);
    DEBUG_PRINT("Setpoint temp: ");
    DEBUG_PRINTLN(setpointTemp);
    DEBUG_PRINT("Setpoint progress: ");
    DEBUG_PRINTLN(setpointProgress);

    checkTempTimer.reset();
  }

  if (stateMachineTimer.isReady())
  {
    switch (roasterState)
    {
    case IDLE:
      // heaterRelay.setPWM(0);
      // fanRelay.setPWM(0);
      digitalWrite(HEATER, LOW);
      digitalWrite(FAN, LOW);
      heaterPID.stop();
      heaterOutputVal = 0;
      break;

    case START_ROAST:
      DEBUG_PRINTLN("Start roast");
      roasterState = ROASTING;

      fanRelay.setPWM(profile.getTargetFanSpeed(millis()));
      delay(500);
      profile.startProfile((int)currentTemp, millis());
      heaterPID.run();
      heaterRelay.setPWM(heaterOutputVal);

      sendWsMessage("{ \"pushMessage\": \"startRoasting\" }");

      profile.flattenProfile(profileBuffer);
      preferences.putBytes("profile", profileBuffer, 200);
      // EEPROM.put(0, profileBuffer);
      break;

    case ROASTING:
      setpointTemp = profile.getTargetTemp(millis());
      setpointFanSpeed = profile.getTargetFanSpeed(millis());
      setpointProgress = profile.getProfileProgress(millis());

      fanRelay.setPWM(setpointFanSpeed);
      heaterRelay.setPWM(heaterOutputVal);

      if (currentTemp >= profile.getFinalTargetTemp())
      {
        heaterPID.stop();
        heaterOutputVal = 0;
        heaterRelay.setPWM(heaterOutputVal);
        // digitalWrite(HEATER, LOW);

        setpointTemp = 145;
        roasterState = COOLING;

        setpointFanSpeed = 255;
        fanRelay.setPWM(setpointFanSpeed);

        setpointProgress = 0;
        myNex.writeStr("page Cooling");
        DEBUG_PRINTLN("Roast complete -> cooling");
      }
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
      myNex.writeNum("globals.setpointFan.val", round(setpointFanSpeed * 100 / 255));
      myNex.writeNum("globals.setpointProg.val", setpointProgress);
      break;

    case COOLING:
      digitalWrite(HEATER, LOW);
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      DEBUG_PRINTLN("Cooling");

      if (currentTemp <= 145)
      {
        fanRelay.setPWM(0);
        digitalWrite(FAN, LOW);
        roasterState = IDLE;

        sendWsMessage("{ \"pushMessage\": \"endRoasting\" }");
        myNex.writeStr("page Start");
        DEBUG_PRINTLN("Cooling - stopped");
      }
      break;
    default:
      DEBUG_PRINTLN("Hit default case!!");
      break;
    }
    stateMachineTimer.reset();
  }
}

void trigger0()
{ // Start roast command received
  int temp1 = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  if (temp1 == 777777)
  {
    temp1 = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  }
  int temp2 = myNex.readNumber("ConfigSetpoint.spTemp2.val");
  if (temp2 == 777777)
  {
    temp2 = myNex.readNumber("ConfigSetpoint.spTemp2.val");
  }
  int temp3 = myNex.readNumber("ConfigSetpoint.spTemp3.val");
  if (temp3 == 777777)
  {
    temp3 = myNex.readNumber("ConfigSetpoint.spTemp3.val");
  }
  int time1 = myNex.readNumber("ConfigSetpoint.spTime1.val");
  if (time1 == 777777)
  {
    time1 = myNex.readNumber("ConfigSetpoint.spTime1.val");
  }
  int time2 = myNex.readNumber("ConfigSetpoint.spTime2.val");
  if (time2 == 777777)
  {
    time2 = myNex.readNumber("ConfigSetpoint.spTime2.val");
  }
  int time3 = myNex.readNumber("ConfigSetpoint.spTime3.val");
  if (time3 == 777777)
  {
    time3 = myNex.readNumber("ConfigSetpoint.spTime3.val");
  }
  int power1 = myNex.readNumber("ConfigSetpoint.spPower1.val");
  if (power1 == 777777)
  {
    power1 = myNex.readNumber("ConfigSetpoint.spFan1.val");
  }
  int power2 = myNex.readNumber("ConfigSetpoint.spFan2.val");
  if (power2 == 777777)
  {
    power2 = myNex.readNumber("ConfigSetpoint.spFan2.val");
  }
  int power3 = myNex.readNumber("ConfigSetpoint.spFan3.val");
  if (power3 == 777777)
  {
    power3 = myNex.readNumber("ConfigSetpoint.spFan3.val");
  }

  profile.clearSetpoints();
  profile.addSetpoint(time1 * 1000, temp1, power1);
  profile.addSetpoint(time2 * 1000, temp2, power2);
  profile.addSetpoint(time3 * 1000, temp3, power3);

  roasterState = START_ROAST;
  myNex.writeStr("page Roasting");
  myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
}

void trigger1()
{ // Stop roast command received
  roasterState = COOLING;

  heaterPID.stop();
  heaterOutputVal = 0;
  heaterRelay.setPWM(heaterOutputVal);
  digitalWrite(HEATER, LOW);

  setpointFanSpeed = 255;
  fanRelay.setPWM(setpointFanSpeed);

  myNex.writeStr("page Cooling");
  myNex.writeNum("globals.nextSetTempNum.val", 145);
}

void trigger2()
{ // Stop cooling command received
  roasterState = IDLE;
  myNex.writeStr("page Start");
}

void trigger3()
{ // Apply WiFi credentials
#ifdef ARDUINO_ARCH_ESP32
  WifiCredentials wifiCredentials;
  wifiCredentials.ssid = myNex.readStr("ConfigWifi.ssid.txt");
  wifiCredentials.password = myNex.readStr("ConfigWifi.password.txt");
  myNex.writeStr("ConfigWifi.ip.txt", "Connecting...");
  String ip = initializeWifi(wifiCredentials);
  myNex.writeStr("ConfigWifi.ip.txt", ip);
  if (ip != "Failed to connect to WiFi")
  {
    preferences.putString("ssid", wifiCredentials.ssid);
    preferences.putString("password", wifiCredentials.password);
  }
#endif
}

#endif