#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>

JsonDocument wsRequestDoc;
String json;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/WebSocket");

extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;

// Wifi credentials struct
struct WifiCredentials {
  String ssid;
  String password;
};

unsigned long ota_progress_millis = 0;

void onOTAStart() {
  // Log when OTA has started
  Serial.println("OTA update started!");
  // <Add your own code here>
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    Serial.printf("OTA Progress Current: %u bytes, Final: %u bytes\n", current, final);
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    Serial.println("OTA update finished successfully!");
  } else {
    Serial.println("There was an error during OTA update!");
  }
  // <Add your own code here>
}

void OTATick() {
  ElegantOTA.loop();
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    data[len] = 0;
    Serial.printf("%s\n", (char *)data);
    deserializeJson(wsRequestDoc, data);

    if (wsRequestDoc["command"] == "getData") {
      JsonDocument wsResponseDoc;
      wsResponseDoc["id"] = wsRequestDoc["id"];
      JsonObject temp_data = wsResponseDoc["data"].to<JsonObject>();
      temp_data["bt"] = currentTemp;
      temp_data["st"] = setpointTemp;
      temp_data["fs"] = setpointFanSpeed * 100 / 255;
      String jsonResponse;
      serializeJson(wsResponseDoc, jsonResponse);
      ws.textAll(jsonResponse);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
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

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String initializeWifi(WifiCredentials wifiCredentials) {
  // Connect to Wi-Fi
  WiFi.begin(wifiCredentials.ssid, wifiCredentials.password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 10) {
    delay(1000);
    Serial.println("Connecting to WiFi..");
    attempts++;
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Failed to connect to WiFi");
    return "Failed to connect to WiFi";
  } else {
    // Initialize mDNS
    if (!MDNS.begin("roaster")) {  // Set the hostname to "roaster.local"
      Serial.println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("mDNS responder started");
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! You've reached roaster.local.");
  });

  ElegantOTA.begin(&server);  // Start ElegantOTA
  // ElegantOTA callbacks
  ElegantOTA.onStart(onOTAStart);
  ElegantOTA.onProgress(onOTAProgress);
  ElegantOTA.onEnd(onOTAEnd);

  // Start server
  server.begin();
  return "roaster.local";
  // return WiFi.localIP().toString();
}

// Send WebSocket message to all connected clients
void sendWsMessage(String message) {
#ifdef ARDUINO_ARCH_ESP32
  if (WiFi.status() == WL_CONNECTED) {
    ws.textAll(message);
  }
#endif
}

// WebSocket cleanup and maintainance
void wsCleanup() {
#ifdef ARDUINO_ARCH_ESP32
  if (WiFi.status() == WL_CONNECTED) {
    ws.cleanupClients();
  }
#endif
}


