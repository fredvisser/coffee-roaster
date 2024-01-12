#include <EasyNextionLibrary.h>
#include <max6675.h>
#include <EEPROM.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <SPI.h>
#include <WiFi.h>
#include "ArduinoJson.h"

#define DEBUG

#ifdef DEBUG
#define DEBUG_SERIALBEGIN(x) Serial.begin(x)
#define DEBUG_PRINT(x) Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)

#else
#define DEBUG_SERIALBEGIN(x) // blank line
#define DEBUG_PRINT(x)       // blank line
#define DEBUG_PRINTLN(x)     // blank line
#endif

#define TC1_CS 10
// #define SCK 13
// #define MISO 12
#define HEATER A0
#define FAN A1

SimpleTimer checkTempTimer(500);
SimpleTimer tickTimer(5);
SimpleTimer stateMachineTimer(500);
SimpleTimer testTimer(2000);

PWMrelay heaterRelay(HEATER, HIGH);

byte roasterState = 0; // 1- idle 2- roasting 3- cooling 4- error

struct SetPoints
{
  uint32_t temps[3];  // in deg F
  uint32_t times[3];  // in ms
  uint32_t powers[3]; // in %
};

SetPoints setpoints = {
    {370, 405, 444},          // in deg F
    {150000, 300000, 480000}, // in ms
    {90, 90, 100}             // in %
};

byte setpointIndex = 0;
int setpointTemp = 0, lastSetpointTemp = 0;
uint32_t setpointTime = 0;
byte setpointPower = 0;

float currentTemp = 0;
uint32_t roastStartTime = 0, setpointStartTime = 0;
int setpointProgress = 0;

MAX6675 thermocouple(SCK, TC1_CS, MISO);

#ifdef ARDUINO_ARCH_ESP32
#define HW_SERIAL Serial0
#else
#define HW_SERIAL Serial
#endif

EasyNex myNex(HW_SERIAL);
JsonDocument doc;
String json;


