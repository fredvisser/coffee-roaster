#include <EasyNextionLibrary.h>
#include <max6675.h>
#include <SimpleTimer.h>
#include <PWMrelay.h>
#include <SPI.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Settings.h>
#include <Profiles.h>

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

#ifdef ARDUINO_ARCH_ESP32
#define HW_SERIAL Serial0
#else
#define HW_SERIAL Serial
#endif

#define TC1_CS 10
// #define SCK 13
// #define MISO 12
#define HEATER A0
#define FAN A1

// Use timers for simple multitasking
SimpleTimer checkTempTimer(500);
SimpleTimer tickTimer(5);
SimpleTimer stateMachineTimer(500);

// PWM is used to control fan and heater outputs
PWMrelay heaterRelay(HEATER, HIGH);
// PWMrelay heaterRelay(FAN, HIGH);

Profiles profile;

byte setpointIndex = 1;
int setpointTemp = 0, lastSetpointTemp = 0;
uint32_t setpointTime = 0;
byte setpointFanSpeed = 0;

byte roasterState = 0; // 1- idle 2- roasting 3- cooling 4- error

float currentTemp = 0;
uint32_t roastStartTime = 0, setpointStartTime = 0;
int setpointProgress = 0;

MAX6675 thermocouple(SCK, TC1_CS, MISO);

EasyNex myNex(HW_SERIAL);
JsonDocument wsRequestDoc;
String json;

float numerator;
float denominator;
float slope;
int roastTime;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/WebSocket");

void initializeProfile()
{
  profile.clearSetpoints();
  profile.addSetpoint(180000, 300, 100);
  profile.addSetpoint(300000, 380, 100);
  profile.addSetpoint(480000, 440, 90);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    data[len] = 0;
    Serial.printf("%s\n", (char *)data);
    deserializeJson(wsRequestDoc, data);

    if (wsRequestDoc["command"] == "getData")
    {
      JsonDocument wsResponseDoc;
      wsResponseDoc["id"] = wsRequestDoc["id"];
      JsonObject temp_data = wsResponseDoc["data"].to<JsonObject>();
      temp_data["bt"] = currentTemp;
      temp_data["st"] = setpointTemp;
      String jsonResponse;
      serializeJson(wsResponseDoc, jsonResponse);
      ws.textAll(jsonResponse);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    // Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    // Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void initializeWifi()
{
  // Connect to Wi-Fi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connecting to WiFi..");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
  //           { request->send_P(200, "text/html", index_html, processor); });

  // Start server
  server.begin();
}

void setup()
{

  // DEBUG_SERIALBEGIN(115200);
  myNex.begin(115200);

  pinMode(HEATER, OUTPUT);
  pinMode(FAN, OUTPUT);

  // Explicitly enable the pinMode for the SPI pins because the library doesn't appear to do this correctly when running on an ESP32
  pinMode(TC1_CS, OUTPUT);
  pinMode(SCK, OUTPUT);
  pinMode(MISO, INPUT);

  digitalWrite(HEATER, LOW);
  digitalWrite(FAN, LOW);

  initializeWifi();
  initializeProfile();

  profile.loadProfileFromEEPROM();

  myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
  myNex.writeNum("ConfigSetpoint.spTemp1.val", profile.getSetpoint(1).temp);
  myNex.writeNum("ConfigSetpoint.spTemp2.val", profile.getSetpoint(2).temp);
  myNex.writeNum("ConfigSetpoint.spTemp3.val", profile.getSetpoint(3).temp);
  myNex.writeNum("ConfigSetpoint.spTime1.val", profile.getSetpoint(1).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime2.val", profile.getSetpoint(2).time / 1000);
  myNex.writeNum("ConfigSetpoint.spTime3.val", profile.getSetpoint(3).time / 1000);
  myNex.writeNum("ConfigSetpoint.spPower1.val", 100);
  myNex.writeNum("ConfigSetpoint.spPower2.val", 100);
  myNex.writeNum("ConfigSetpoint.spPower3.val", 100);

  myNex.writeStr("page Start");

  Serial.printf("Profile setpoints: %d\n", profile.getSetpointCount());
  for (int i = 0; i < profile.getSetpointCount(); i++)
  {
    Serial.printf("Setpoint %d: %d, %d, %d\n", i, profile.getSetpoint(i).time, profile.getSetpoint(i).temp, profile.getSetpoint(i).fanSpeed);
  }

  DEBUG_PRINTLN("Setup complete");
  // roasterState = 1;
}

void loop()
{
  if (tickTimer.isReady())
  {
    myNex.NextionListen();
    heaterRelay.tick();
    tickTimer.reset();
    ws.cleanupClients();
  }

  if (checkTempTimer.isReady())
  {
    currentTemp = thermocouple.readFarenheit();
    DEBUG_PRINT("Temp: ");
    DEBUG_PRINTLN(currentTemp);
    checkTempTimer.reset();
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
      profile.startProfile((int)currentTemp);

      DEBUG_PRINT("Roast start time: ");
      DEBUG_PRINTLN(roastStartTime);

      // digitalWrite(HEATER, HIGH);
      digitalWrite(FAN, HIGH);
      delay(500);
      heaterRelay.setPWM(120);
      ws.textAll("{ \"pushMessage\": \"startRoasting\" }");

      // EEPROM.put(0, setpoints);
      profile.saveProfileToEEPROM();
      break;

    case 2: // roasting
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      myNex.writeNum("globals.nextSetTempNum.val", setpointTemp);
      // myNex.writeNum("globals.setpointFanSpeed.val", round(setpointFanSpeed * 100)); //TODO change to globals.setpointFanSpeed.val in Nextion

      setpointProgress = profile.getProfileProgress();

      myNex.writeNum("globals.setpointProg.val", setpointProgress);

      setpointTemp = profile.getTargetTemp();

      DEBUG_PRINT("setpointTemp: ");
      DEBUG_PRINTLN(setpointTemp);

      if (setpointTemp > currentTemp - 10)
      {
        heaterRelay.setPWM(255);
      }
      else if (setpointTemp > currentTemp - 5)
      {
        heaterRelay.setPWM(230);
      }
      else if (setpointTemp > currentTemp - 2)
      {
        heaterRelay.setPWM(210);
      }
      else
      {
        heaterRelay.setPWM(150);
      }

      if (currentTemp >= profile.getFinalTargetTemp())
      {
        heaterRelay.setPWM(0);
        roasterState = 3; // cooling
        myNex.writeStr("page Cooling");
        DEBUG_PRINTLN("Roast complete -> cooling");
      }
      break;

    case 3:
      heaterRelay.setPWM(0);
      myNex.writeNum("globals.currentTempNum.val", (int)currentTemp);
      DEBUG_PRINTLN("Cooling");
      if (currentTemp <= 145)
      {
        DEBUG_PRINTLN("Cooling - stopped");
        digitalWrite(FAN, LOW);
        roasterState = 0; // idle
        ws.textAll("{ \"pushMessage\": \"endRoasting\" }");
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

  // setpoints.temps[1] = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  // if (setpoints.temps[1] == 777777)
  // {
  //   setpoints.temps[1] = myNex.readNumber("ConfigSetpoint.spTemp1.val");
  // }
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

  profile.clearSetpoints();
  profile.addSetpoint(time1 * 1000, temp1, 100);
  profile.addSetpoint(time2 * 1000, temp2, 100);
  profile.addSetpoint(time3 * 1000, temp3, 100);

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
