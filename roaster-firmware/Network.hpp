#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>  // v3.1.7+ with async mode enabled for ESPAsyncWebServer compatibility

// Removed global JsonDocument to prevent heap fragmentation
// Using local allocation in functions instead
String json;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/WebSocket");

extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern double fanTemp;

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

// void OTATick() {
//   ElegantOTA.loop();
// }

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) {
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
    // Allocate buffer with space for null terminator (prevent buffer overflow)
    char* buffer = (char*)malloc(len + 1);
    if (buffer == NULL) {
      Serial.println("WebSocket: Failed to allocate memory");
      return;
    }
    
    memcpy(buffer, data, len);
    buffer[len] = '\0';
    Serial.printf("%s\n", buffer);
    
    // Use local JsonDocument to prevent heap fragmentation
    JsonDocument wsRequestDoc;
    DeserializationError error = deserializeJson(wsRequestDoc, buffer);
    free(buffer);  // Clean up buffer
    
    // Validate JSON parsing
    if (error) {
      DEBUG_PRINTF("WebSocket: Invalid JSON - %s\n", error.c_str());
      return;
    }
    
    // Validate command exists and is a string
    if (!wsRequestDoc["command"].is<const char*>()) {
      DEBUG_PRINTLN("WebSocket: Missing or invalid 'command' field");
      return;
    }
    
    // Whitelist of valid commands
    const char* command = wsRequestDoc["command"];
    if (strcmp(command, "getData") == 0) {
      // Local allocation for response as well
      JsonDocument wsResponseDoc;
      wsResponseDoc["id"] = wsRequestDoc["id"];
      JsonObject temp_data = wsResponseDoc["data"].to<JsonObject>();
      temp_data["bt"] = currentTemp;
      temp_data["st"] = setpointTemp;
      temp_data["fs"] = setpointFanSpeed * 100 / 255;
      temp_data["ft"] = fanTemp;
      String jsonResponse;
      serializeJson(wsResponseDoc, jsonResponse);
      ws.textAll(jsonResponse);
    } else {
      // Unknown command - log and ignore
      DEBUG_PRINTF("WebSocket: Unknown command '%s' - ignoring\n", command);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      DEBUG_PRINTF("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      DEBUG_PRINTF("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_PING:
      // Ping/Pong - connection keepalive
      break;
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String initializeWifi(const WifiCredentials& wifiCredentials) {
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
      Serial.println("Error setting up MDNS responder! Continuing without mDNS...");
      // Graceful degradation - device works without mDNS, user must use IP address
    } else {
      Serial.println("mDNS responder started - device accessible at roaster.local");
    }
  }

  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", "Hi! You've reached roaster.local.");
  });

  // Start ElegantOTA for over-the-air updates
  ElegantOTA.begin(&server);  // Start ElegantOTA in async mode
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

// WiFi connection monitoring and auto-reconnection
// Call this periodically (e.g., every 5-10 seconds) from main loop
void checkWiFiConnection(const WifiCredentials& wifiCredentials) {
#ifdef ARDUINO_ARCH_ESP32
  static unsigned long lastCheck = 0;
  static int reconnectAttempts = 0;
  const unsigned long CHECK_INTERVAL = 10000;  // Check every 10 seconds
  const int MAX_RECONNECT_ATTEMPTS = 3;        // Try 3 times before giving up
  
  if (millis() - lastCheck < CHECK_INTERVAL) {
    return;  // Not time to check yet
  }
  
  lastCheck = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    DEBUG_PRINTF("WiFi disconnected! Status: %d, Attempt %d/%d\n", WiFi.status(), reconnectAttempts + 1, MAX_RECONNECT_ATTEMPTS);
    
    if (reconnectAttempts < MAX_RECONNECT_ATTEMPTS) {
      reconnectAttempts++;
      WiFi.disconnect();
      delay(1000);
      WiFi.begin(wifiCredentials.ssid, wifiCredentials.password);
      
      // Give it 5 seconds to connect
      int attempts = 0;
      while (WiFi.status() != WL_CONNECTED && attempts < 5) {
        delay(1000);
        attempts++;
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        DEBUG_PRINTF("WiFi reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
        reconnectAttempts = 0;  // Reset counter on successful reconnect
      }
    } else {
      // Max attempts reached, will retry after CHECK_INTERVAL
      reconnectAttempts = 0;
    }
  } else {
    reconnectAttempts = 0;  // Reset counter when connected
  }
#endif
}

#endif // NETWORK_HPP