void setup()
{
  DEBUG_SERIALBEGIN(115200);
  myNex.begin(115200);

  pinMode(HEATER, OUTPUT);
  pinMode(FAN, OUTPUT);

  // Explicitly enable the pinMode for the SPI pins because the library doesn't appear to do this correctly when running on an ESP32
  pinMode(TC1_CS, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  digitalWrite(HEATER, LOW);
  digitalWrite(FAN, LOW);

  EEPROM.get(0, setpoints);
  myNex.writeNum("ConfigSetpoint.spTemp1.val", setpoints.temps[0]);
  myNex.writeNum("ConfigSetpoint.spTemp2.val", setpoints.temps[1]);
  myNex.writeNum("ConfigSetpoint.spTemp3.val", setpoints.temps[2]);
  myNex.writeNum("ConfigSetpoint.spTime1.val", setpoints.times[0] / 1000);
  myNex.writeNum("ConfigSetpoint.spTime2.val", setpoints.times[1] / 1000);
  myNex.writeNum("ConfigSetpoint.spTime3.val", setpoints.times[2] / 1000);

  myNex.writeStr("page Start");

  DEBUG_PRINTLN("Setup complete");
}

void loop()
{
  if (tickTimer.isReady())
  {
    myNex.NextionListen();
    heaterRelay.tick();
    tickTimer.reset();
  }

  if (checkTempTimer.isReady())
  {
    currentTemp = thermocouple.readFarenheit();
    // currentRoastTemp = currentTemp;
    DEBUG_PRINT("Temp: ");
    DEBUG_PRINTLN(currentTemp);
    checkTempTimer.reset();
    // digitalWrite(14, !digitalRead(14));
  }

  if (stateMachineTimer.isReady())
  {
    switch (roasterState)
    {
    case 0: // idle
      // digitalWrite(HEATER, LOW);
      heaterRelay.setPWM(0);
      digitalWrite(FAN, LOW);
      break;

    case 1: // start roast
      DEBUG_PRINTLN("Start roast");
      roasterState = 2;

      roastStartTime = millis();
      setpointStartTime = roastStartTime;
      setpointIndex = 0;

      DEBUG_PRINT("Roast start time: ");
      DEBUG_PRINTLN(roastStartTime);

      // digitalWrite(HEATER, HIGH);
      digitalWrite(FAN, HIGH);
      delay(500);
      heaterRelay.setPWM(120);

      EEPROM.put(0, setpoints);
      break;

    case 2: // roasting
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
      myNex.writeNum("globals.setpointPower.val", round(setpointPower * 100));

      if (setpointIndex == 0)
      {
        setpointProgress = (millis() - roastStartTime) * 100 / setpoints.times[0];
      }
      else
      {
        setpointProgress = (millis() - roastStartTime - setpoints.times[setpointIndex - 1]) * 100 / (setpoints.times[setpointIndex] - setpoints.times[setpointIndex - 1]);
      }
      myNex.writeNum("globals.setpointProg.val", setpointProgress);

      if ((millis() >= (setpointTime + roastStartTime)) && setpointIndex < 2)
      {
        setpointIndex++;
        setpointTime = setpoints.times[setpointIndex];
        setpointTemp = setpoints.temps[setpointIndex];
        setpointPower = setpoints.powers[setpointIndex] / 100;
      }

      if ((currentTemp >= setpoints.temps[2]) && millis() >= (setpointTime + roastStartTime))
      {
        // digitalWrite(HEATER, LOW);
        heaterRelay.setPWM(0);
        roasterState = 3; // cooling
        myNex.writeStr("page Cooling");
        DEBUG_PRINTLN("Roast complete -> cooling");
      }
      else if ((currentTemp >= setpointTemp) && millis() <= (setpointTime + roastStartTime))
      {
        // digitalWrite(HEATER, LOW);
        heaterRelay.setPWM(setpointPower / 2);
        DEBUG_PRINTLN("Temp setpoint before time setpoint");
      }
      else if ((currentTemp < setpointTemp) && heaterRelay.getPWM() != setpointPower)
      {
        // digitalWrite(HEATER, HIGH);
        heaterRelay.setPWM(setpointPower);
        DEBUG_PRINTLN("Re-enable heat");
      }
      break;

    case 3:
      // digitalWrite(HEATER, LOW);
      heaterRelay.setPWM(0);
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      DEBUG_PRINTLN("Cooling");
      if (currentTemp <= 145)
      {
        DEBUG_PRINTLN("Cooling - stopped");
        digitalWrite(FAN, LOW);
        roasterState = 0; // idle
        myNex.writeStr("page Start");
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

  setpoints.temps[0] = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  if (setpoints.temps[0] == 777777)
  {
    setpoints.temps[0] = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  }
  setpoints.temps[1] = myNex.readNumber("ConfigSetpoint.spTemp2.val");
  if (setpoints.temps[1] == 777777)
  {
    setpoints.temps[1] = myNex.readNumber("ConfigSetpoint.spTemp2.val");
  }
  setpoints.temps[2] = myNex.readNumber("ConfigSetpoint.spTemp3.val");
  if (setpoints.temps[2] == 777777)
  {
    setpoints.temps[2] = myNex.readNumber("ConfigSetpoint.spTemp3.val");
  }

  DEBUG_PRINTLN("setpoints.temps: ");
  DEBUG_PRINTLN(setpoints.temps[0]);
  DEBUG_PRINTLN(setpoints.temps[1]);
  DEBUG_PRINTLN(setpoints.temps[2]);

  setpoints.times[0] = myNex.readNumber("ConfigSetpoint.spTime1.val");
  if (setpoints.times[0] == 777777)
  {
    setpoints.times[0] = myNex.readNumber("ConfigSetpoint.spTime1.val");
  }
  setpoints.times[1] = myNex.readNumber("ConfigSetpoint.spTime2.val");
  if (setpoints.times[1] == 777777)
  {
    setpoints.times[1] = myNex.readNumber("ConfigSetpoint.spTime2.val");
  }
  setpoints.times[2] = myNex.readNumber("ConfigSetpoint.spTime3.val");
  if (setpoints.times[2] == 777777)
  {
    setpoints.times[2] = myNex.readNumber("ConfigSetpoint.spTime3.val");
  }
  for (int i = 0; i <= 2; i++)
  {
    setpoints.times[i] = setpoints.times[i] * 1000;
  }

  DEBUG_PRINTLN("setpoints.times: ");
  DEBUG_PRINTLN(setpoints.times[0]);
  DEBUG_PRINTLN(setpoints.times[1]);
  DEBUG_PRINTLN(setpoints.times[2]);

  setpoints.powers[0] = myNex.readNumber("ConfigSetpoint.spPower1.val");
  if (setpoints.powers[0] == 777777)
  {
    setpoints.powers[0] = myNex.readNumber("ConfigSetpoint.spPower1.val");
  }
  setpoints.powers[1] = myNex.readNumber("ConfigSetpoint.spPower2.val");
  if (setpoints.powers[1] == 777777)
  {
    setpoints.powers[1] = myNex.readNumber("ConfigSetpoint.spPower2.val");
  }
  setpoints.powers[2] = myNex.readNumber("ConfigSetpoint.spPower3.val");
  if (setpoints.powers[2] == 777777)
  {
    setpoints.powers[2] = myNex.readNumber("ConfigSetpoint.spPower3.val");
  }

  for (int i = 0; i <= 2; i++)
  {
    setpoints.powers[i] = setpoints.powers[i] * 255;
  }

  DEBUG_PRINTLN("setpoints.powers: ");
  DEBUG_PRINTLN(setpoints.powers[0]);
  DEBUG_PRINTLN(setpoints.powers[1]);
  DEBUG_PRINTLN(setpoints.powers[2]);

  setpointTemp = setpoints.temps[0];
  setpointTime = setpoints.times[0];
  setpointPower = setpoints.powers[0] / 100;

  roasterState = 1;
  myNex.writeStr("page Roasting");
  myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
}

void trigger1()
{ // Stop roast command received
  roasterState = 3;
  myNex.writeStr("page Cooling");
  myNex.writeNum("globals.nextSetTempNum.val", 145);
}

void trigger2()
{ // Stop cooling command received
  roasterState = 0;
  myNex.writeStr("page Start");
}
