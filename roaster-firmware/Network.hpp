#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "Types.hpp"
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>  // v3.1.7+ with async mode enabled for ESPAsyncWebServer compatibility
#include "DebugLog.hpp"
#include "PIDController.hpp"
#include "StepResponseTuner.hpp"
#include "PIDRuntimeController.hpp"
#include "PIDValidation.hpp"
#include "ProfileManager.hpp"    // Profile backend logic
#include "ProfileWebUI.hpp"     // Profile UI HTML/CSS/JS
#include "SystemLinkWebUI.hpp"
#include <vector>

// Removed global JsonDocument to prevent heap fragmentation
// Using local allocation in functions instead
String json;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/WebSocket");

// External variables from main firmware
extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern double fanTemp;
extern double heaterOutputVal;
extern double heaterPidTrimVal;
extern double heaterFeedforwardVal;
extern int setpointProgress;
extern int bdcFanMs;
extern int badReadingCount;
extern double kp;
extern double ki;
extern double kd;
extern RoasterState roasterState;  // Defined in Types.hpp
extern Profiles profile;  // Profile configuration
extern ProfileManager profileManager;
extern Preferences preferences; // NVS preferences from main firmware
extern uint8_t profileBuffer[200];
extern EasyNex myNex;
extern StepResponseTuner stepTuner;
extern PIDRuntimeController pidRuntimeController;
extern PIDValidationSession pidValidation;
extern PIDController heaterPID;
extern bool restartRequested;
extern unsigned long restartAt;

// Helper to update Nextion display with active profile
void plotProfileOnWaveform();
bool startValidationRoast(double finalTargetTemp, uint32_t fanPercent);
void setManualPIDGains(double newKp, double newKi, double newKd);

void updateNextionActiveProfile() {
    String activeId = profileManager.getActiveProfileId();
    LOG_INFOF("updateNextionActiveProfile: Updating for ID %s", activeId.c_str());
    String activeName;
    if (activeId.length() > 0) profileManager.loadProfileMeta(activeId, activeName);
    
    myNex.writeStr("page ProfileActive");
    delay(100);
    
    if (activeName.length() > 0) {
        String displayName = activeName;
        myNex.writeStr("ProfileActive.t1.txt", displayName);
    }
    
    // Update global setpoint variable on Nextion
    uint32_t finalTemp = profile.getFinalTargetTemp();
    myNex.writeNum("globals.setTempNum.val", finalTemp);

    plotProfileOnWaveform();
}

// WifiCredentials struct defined in Types.hpp

unsigned long ota_progress_millis = 0;
bool otaUpdateInProgress = false;

void onOTAStart() {
  // Log when OTA has started
  otaUpdateInProgress = true;
  ota_progress_millis = millis();
  LOG_INFOF("OTA update started: freeHeap=%u, wifiRSSI=%d", ESP.getFreeHeap(), WiFi.RSSI());
}

void onOTAProgress(size_t current, size_t final) {
  // Log every 1 second
  if (millis() - ota_progress_millis > 1000) {
    ota_progress_millis = millis();
    LOG_INFOF("OTA progress: current=%u final=%u freeHeap=%u", current, final, ESP.getFreeHeap());
  }
}

void onOTAEnd(bool success) {
  // Log when OTA has finished
  if (success) {
    LOG_INFOF("OTA update finished successfully: freeHeap=%u", ESP.getFreeHeap());
    restartRequested = true;
    restartAt = millis() + 8000; // Give the HTTP response plenty of time to flush before rebooting.
  } else {
    LOG_ERRORF("OTA update failed: freeHeap=%u", ESP.getFreeHeap());
  }
  otaUpdateInProgress = false;
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
    StaticJsonDocument<1024> wsRequestDoc;
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
      StaticJsonDocument<1024> wsResponseDoc;
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

// State name helper
const char* getStateName(byte state) {
  switch(state) {
    case 0: return "IDLE";
    case 1: return "START_ROAST";
    case 2: return "ROASTING";
    case 3: return "COOLING";
    case 4: return "ERROR";
    case 5: return "CALIBRATING";
    default: return "UNKNOWN";
  }
}

// Serialize system state to JSON
String getSystemStateJSON() {
  DynamicJsonDocument doc(1280);
  
  doc["timestamp"] = millis();
  doc["state"] = getStateName(roasterState);
  doc["uptime"] = millis() / 1000;
  
  JsonObject temps = doc.createNestedObject("temps");
  temps["current"] = round(currentTemp * 10) / 10.0;
  temps["setpoint"] = round(setpointTemp * 10) / 10.0;
  temps["fan"] = round(fanTemp * 10) / 10.0;
  
  JsonObject control = doc.createNestedObject("control");
  control["heater"] = (int)heaterOutputVal;
  control["pidTrim"] = round(heaterPidTrimVal * 10) / 10.0;
  control["feedforward"] = round(heaterFeedforwardVal * 10) / 10.0;
  control["pwmFan"] = (int)setpointFanSpeed;
  control["bdcFan"] = bdcFanMs;
  control["scheduleEnabled"] = pidRuntimeController.isEnabled();
  control["activeBand"] = pidRuntimeController.getActiveBandIndex();
  
  JsonObject profileObj = doc.createNestedObject("profile");
  profileObj["progress"] = setpointProgress;
  profileObj["setpointCount"] = profile.getSetpointCount();
  profileObj["finalTemp"] = (int)profile.getFinalTargetTemp();
  
  JsonObject safety = doc.createNestedObject("safety");
  safety["badReadings"] = badReadingCount;
  
  JsonObject memory = doc.createNestedObject("memory");
  memory["heapFree"] = ESP.getFreeHeap();
  memory["heapSize"] = ESP.getHeapSize();
  
  String output;
  serializeJson(doc, output);
  return output;
}

// Simple sanitization for legacy name-based storage
String sanitizeProfileName(const String& name) {
  String sanitized;
  sanitized.reserve(name.length());
  for (unsigned int i = 0; i < name.length(); i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') {
      sanitized += c;
    } else {
      sanitized += '_';
    }
  }
  return sanitized.length() ? sanitized : String("profile");
}

// Serialize profile to JSON
String getProfileJSON() {
  return profileManager.getProfile(profileManager.getActiveProfileId());
}

// Load profile by name from preferences into active `profile`
bool loadProfileByName(const String& name) {
  // Legacy support removed or redirected
  return false;
}

// Broadcast system state to all WebSocket clients
void broadcastSystemState() {
  String stateJson = getSystemStateJSON();
  ws.textAll(stateJson);
}

// Broadcast logs to all WebSocket clients
void broadcastLogs(int maxEntries = 10) {
  String logsJson = debugLogger.getLogsJSON(maxEntries, true);  // Wrap in {logs: [...]}
  ws.textAll(logsJson);
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String initializeWifi(const WifiCredentials& wifiCredentials) {
  if (wifiCredentials.ssid.length() == 0) {
    Serial.println("No WiFi credentials - skipping WiFi setup");
    return "No WiFi";
  }

  // Connect to Wi-Fi
  WiFi.begin(wifiCredentials.ssid, wifiCredentials.password);
  int attempts = 0;
  // Reduced blocking wait to 3 seconds to allow faster boot
  while (WiFi.status() != WL_CONNECTED && attempts < 3) {
    esp_task_wdt_reset(); // Pet the watchdog
    delay(1000);
    Serial.println("Connecting to WiFi..");
    attempts++;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected yet - continuing boot (will retry in background)");
  } else {
    Serial.println(WiFi.localIP());
  }

  // Initialize mDNS regardless of current connection state (it might connect later)
  if (!MDNS.begin("roaster")) {
    Serial.println("Error setting up MDNS responder!");
  } else {
    Serial.println("mDNS responder started - device accessible at roaster.local");
  }

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Roaster Links</title>
  <style>
    body { font-family: Arial, sans-serif; background: #0d1117; color: #c9d1d9; margin: 0; padding: 24px; }
    .page { max-width: 960px; margin: 0 auto; }
    .topnav { display: flex; flex-wrap: wrap; gap: 10px; margin: 0 auto 20px; padding: 14px; background: #161b22; border: 1px solid #30363d; border-radius: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.2); }
    .topnav a { display: inline-flex; align-items: center; justify-content: center; min-width: 120px; padding: 10px 14px; border-radius: 999px; background: #21262d; color: #c9d1d9; text-decoration: none; font-weight: 600; }
    .topnav a.active { background: linear-gradient(135deg, #1f6feb, #58a6ff); color: #fff; }
    .card { max-width: 420px; margin: 0 auto; background: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 24px; box-shadow: 0 8px 24px rgba(0,0,0,0.25); }
    h1 { margin-top: 0; font-size: 24px; color: #fff; }
    p { color: #8b949e; }
    a { display: block; margin: 12px 0; padding: 12px 16px; border-radius: 8px; text-decoration: none; color: #fff; font-weight: 600; text-align: center; background: linear-gradient(135deg, #1f6feb, #58a6ff); }
    a.secondary { background: linear-gradient(135deg, #3fb950, #2ea043); }
    a.tertiary { background: linear-gradient(135deg, #f0883e, #f85149); }
  </style>
</head>
<body>
  <div class="page">
    <nav class="topnav">
      <a class="active" href="/">Home</a>
      <a href="/console">Console</a>
      <a href="/profile">Profiles</a>
      <a href="/pid">PID</a>
      <a href="/update">Update</a>
      <a href="/systemlink">SystemLink</a>
    </nav>
    <div class="card">
      <h1>Roaster Control</h1>
      <p>Select a tool:</p>
      <a href="/console">Debug Console</a>
      <a class="secondary" href="/profile">Profile Editor</a>
      <a class="tertiary" href="/pid">PID Tuning</a>
      <a class="secondary" href="/update">Firmware Update</a>
      <a href="/systemlink">SystemLink</a>
    </div>
  </div>
</body>
</html>
)rawliteral");
  });

  // API endpoint: System state
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("API: /api/state requested");
    String json = getSystemStateJSON();
    request->send(200, "application/json", json);
  });

  // =============================================================================
  // PROFILE API - ID-based RESTful CRUD Operations
  // =============================================================================

  // GET /api/profiles - list summaries
  server.on("/api/profiles", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("GET /api/profiles");
    String result = profileManager.getProfilesList();
    request->send(200, "application/json", result);
  });

  // POST /api/profiles - create
  server.on("/api/profiles", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        String* body = new String();
        body->reserve(total + 1);
        request->_tempObject = (void*)body;
      }

      String* body = (String*)request->_tempObject;
      if (!body) {
        LOG_ERROR("POST /api/profiles: body allocation failed");
        request->send(500, "application/json", "{\"error\":\"internal_error\"}");
        return;
      }

      for (size_t i = 0; i < len; i++) *body += (char)data[i];
      if (index + len < total) { 
        esp_task_wdt_reset(); // Pet watchdog during large uploads
        yield(); 
        return; 
      }

      // Guard against oversized payloads that can destabilize heap
      if (body->length() > 4096) {
        LOG_WARNF("POST /api/profiles: payload too large (%d)", (int)body->length());
        delete body;
        request->send(413, "application/json", "{\"error\":\"payload_too_large\"}");
        return;
      }

      LOG_DEBUG("POST /api/profiles: Calling saveProfile...");
      ProfileOperationResult result = profileManager.saveProfile(*body);
      LOG_DEBUG("POST /api/profiles: saveProfile returned");
      delete body;
      request->_tempObject = nullptr; // Prevent double-free or access
      LOG_DEBUG("POST /api/profiles: body deleted");
      
      if (result.success) {
          LOG_DEBUG("POST /api/profiles: sending success response");
          String out = "{\"ok\":true,\"id\":\"" + result.id + "\"}";
          request->send(201, "application/json", out);
      } else {
          LOG_DEBUG("POST /api/profiles: sending error response");
          String out = "{\"ok\":false,\"error\":\"" + result.error + "\"}";
          request->send(400, "application/json", out);
      }
      LOG_DEBUG("POST /api/profiles: response sent");
    }
  );

  // GET /api/profile/:id
  server.on("/api/profile/*", HTTP_GET, [](AsyncWebServerRequest *request) {
    String path = request->url();
    String id = path.substring(String("/api/profile/").length());
    if (id.length() == 0) {
      request->send(400, "application/json", "{\"error\":\"missing_id\"}");
      return;
    }
    LOG_DEBUGF("GET /api/profile/%s", id.c_str());
    
    String result = profileManager.getProfile(id);
    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, result);
    
    if (err) {
        request->send(500, "application/json", "{\"error\":\"json_error\"}");
    } else if (doc.containsKey("error")) {
        String error = doc["error"].as<String>();
        request->send((error == "not_found") ? 404 : 400, "application/json", result);
    } else {
        request->send(200, "application/json", result);
    }
  });

  // POST /api/profile/:id/activate
  server.on("/api/profile/*", HTTP_POST, [](AsyncWebServerRequest *request) {
    String path = request->url();
    if (!path.endsWith("/activate")) return;  // Only handle activate here

    int start = String("/api/profile/").length();
    int end = path.lastIndexOf("/activate");
    String id = path.substring(start, end);
    if (id.length() == 0) {
      request->send(400, "application/json", "{\"error\":\"missing_id\"}");
      return;
    }
    LOG_DEBUGF("POST /api/profile/%s/activate", id.c_str());
    
    bool success = profileManager.activateProfile(id);
    if (success) {
        updateNextionActiveProfile();
        request->send(200, "application/json", "{\"ok\":true}");
    } else {
        request->send(404, "application/json", "{\"ok\":false,\"error\":\"not_found\"}");
    }
  });

  // PUT /api/profile/:id
  server.on("/api/profile/*", HTTP_PUT,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      String path = request->url();
      if (path.endsWith("/activate")) return;  // handled in POST

      String id = path.substring(String("/api/profile/").length());
      if (id.length() == 0) {
        request->send(400, "application/json", "{\"error\":\"missing_id\"}");
        return;
      }

      if (index == 0) {
        String* body = new String();
        body->reserve(total + 1);
        request->_tempObject = (void*)body;
      }

      String* body = (String*)request->_tempObject;
      if (!body) {
        LOG_ERROR("PUT /api/profile/:id: body allocation failed");
        request->send(500, "application/json", "{\"error\":\"internal_error\"}");
        return;
      }

      for (size_t i = 0; i < len; i++) *body += (char)data[i];
      if (index + len < total) { yield(); return; }

      if (body->length() > 4096) {
        LOG_WARNF("PUT /api/profile/%s: payload too large (%d)", id.c_str(), (int)body->length());
        delete body;
        request->_tempObject = nullptr;
        request->send(413, "application/json", "{\"error\":\"payload_too_large\"}");
        return;
      }

      LOG_DEBUGF("PUT /api/profile/%s: updating profile", id.c_str());
      
      DynamicJsonDocument doc(4096);
      DeserializationError err = deserializeJson(doc, *body);
      if (err) {
          delete body;
          request->_tempObject = nullptr;
          request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
          return;
      }
      
      doc["id"] = id; // Force ID from URL
      String updatedBody;
      serializeJson(doc, updatedBody);
      delete body;
      request->_tempObject = nullptr;

      ProfileOperationResult result = profileManager.saveProfile(updatedBody, id);
      
      if (result.success) {
          String out = "{\"ok\":true,\"id\":\"" + result.id + "\"}";
          request->send(200, "application/json", out);
      } else {
          String out = "{\"ok\":false,\"error\":\"" + result.error + "\"}";
          request->send(400, "application/json", out);
      }
    }
  );

  // DELETE /api/profile/:id
  server.on("/api/profile/*", HTTP_DELETE, [](AsyncWebServerRequest *request) {
    String path = request->url();
    if (path.endsWith("/activate")) return;  // Not applicable
    String id = path.substring(String("/api/profile/").length());
    if (id.length() == 0) {
      request->send(400, "application/json", "{\"error\":\"missing_id\"}");
      return;
    }
    LOG_DEBUGF("DELETE /api/profile/%s", id.c_str());
    
    ProfileOperationResult result = profileManager.deleteProfile(id);
    
    if (result.success) {
        request->send(200, "application/json", "{\"ok\":true}");
    } else {
        DynamicJsonDocument resDoc(256);
        resDoc["ok"] = false;
        resDoc["error"] = result.error;
        String out;
        serializeJson(resDoc, out);
        int code = (result.error == "cannot_delete_active") ? 409 : 404;
        request->send(code, "application/json", out);
    }
  });

  // =============================================================================
  // OTHER API ENDPOINTS
  // =============================================================================

  // API endpoint: Debug logs
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("API: /api/logs requested");
    int maxEntries = 50;
    if (request->hasParam("max")) {
      maxEntries = request->getParam("max")->value().toInt();
      maxEntries = constrain(maxEntries, 1, 100);
    }
    String json = debugLogger.getLogsJSON(maxEntries, true);  // Wrap in {logs: [...]}
    request->send(200, "application/json", json);
  });

  server.on("/api/systemlink", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "application/json", getSystemLinkConfigJSON());
  });

  server.on("/api/systemlink", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        String* body = new String();
        body->reserve(total + 1);
        request->_tempObject = (void*)body;
      }

      String* body = (String*)request->_tempObject;
      if (!body) {
        request->send(500, "application/json", "{\"error\":\"internal_error\"}");
        return;
      }

      for (size_t i = 0; i < len; i++) *body += (char)data[i];
      if (index + len < total) { yield(); return; }

      String error;
      bool ok = updateSystemLinkConfigFromJSON(*body, error);
      delete body;
      request->_tempObject = nullptr;

      if (!ok) {
        request->send(400, "application/json", String("{\"error\":\"") + error + "\"}");
        return;
      }

      request->send(200, "application/json", getSystemLinkConfigJSON());
    });

  // Register more specific PID validation routes before /api/pid because
  // ESPAsyncWebServer matches the shorter prefix route first.
  server.on("/api/calibrate-pid/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<4096> doc;

    doc["running"] = stepTuner.isRunning();
    doc["complete"] = stepTuner.isComplete();
    doc["phase"] = stepTuner.getPhaseName();
    doc["progressPercent"] = stepTuner.getProgressPercent();
      doc["sampleCount"] = stepTuner.getSampleCount();
      doc["lastError"] = stepTuner.getLastError();
      doc["method"] = "step_response";

      StepResponseTuner::Summary ss = stepTuner.getSummary();
      JsonObject model = doc.createNestedObject("model");
      model["valid"] = ss.valid;
      model["passed"] = ss.passed;
      model["targetTemp"] = ss.targetTemp;
      model["ambientTemp"] = ss.ambientTemp;
      model["recommendedKp"] = ss.recommendedKp;
      model["recommendedKi"] = ss.recommendedKi;
      model["recommendedKd"] = ss.recommendedKd;
      model["meanFitRmse"] = ss.meanFitRmse;
      model["worstFitRmse"] = ss.worstFitRmse;
      model["maxTemp"] = ss.maxTemp;
      model["validBandCount"] = ss.validBandCount;
      model["totalSamples"] = ss.totalSamples;
      model["completedBands"] = ss.completedBands;
      model["totalBands"] = ss.totalBands;
      model["tauCFactor"] = ss.tauCFactor;
      model["currentSetpoint"] = ss.currentSetpoint;

      // Band details with FOPDT models
      JsonArray bands = model.createNestedArray("bands");
      for (uint8_t i = 0; i < ss.totalBands && i < StepResponseTuner::MAX_BANDS; i++) {
        const StepResponseTuner::BandResult &br = ss.bands[i];
        JsonObject item = bands.createNestedObject();
        item["index"] = i + 1;
        item["valid"] = br.valid;
        item["targetTemp"] = br.targetTemp;
        item["minTemp"] = br.minTemp;
        item["maxTemp"] = br.maxTemp;
        item["sampleCount"] = br.sampleCount;
        item["kp"] = br.kp;
        item["ki"] = br.ki;
        item["kd"] = br.kd;
        if (br.model.valid) {
          item["processGain"] = br.model.processGain;
          item["timeConstant"] = br.model.timeConstant;
          item["deadTime"] = br.model.deadTime;
          item["fitRmse"] = br.model.fitRmse;
          item["baselineTemp"] = br.model.baselineTemp;
          item["finalTemp"] = br.model.finalTemp;
        }
      }

      // Backward-compatible cycle array (maps bands to cycles)
      JsonArray cycles = model.createNestedArray("cycles");
      for (uint8_t i = 0; i < ss.totalBands && i < StepResponseTuner::MAX_BANDS; i++) {
        const StepResponseTuner::BandResult &br = ss.bands[i];
        JsonObject item = cycles.createNestedObject();
        item["index"] = i + 1;
        item["valid"] = br.valid;
        item["passed"] = br.valid;
        item["modelValid"] = br.model.valid;
        item["processGain"] = br.model.processGain;
        item["timeConstant"] = br.model.timeConstant;
        item["deadTime"] = br.model.deadTime;
        item["fitRmse"] = br.model.fitRmse;
        item["sampleCount"] = br.sampleCount;
        item["kp"] = br.kp;
        item["ki"] = br.ki;
        item["kd"] = br.kd;
      }

      if (stepTuner.isComplete()) {
        model["kp"] = kp;
        model["ki"] = ki;
        model["kd"] = kd;
      }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  server.on("/api/pid/validate/status", HTTP_GET, [](AsyncWebServerRequest *request) {
    PIDValidationSession::Summary summary = pidValidation.getSummary();
    StaticJsonDocument<768> doc;
    doc["active"] = summary.active;
    doc["complete"] = summary.complete;
    doc["passed"] = summary.passed;
    doc["cancelled"] = summary.cancelled;
    doc["startTemp"] = summary.startTemp;
    doc["finalTargetTemp"] = summary.finalTargetTemp;
    doc["durationSeconds"] = summary.durationSeconds;
    doc["meanAbsError"] = summary.meanAbsError;
    doc["rmse"] = summary.rmse;
    doc["maxAbsError"] = summary.maxAbsError;
    doc["holdMeanAbsError"] = summary.holdMeanAbsError;
    doc["holdMaxAbsError"] = summary.holdMaxAbsError;
    doc["withinOneDegreePercent"] = summary.withinOneDegreePercent;
    doc["withinTwoDegreesPercent"] = summary.withinTwoDegreesPercent;
    doc["sampleCount"] = summary.sampleCount;
    doc["holdSampleCount"] = summary.holdSampleCount;
    doc["lastError"] = summary.lastError;
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // API endpoint: Start PID Calibration
  server.on("/api/calibrate-pid", HTTP_POST, [](AsyncWebServerRequest *request) {
    LOG_INFO("API: /api/calibrate-pid requested");
    
    if (roasterState != IDLE) {
        request->send(400, "application/json", "{\"error\":\"Roaster must be IDLE to start calibration\"}");
        return;
    }

    if (currentTemp > 140.0) {
      request->send(400, "application/json", "{\"error\":\"Roaster must be below 140F to start tuning\"}");
      return;
    }
    
    int fanSpeed = 255;
    if (request->hasParam("fan")) {
        fanSpeed = request->getParam("fan")->value().toInt();
    }

    setpointFanSpeed = constrain(fanSpeed, 80, 255);

    double tauCFactor = 0.5;
    if (request->hasParam("tau_c")) {
      tauCFactor = request->getParam("tau_c")->value().toFloat();
    }
    stepTuner.start(currentTemp, kp, ki, kd, setpointFanSpeed, tauCFactor);
    if (!stepTuner.isRunning()) {
      String error = String("{\"error\":\"") + stepTuner.getLastError() + "\"}";
      request->send(400, "application/json", error);
      return;
    }
    roasterState = CALIBRATING;
    char msg[192];
    snprintf(msg, sizeof(msg), "{\"status\":\"Step-response tuning started\",\"method\":\"step_response\",\"fan\":%d,\"tau_c_factor\":%.2f}",
             setpointFanSpeed, tauCFactor);
    request->send(200, "application/json", msg);
  });

  server.on("/api/calibrate-pid/cancel", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (roasterState != CALIBRATING) {
      request->send(400, "application/json", "{\"error\":\"No calibration is currently running\"}");
      return;
    }

    stepTuner.cancel();
    request->send(200, "application/json", "{\"ok\":true,\"status\":\"cancel_requested\"}");
  });

  server.on("/api/calibrate-pid/trace", HTTP_GET, [](AsyncWebServerRequest *request) {
    String out = "{";
    out += "\"running\":";

    out += stepTuner.isRunning() ? "true" : "false";
    out += ",\"method\":\"step_response\"";
    out += ",\"phase\":\"";
    out += stepTuner.getPhaseName();
      out += "\",\"bandIndex\":";
      out += String(static_cast<int>(stepTuner.getActiveBandIndex()) + 1);
      out += ",\"samples\":[";

      uint16_t count = stepTuner.getTraceSampleCount();
      for (uint16_t index = 0; index < count; index++) {
        StepResponseTuner::TraceSample sample = stepTuner.getTraceSample(index);
        if (index > 0) out += ',';
        out += "{\"elapsedSeconds\":";
        out += String(sample.elapsedMs / 1000.0f, 1);
        out += ",\"actualTempF\":";
        out += String(sample.actualTempF, 1);
        out += ",\"setpointTempF\":";
        out += String(sample.setpointTempF, 1);
        out += ",\"heaterOutput\":";
        out += String(sample.heaterOutput, 1);
        out += ",\"phaseId\":";
        out += String(sample.phaseId);
        out += '}';
      }
    out += "]}";
    request->send(200, "application/json", out);
  });

  server.on("/api/pid/validate", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (roasterState != IDLE) {
      request->send(400, "application/json", "{\"error\":\"Roaster must be IDLE to start validation\"}");
      return;
    }

    if (currentTemp > 180.0) {
      request->send(400, "application/json", "{\"error\":\"Roaster must be below 180F to start validation\"}");
      return;
    }

    double target = PIDValidationSession::VALIDATION_RAMP_END_TEMP_F;

    int fanPercent = 70;
    if (request->hasParam("fanPercent")) {
      fanPercent = request->getParam("fanPercent")->value().toInt();
    }

    if (!startValidationRoast(target, constrain(fanPercent, 20, 100))) {
      request->send(500, "application/json", "{\"error\":\"failed_to_start_validation\"}");
      return;
    }

    StaticJsonDocument<256> doc;
    doc["ok"] = true;
    doc["target"] = target;
    doc["fanPercent"] = constrain(fanPercent, 20, 100);
    doc["message"] = "Validation roast started";
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // API endpoint: Get current PID values
  server.on("/api/pid", HTTP_GET, [](AsyncWebServerRequest *request) {
    StaticJsonDocument<512> doc;
    doc["kp"] = kp;
    doc["ki"] = ki;
    doc["kd"] = kd;
    doc["scheduleEnabled"] = pidRuntimeController.isEnabled();
    doc["activeBand"] = pidRuntimeController.getActiveBandIndex();
    doc["validBandCount"] = pidRuntimeController.getValidBandCount();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  // API endpoint: Apply new PID values
  server.on("/api/pid", HTTP_POST,
    [](AsyncWebServerRequest *request) {},
    nullptr,
    [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
      if (index == 0) {
        String* body = new String();
        body->reserve(total + 1);
        request->_tempObject = (void*)body;
      }

      String* body = (String*)request->_tempObject;
      if (!body) {
        request->send(500, "application/json", "{\"error\":\"internal_error\"}");
        return;
      }

      for (size_t i = 0; i < len; i++) *body += (char)data[i];
      if (index + len < total) { yield(); return; }

      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, *body);
      delete body;
      request->_tempObject = nullptr;

      if (err) {
        request->send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
      }

      double newKp = doc.containsKey("kp") ? doc["kp"].as<double>() : kp;
      double newKi = doc.containsKey("ki") ? doc["ki"].as<double>() : ki;
      double newKd = doc.containsKey("kd") ? doc["kd"].as<double>() : kd;

      // Bounds check: reject unreasonable PID gains
      if (newKp < 0 || newKp > 100 || newKi < 0 || newKi > 10 || newKd < 0 || newKd > 50) {
        request->send(400, "application/json", "{\"error\":\"pid_gains_out_of_range\",\"limits\":{\"kp\":\"0-100\",\"ki\":\"0-10\",\"kd\":\"0-50\"}}");
        return;
      }

      setManualPIDGains(newKp, newKi, newKd);

      StaticJsonDocument<384> resp;
      resp["ok"] = true;
      resp["kp"] = kp;
      resp["ki"] = ki;
      resp["kd"] = kd;
      resp["scheduleEnabled"] = pidRuntimeController.isEnabled();
      String out;
      serializeJson(resp, out);
      request->send(200, "application/json", out);
    }
  );

  // Debug Console UI
  server.on("/console", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_INFO("Console UI accessed");
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Roaster Debug Console</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
      background: #0d1117;
      color: #c9d1d9;
      padding: 20px;
      line-height: 1.5;
    }
    .topnav {
      display: flex;
      flex-wrap: wrap;
      gap: 10px;
      max-width: 1400px;
      margin: 0 auto 20px;
      padding: 14px;
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 14px;
      box-shadow: 0 8px 24px rgba(0,0,0,0.2);
    }
    .topnav a {
      display: inline-flex;
      align-items: center;
      justify-content: center;
      min-width: 120px;
      padding: 10px 14px;
      border-radius: 999px;
      background: #21262d;
      color: #c9d1d9;
      text-decoration: none;
      font-weight: 600;
    }
    .topnav a.active {
      background: linear-gradient(135deg, #1f6feb, #58a6ff);
      color: #fff;
    }
    .header {
      background: linear-gradient(135deg, #1f6feb 0%, #0969da 100%);
      padding: 24px;
      border-radius: 12px;
      margin-bottom: 24px;
      box-shadow: 0 8px 24px rgba(31, 111, 235, 0.2);
    }
    .header h1 {
      font-size: 28px;
      font-weight: 600;
      margin-bottom: 8px;
      color: #fff;
    }
    .header .subtitle {
      color: rgba(255, 255, 255, 0.8);
      font-size: 14px;
      display: flex;
      align-items: center;
      gap: 16px;
    }
    .status-badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      padding: 4px 12px;
      background: rgba(255, 255, 255, 0.15);
      border-radius: 20px;
      font-size: 12px;
      font-weight: 500;
    }
    .status-dot {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #3fb950;
      animation: pulse 2s ease-in-out infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.5; }
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
      gap: 20px;
      margin-bottom: 20px;
    }
    .card {
      background: #161b22;
      border: 1px solid #30363d;
      border-radius: 8px;
      padding: 20px;
      transition: border-color 0.2s;
    }
    .card:hover {
      border-color: #1f6feb;
    }
    .card-title {
      font-size: 16px;
      font-weight: 600;
      margin-bottom: 16px;
      color: #fff;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .card-title::before {
      content: '';
      width: 4px;
      height: 16px;
      background: #1f6feb;
      border-radius: 2px;
    }
    .metric-row {
      display: flex;
      justify-content: space-between;
      padding: 10px 0;
      border-bottom: 1px solid #21262d;
    }
    .metric-row:last-child {
      border-bottom: none;
    }
    .metric-label {
      color: #8b949e;
      font-size: 14px;
    }
    .metric-value {
      font-weight: 600;
      font-size: 16px;
      color: #58a6ff;
      font-family: 'Courier New', monospace;
    }
    .metric-value.large {
      font-size: 32px;
      color: #3fb950;
    }
    .metric-value.warn {
      color: #f0883e;
    }
    .metric-value.error {
      color: #f85149;
    }
    .gauge-container {
      position: relative;
      width: 100%;
      height: 120px;
      margin: 16px 0;
    }
    .gauge {
      width: 100%;
      height: 100%;
    }
    .gauge-value {
      position: absolute;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
      font-size: 28px;
      font-weight: 700;
      color: #fff;
    }
    .gauge-label {
      text-align: center;
      color: #8b949e;
      font-size: 13px;
      margin-top: 8px;
    }
    .log-container {
      background: #0d1117;
      border: 1px solid #30363d;
      border-radius: 6px;
      padding: 12px;
      max-height: 400px;
      overflow-y: auto;
      font-family: 'Courier New', monospace;
      font-size: 13px;
    }
    .log-entry {
      padding: 6px 8px;
      margin: 2px 0;
      border-radius: 4px;
      display: flex;
      gap: 12px;
      border-left: 3px solid transparent;
    }
    .log-entry.DEBUG { border-left-color: #8b949e; }
    .log-entry.INFO { border-left-color: #58a6ff; }
    .log-entry.WARN { border-left-color: #f0883e; }
    .log-entry.ERROR { border-left-color: #f85149; background: rgba(248, 81, 73, 0.1); }
    .log-time {
      color: #6e7681;
      min-width: 80px;
    }
    .log-level {
      min-width: 50px;
      font-weight: 600;
    }
    .log-level.DEBUG { color: #8b949e; }
    .log-level.INFO { color: #58a6ff; }
    .log-level.WARN { color: #f0883e; }
    .log-level.ERROR { color: #f85149; }
    .log-message {
      color: #c9d1d9;
      flex: 1;
    }
    .controls {
      display: flex;
      gap: 12px;
      margin-bottom: 12px;
      flex-wrap: wrap;
    }
    .btn {
      padding: 8px 16px;
      background: #21262d;
      border: 1px solid #30363d;
      color: #c9d1d9;
      border-radius: 6px;
      cursor: pointer;
      font-size: 14px;
      transition: all 0.2s;
    }
    .btn:hover {
      background: #30363d;
      border-color: #1f6feb;
    }
    .btn.active {
      background: #1f6feb;
      border-color: #1f6feb;
      color: #fff;
    }
    .full-width {
      grid-column: 1 / -1;
    }
    @media (max-width: 768px) {
      .grid {
        grid-template-columns: 1fr;
      }
      .header h1 {
        font-size: 22px;
      }
    }
  </style>
</head>
<body>
  <nav class="topnav">
    <a href="/">Home</a>
    <a class="active" href="/console">Console</a>
    <a href="/profile">Profiles</a>
    <a href="/pid">PID</a>
    <a href="/update">Update</a>
    <a href="/systemlink">SystemLink</a>
  </nav>
  <div class="header">
    <h1>☕ Coffee Roaster Debug Console</h1>
    <div class="subtitle">
      <span class="status-badge">
        <span class="status-dot"></span>
        <span id="stateText">Loading...</span>
      </span>
      <span id="uptime">Uptime: --</span>
      <span>Last Update: <span id="lastUpdate">--</span></span>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="card-title">🌡️ Bean Temperature</div>
      <div style="display: flex; justify-content: center; margin: 20px 0;">
        <svg id="tempGauge" width="200" height="200" viewBox="0 0 200 200">
          <circle cx="100" cy="100" r="90" fill="none" stroke="#21262d" stroke-width="12"/>
          <path id="tempArc" fill="none" stroke="url(#tempGradient)" stroke-width="12" stroke-linecap="round"/>
          <text x="100" y="95" text-anchor="middle" font-size="36" font-weight="bold" fill="#fff" id="tempValue">--</text>
          <text x="100" y="115" text-anchor="middle" font-size="16" fill="#8b949e">°F</text>
          <text x="100" y="135" text-anchor="middle" font-size="13" fill="#58a6ff" id="tempTarget">Target: --</text>
          <defs>
            <linearGradient id="tempGradient" x1="0%" y1="0%" x2="100%" y2="0%">
              <stop offset="0%" style="stop-color:#58a6ff;stop-opacity:1" />
              <stop offset="50%" style="stop-color:#3fb950;stop-opacity:1" />
              <stop offset="100%" style="stop-color:#f85149;stop-opacity:1" />
            </linearGradient>
          </defs>
        </svg>
      </div>
      <div class="metric-row">
        <span class="metric-label">Fan Temp</span>
        <span class="metric-value" id="fanTemp">--°F</span>
      </div>
    </div>

    <div class="card">
      <div class="card-title">⚙️ Control Output</div>
      <div style="margin: 20px 0;">
        <div style="margin-bottom: 20px;">
          <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
            <span class="metric-label">Heater</span>
            <span class="metric-value" id="heaterOutput">--%</span>
          </div>
          <div style="background: #21262d; border-radius: 8px; height: 24px; overflow: hidden;">
            <div id="heaterBar" style="height: 100%; background: linear-gradient(90deg, #f0883e, #f85149); width: 0%; transition: width 0.3s;"></div>
          </div>
        </div>
        <div style="margin-bottom: 20px;">
          <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
            <span class="metric-label">PWM Fan</span>
            <span class="metric-value" id="pwmFan">--</span>
          </div>
          <div style="background: #21262d; border-radius: 8px; height: 24px; overflow: hidden;">
            <div id="fanBar" style="height: 100%; background: linear-gradient(90deg, #58a6ff, #3fb950); width: 0%; transition: width 0.3s;"></div>
          </div>
        </div>
        <div class="metric-row">
          <span class="metric-label">BDC Fan</span>
          <span class="metric-value" id="bdcFan">-- µs</span>
        </div>
      </div>
    </div>

    <div class="card">
      <div class="card-title">📊 Roast Profile</div>
      <div style="margin: 20px 0;">
        <div style="display: flex; justify-content: space-between; margin-bottom: 8px;">
          <span class="metric-label">Progress</span>
          <span class="metric-value" id="profileProgress">--%</span>
        </div>
        <div style="background: #21262d; border-radius: 8px; height: 32px; overflow: hidden; margin-bottom: 20px;">
          <div id="progressBar" style="height: 100%; background: linear-gradient(90deg, #1f6feb, #58a6ff); width: 0%; transition: width 0.5s; display: flex; align-items: center; justify-content: center; font-size: 14px; font-weight: 600; color: #fff;"></div>
        </div>
      </div>
      <div class="metric-row">
        <span class="metric-label">Setpoints</span>
        <span class="metric-value" id="setpointCount">--</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Final Target</span>
        <span class="metric-value" id="finalTemp">--°F</span>
      </div>
    </div>

    <div class="card">
      <div class="card-title">🛡️ Safety & System</div>
      <div class="metric-row">
        <span class="metric-label">Bad Readings</span>
        <span class="metric-value" id="badReadings">--</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Free Heap</span>
        <span class="metric-value" id="heapFree">-- KB</span>
      </div>
      <div class="metric-row">
        <span class="metric-label">Heap Usage</span>
        <span class="metric-value" id="heapUsage">--%</span>
      </div>
    </div>

    <div class="card full-width">
      <div class="card-title">� Live Temperature Chart</div>
      <canvas id="tempChart" width="800" height="200" style="width: 100%; height: 200px;"></canvas>
    </div>

    <div class="card full-width">
      <div class="card-title">�📝 Debug Logs</div>
      <div class="controls">
        <button class="btn active" onclick="filterLogs('ALL')">All</button>
        <button class="btn" onclick="filterLogs('ERROR')">Errors</button>
        <button class="btn" onclick="filterLogs('WARN')">Warnings</button>
        <button class="btn" onclick="filterLogs('INFO')">Info</button>
        <button class="btn" onclick="filterLogs('DEBUG')">Debug</button>
        <button class="btn" onclick="clearLogs()">Clear Display</button>
        <button class="btn" onclick="toggleAutoScroll()">Auto-scroll: <span id="autoScrollState">ON</span></button>
      </div>
      <div class="log-container" id="logContainer">
        <div class="log-entry INFO">
          <span class="log-time">00:00:00</span>
          <span class="log-level INFO">INFO</span>
          <span class="log-message">Console loaded. Fetching data...</span>
        </div>
      </div>
    </div>
  </div>

  <script>
    let autoScroll = true;
    let logFilter = 'ALL';
    let wsConnected = false;

    function formatUptime(seconds) {
      const h = Math.floor(seconds / 3600);
      const m = Math.floor((seconds % 3600) / 60);
      const s = seconds % 60;
      return `${h}h ${m}m ${s}s`;
    }

    function updateSystemState(data) {
      document.getElementById('stateText').textContent = data.state || '--';
      document.getElementById('uptime').textContent = 'Uptime: ' + formatUptime(data.uptime || 0);
      document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();

      // Update temperature gauge
      const currentTemp = data.temps?.current || 0;
      const setpointTemp = data.temps?.setpoint || 0;
      document.getElementById('tempValue').textContent = Math.round(currentTemp);
      document.getElementById('tempTarget').textContent = 'Target: ' + Math.round(setpointTemp);
      updateGauge(currentTemp, 500); // Max temp 500°F
      
      document.getElementById('fanTemp').textContent = (data.temps?.fan || '--') + '°F';

      // Update control bars
      const heaterPct = Math.round((data.control?.heater || 0) / 255 * 100);
      document.getElementById('heaterOutput').textContent = heaterPct + '%';
      document.getElementById('heaterBar').style.width = heaterPct + '%';
      
      const pwmFan = data.control?.pwmFan || 0;
      const fanPct = Math.round(pwmFan / 255 * 100);
      document.getElementById('pwmFan').textContent = pwmFan;
      document.getElementById('fanBar').style.width = fanPct + '%';
      
      document.getElementById('bdcFan').textContent = (data.control?.bdcFan || '--') + ' µs';

      // Update profile progress
      const progress = data.profile?.progress || 0;
      document.getElementById('profileProgress').textContent = progress + '%';
      const progressBar = document.getElementById('progressBar');
      progressBar.style.width = progress + '%';
      progressBar.textContent = progress > 10 ? progress + '%' : '';
      
      document.getElementById('setpointCount').textContent = data.profile?.setpointCount || '--';
      document.getElementById('finalTemp').textContent = (data.profile?.finalTemp || '--') + '°F';

      const badReadings = data.safety?.badReadings || 0;
      const badReadingsEl = document.getElementById('badReadings');
      badReadingsEl.textContent = badReadings;
      badReadingsEl.className = 'metric-value' + (badReadings > 5 ? ' error' : badReadings > 2 ? ' warn' : '');

      const heapFree = Math.round((data.memory?.heapFree || 0) / 1024);
      const heapSize = data.memory?.heapSize || 1;
      const heapUsage = Math.round((1 - (data.memory?.heapFree || 0) / heapSize) * 100);
      document.getElementById('heapFree').textContent = heapFree + ' KB';
      const heapUsageEl = document.getElementById('heapUsage');
      heapUsageEl.textContent = heapUsage + '%';
      heapUsageEl.className = 'metric-value' + (heapUsage > 80 ? ' error' : heapUsage > 60 ? ' warn' : '');
      
      // Update temperature chart
      updateChart(currentTemp, setpointTemp);
    }

    function updateLogs(logs) {
      const container = document.getElementById('logContainer');
      container.innerHTML = '';
      
      logs.forEach(log => {
        if (logFilter !== 'ALL' && log.level !== logFilter) return;
        
        const entry = document.createElement('div');
        entry.className = 'log-entry ' + log.level;
        
        const time = new Date(log.timestamp).toLocaleTimeString();
        entry.innerHTML = `
          <span class="log-time">${time}</span>
          <span class="log-level ${log.level}">${log.level}</span>
          <span class="log-message">${log.message}</span>
        `;
        
        container.appendChild(entry);
      });

      if (autoScroll) {
        container.scrollTop = container.scrollHeight;
      }
    }

    function filterLogs(level) {
      logFilter = level;
      document.querySelectorAll('.controls .btn').forEach(btn => {
        btn.classList.toggle('active', btn.textContent.startsWith(level) || (level === 'ALL' && btn.textContent === 'All'));
      });
      fetchLogs();
    }

    function clearLogs() {
      document.getElementById('logContainer').innerHTML = '<div class="log-entry INFO"><span class="log-time">' + 
        new Date().toLocaleTimeString() + '</span><span class="log-level INFO">INFO</span>' +
        '<span class="log-message">Display cleared (logs still in memory)</span></div>';
    }

    function toggleAutoScroll() {
      autoScroll = !autoScroll;
      document.getElementById('autoScrollState').textContent = autoScroll ? 'ON' : 'OFF';
    }

    // Temperature gauge rendering
    function updateGauge(temp, maxTemp) {
      const percentage = Math.min(temp / maxTemp, 1);
      const angle = percentage * 270 - 135; // -135° to 135° (270° arc)
      const radians = (angle * Math.PI) / 180;
      
      const centerX = 100;
      const centerY = 100;
      const radius = 90;
      
      const startX = centerX + radius * Math.cos(-135 * Math.PI / 180);
      const startY = centerY + radius * Math.sin(-135 * Math.PI / 180);
      const endX = centerX + radius * Math.cos(radians);
      const endY = centerY + radius * Math.sin(radians);
      
      const largeArc = percentage > 0.75 ? 1 : 0;
      
      const path = `M ${startX} ${startY} A ${radius} ${radius} 0 ${largeArc} 1 ${endX} ${endY}`;
      document.getElementById('tempArc').setAttribute('d', path);
    }

    // Temperature chart
    const chartData = {
      temps: [],
      setpoints: [],
      times: [],
      maxPoints: 120 // 2 minutes at 1 sample/second
    };

    function updateChart(temp, setpoint) {
      const now = new Date();
      chartData.temps.push(temp);
      chartData.setpoints.push(setpoint);
      chartData.times.push(now);
      
      // Keep only last maxPoints
      if (chartData.temps.length > chartData.maxPoints) {
        chartData.temps.shift();
        chartData.setpoints.shift();
        chartData.times.shift();
      }
      
      drawChart();
    }

    function drawChart() {
      const canvas = document.getElementById('tempChart');
      const ctx = canvas.getContext('2d');
      const width = canvas.width;
      const height = canvas.height;
      
      // Clear canvas
      ctx.fillStyle = '#0d1117';
      ctx.fillRect(0, 0, width, height);
      
      if (chartData.temps.length < 2) return;
      
      // Find min/max for scaling
      const allValues = [...chartData.temps, ...chartData.setpoints];
      const minTemp = Math.min(...allValues) - 10;
      const maxTemp = Math.max(...allValues) + 10;
      const tempRange = maxTemp - minTemp;
      
      // Draw grid
      ctx.strokeStyle = '#21262d';
      ctx.lineWidth = 1;
      for (let i = 0; i <= 4; i++) {
        const y = (height / 4) * i;
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(width, y);
        ctx.stroke();
      }
      
      // Draw setpoint line
      ctx.strokeStyle = '#58a6ff';
      ctx.lineWidth = 2;
      ctx.setLineDash([5, 5]);
      ctx.beginPath();
      chartData.setpoints.forEach((temp, i) => {
        const x = (i / (chartData.maxPoints - 1)) * width;
        const y = height - ((temp - minTemp) / tempRange) * height;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
      ctx.setLineDash([]);
      
      // Draw temperature line
      ctx.strokeStyle = '#3fb950';
      ctx.lineWidth = 3;
      ctx.beginPath();
      chartData.temps.forEach((temp, i) => {
        const x = (i / (chartData.maxPoints - 1)) * width;
        const y = height - ((temp - minTemp) / tempRange) * height;
        if (i === 0) ctx.moveTo(x, y);
        else ctx.lineTo(x, y);
      });
      ctx.stroke();
      
      // Draw labels
      ctx.fillStyle = '#8b949e';
      ctx.font = '12px monospace';
      ctx.textAlign = 'left';
      ctx.fillText(Math.round(maxTemp) + '°F', 5, 15);
      ctx.fillText(Math.round(minTemp) + '°F', 5, height - 5);
    }

    async function fetchState() {
      try {
        const response = await fetch('/api/state');
        const data = await response.json();
        updateSystemState(data);
      } catch (error) {
        console.error('Error fetching state:', error);
      }
    }

    async function fetchLogs() {
      try {
        const response = await fetch('/api/logs?max=100');
        const data = await response.json();
        updateLogs(data.logs || []);
      } catch (error) {
        console.error('Error fetching logs:', error);
      }
    }

    // WebSocket connection
    let ws = null;
    let reconnectInterval = null;
    let pollingInterval = null;
    let logsPollingInterval = null;

    function connectWebSocket() {
      const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
      const wsUrl = `${protocol}//${window.location.host}/WebSocket`;
      
      ws = new WebSocket(wsUrl);
      
      ws.onopen = () => {
        console.log('WebSocket connected');
        wsConnected = true;
        document.querySelector('.status-dot').style.background = '#3fb950';
        
        // Stop polling when WebSocket is connected
        if (pollingInterval) {
          clearInterval(pollingInterval);
          pollingInterval = null;
        }
        if (logsPollingInterval) {
          clearInterval(logsPollingInterval);
          logsPollingInterval = null;
        }
        
        // Clear reconnect timer
        if (reconnectInterval) {
          clearTimeout(reconnectInterval);
          reconnectInterval = null;
        }
      };
      
      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          
          // Check if it's state data or logs data
          if (data.state !== undefined) {
            updateSystemState(data);
          } else if (data.logs !== undefined) {
            updateLogs(data.logs);
          }
        } catch (error) {
          console.error('Error parsing WebSocket message:', error);
        }
      };
      
      ws.onerror = (error) => {
        console.error('WebSocket error:', error);
      };
      
      ws.onclose = () => {
        console.log('WebSocket disconnected');
        wsConnected = false;
        document.querySelector('.status-dot').style.background = '#f0883e';
        
        // Fall back to polling
        if (!pollingInterval) {
          pollingInterval = setInterval(fetchState, 1000);
        }
        if (!logsPollingInterval) {
          logsPollingInterval = setInterval(fetchLogs, 2000);
        }
        
        // Try to reconnect after 5 seconds
        if (!reconnectInterval) {
          reconnectInterval = setTimeout(connectWebSocket, 5000);
        }
      };
    }

    // Initial load
    fetchState();
    fetchLogs();

    // Try to connect via WebSocket
    connectWebSocket();

    // Start with polling as fallback (will be cleared if WebSocket connects)
    pollingInterval = setInterval(fetchState, 1000);
    logsPollingInterval = setInterval(fetchLogs, 2000);
  </script>
</body>
</html>
)rawliteral");
  });

  // PID Tuning UI
  server.on("/pid", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_INFO("PID UI accessed");
    request->send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>PID Workflow</title>
  <style>
    body { font-family: Arial, sans-serif; background: #0d1117; color: #c9d1d9; padding: 20px; }
    .topnav { display: flex; flex-wrap: wrap; gap: 10px; max-width: 920px; margin: 0 auto 16px; padding: 14px; background: #161b22; border: 1px solid #30363d; border-radius: 14px; box-shadow: 0 8px 24px rgba(0,0,0,0.2); }
    .topnav a { display: inline-flex; align-items: center; justify-content: center; min-width: 120px; padding: 10px 14px; border-radius: 999px; background: #21262d; color: #c9d1d9; text-decoration: none; font-weight: 600; }
    .topnav a.active { background: linear-gradient(135deg, #1f6feb, #58a6ff); color: #fff; }
    .page { max-width: 920px; margin: 0 auto; display: grid; gap: 16px; }
    .card { background: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 20px; box-shadow: 0 8px 24px rgba(0,0,0,0.25); }
    h1, h2 { margin-top: 0; color: #fff; }
    label { display: block; margin: 12px 0 4px; color: #8b949e; }
    input { width: 100%; padding: 10px; border-radius: 8px; border: 1px solid #30363d; background: #0d1117; color: #c9d1d9; }
    button { margin-top: 14px; width: 100%; padding: 12px; border: none; border-radius: 8px; font-weight: 700; cursor: pointer; }
    button:disabled { opacity: 0.45; cursor: not-allowed; }
    .primary { background: linear-gradient(135deg, #1f6feb, #58a6ff); color: #fff; }
    .secondary { background: linear-gradient(135deg, #3fb950, #2ea043); color: #fff; }
    .tertiary { background: linear-gradient(135deg, #f0883e, #f85149); color: #fff; }
    .note { color: #8b949e; font-size: 13px; margin-top: 8px; }
    .row { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
    .two-col { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 16px; }
    .button-row { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: 10px; }
    .status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(180px, 1fr)); gap: 10px; margin-top: 16px; }
    .metric { background: #0d1117; border: 1px solid #30363d; border-radius: 10px; padding: 12px; }
    .metric-label { color: #8b949e; font-size: 12px; text-transform: uppercase; letter-spacing: 0.04em; }
    .metric-value { color: #fff; font-size: 20px; font-weight: 700; margin-top: 6px; }
    .chart { margin-top: 16px; background: #0d1117; border: 1px solid #30363d; border-radius: 10px; padding: 12px; }
    .chart canvas { width: 100%; height: 260px; display: block; }
    .legend { display: flex; gap: 18px; margin-top: 10px; color: #8b949e; font-size: 12px; }
    .legend span { display: inline-flex; align-items: center; gap: 8px; }
    .legend span::before { content: ""; width: 18px; height: 3px; border-radius: 999px; display: inline-block; }
    .legend .actual::before { background: #58a6ff; }
    .legend .setpoint::before { background: #f2cc60; }
    table { width: 100%; border-collapse: collapse; margin-top: 16px; }
    th, td { text-align: left; padding: 10px; border-bottom: 1px solid #30363d; font-size: 14px; }
    th { color: #8b949e; }
    .small { font-size: 12px; color: #8b949e; }
  </style>
</head>
<body>
  <nav class="topnav">
    <a href="/">Home</a>
    <a href="/console">Console</a>
    <a href="/profile">Profiles</a>
    <a class="active" href="/pid">PID</a>
    <a href="/update">Update</a>
    <a href="/systemlink">SystemLink</a>
  </nav>
  <div class="page">
    <div class="card">
      <h1>PID Workflow</h1>
      <p class="note">Apply manual gains directly, or run the step-response PID tuning workflow. The tuner applies open-loop step inputs at 2 temperature bands, fits FOPDT models, and calculates SIMC PID gains.</p>
      <div class="row">
        <div>
          <label for="kp">Kp</label>
          <input id="kp" type="number" step="0.01" />
        </div>
        <div>
          <label for="ki">Ki</label>
          <input id="ki" type="number" step="0.01" />
        </div>
        <div>
          <label for="kd">Kd</label>
          <input id="kd" type="number" step="0.01" />
        </div>
      </div>
      <button class="primary" onclick="applyPid()">Apply PID Values</button>
      <p class="note" id="status">Loading current PID...</p>
    </div>

    <div class="two-col">
      <div class="card">
        <h2>PID Tuning</h2>
        <label for="fan">Calibration Fan PWM (0-255)</label>
        <input id="fan" type="number" value="255" />
        <div id="stepOptions">
          <label for="tauC">&tau;<sub>c</sub> Factor (0.3-3.0, lower = more aggressive)</label>
          <input id="tauC" type="number" min="0.3" max="3.0" step="0.1" value="0.5" />
        </div>
        <div class="button-row">
          <button id="startAutotuneButton" class="secondary" onclick="startAutotune()">Run Step-Response PID Tuning</button>
          <button id="cancelAutotuneButton" class="tertiary" onclick="cancelAutotune()" disabled>Cancel Tuning</button>
        </div>
        <p class="note" id="methodNote">The step-response tuner stabilizes at 2 temperature bands (175, 275F), applies a heater step, records the open-loop response, fits a first-order-plus-dead-time model, and computes SIMC PID gains. Auto-validation runs after cooling.</p>
        <div class="status-grid">
          <div class="metric"><div class="metric-label">Phase</div><div class="metric-value" id="autotunePhase">--</div></div>
          <div class="metric"><div class="metric-label">Progress</div><div class="metric-value" id="autotuneProgress">--</div></div>
          <div class="metric"><div class="metric-label">Setpoint</div><div class="metric-value" id="autotuneSetpoint">--</div></div>
          <div class="metric"><div class="metric-label">Best PID</div><div class="metric-value small" id="autotunePid">--</div></div>
          <div class="metric"><div class="metric-label" id="autotuneBestLabel">Best Cycle</div><div class="metric-value" id="autotuneBest">--</div></div>
          <div class="metric"><div class="metric-label">Outcome</div><div class="metric-value" id="autotuneOutcome">--</div></div>
        </div>
        <div class="note" id="autotuneSummary">No tuning run recorded yet.</div>
        <div class="chart">
          <canvas id="autotuneChart" width="760" height="260"></canvas>
          <div class="legend">
            <span class="actual">Actual Temp</span>
            <span class="setpoint">Setpoint</span>
          </div>
        </div>
        <table>
          <thead id="cycleTableHeader">
            <tr><th>Cycle</th><th>MAE</th><th>Overshoot</th><th>Oscillations</th><th>Score</th><th>PID</th></tr>
          </thead>
          <tbody id="cycleTableBody">
            <tr><td colspan="6" class="small">No data yet.</td></tr>
          </tbody>
        </table>
      </div>
    </div>
  </div>

  <script>
    const chartCanvas = document.getElementById('autotuneChart');
    const chartCtx = chartCanvas.getContext('2d');
    const pidInputIds = ['kp', 'ki', 'kd'];
    let latestTraceSamples = [];
    let pidInputsDirty = false;
    let suppressPidDirty = false;

    function formatNumber(value, digits = 2) {
      return Number.isFinite(value) ? value.toFixed(digits) : '--';
    }

    function bindPidInputs() {
      pidInputIds.forEach(id => {
        const input = document.getElementById(id);
        input.addEventListener('input', () => {
          if (!suppressPidDirty) {
            pidInputsDirty = true;
          }
        });
      });
    }

    function setPidInputs(nextKp, nextKi, nextKd) {
      suppressPidDirty = true;
      document.getElementById('kp').value = Number.isFinite(nextKp) ? Number(nextKp).toFixed(2) : '';
      document.getElementById('ki').value = Number.isFinite(nextKi) ? Number(nextKi).toFixed(3) : '';
      document.getElementById('kd').value = Number.isFinite(nextKd) ? Number(nextKd).toFixed(2) : '';
      suppressPidDirty = false;
      pidInputsDirty = false;
    }

    function drawTracePlaceholder(message) {
      chartCtx.clearRect(0, 0, chartCanvas.width, chartCanvas.height);
      chartCtx.fillStyle = '#0d1117';
      chartCtx.fillRect(0, 0, chartCanvas.width, chartCanvas.height);
      chartCtx.fillStyle = '#8b949e';
      chartCtx.font = '14px Arial';
      chartCtx.textAlign = 'center';
      chartCtx.fillText(message, chartCanvas.width / 2, chartCanvas.height / 2);
    }

    function renderTrace(samples) {
      latestTraceSamples = Array.isArray(samples) ? samples : [];

      const width = chartCanvas.width;
      const height = chartCanvas.height;
      const padLeft = 52;
      const padRight = 18;
      const padTop = 18;
      const padBottom = 34;

      chartCtx.clearRect(0, 0, width, height);
      chartCtx.fillStyle = '#0d1117';
      chartCtx.fillRect(0, 0, width, height);

      if (!latestTraceSamples.length) {
        drawTracePlaceholder('No trace data yet.');
        return;
      }

      const maxTime = latestTraceSamples.reduce((maxValue, sample) => Math.max(maxValue, Number(sample.elapsedSeconds) || 0), 0);
      const temperatureValues = latestTraceSamples.flatMap(sample => [Number(sample.actualTempF), Number(sample.setpointTempF)]).filter(Number.isFinite);

      if (!temperatureValues.length) {
        drawTracePlaceholder('Trace data unavailable.');
        return;
      }

      let minTemp = Math.min(...temperatureValues);
      let maxTemp = Math.max(...temperatureValues);
      minTemp = Math.floor((minTemp - 5) / 5) * 5;
      maxTemp = Math.ceil((maxTemp + 5) / 5) * 5;

      if (maxTemp <= minTemp) {
        maxTemp = minTemp + 10;
      }

      const plotWidth = width - padLeft - padRight;
      const plotHeight = height - padTop - padBottom;
      const xFor = time => padLeft + ((Number(time) || 0) / Math.max(maxTime, 1)) * plotWidth;
      const yFor = temp => padTop + (1 - ((Number(temp) - minTemp) / (maxTemp - minTemp))) * plotHeight;

      chartCtx.strokeStyle = '#21262d';
      chartCtx.lineWidth = 1;
      for (let row = 0; row <= 4; row++) {
        const y = padTop + (plotHeight / 4) * row;
        chartCtx.beginPath();
        chartCtx.moveTo(padLeft, y);
        chartCtx.lineTo(width - padRight, y);
        chartCtx.stroke();
      }

      chartCtx.fillStyle = '#8b949e';
      chartCtx.font = '11px Arial';
      chartCtx.textAlign = 'right';
      for (let row = 0; row <= 4; row++) {
        const ratio = 1 - row / 4;
        const labelTemp = minTemp + (maxTemp - minTemp) * ratio;
        const y = padTop + (plotHeight / 4) * row + 4;
        chartCtx.fillText(`${formatNumber(labelTemp, 0)}F`, padLeft - 8, y);
      }

      chartCtx.textAlign = 'center';
      for (let column = 0; column <= 4; column++) {
        const timeValue = (Math.max(maxTime, 1) / 4) * column;
        const x = padLeft + (plotWidth / 4) * column;
        chartCtx.fillText(`${formatNumber(timeValue, 0)}s`, x, height - 10);
      }

      function drawSeries(color, accessor) {
        chartCtx.beginPath();
        chartCtx.strokeStyle = color;
        chartCtx.lineWidth = 2;
        let started = false;
        latestTraceSamples.forEach(sample => {
          const value = Number(accessor(sample));
          if (!Number.isFinite(value)) {
            return;
          }
          const x = xFor(sample.elapsedSeconds);
          const y = yFor(value);
          if (!started) {
            chartCtx.moveTo(x, y);
            started = true;
          } else {
            chartCtx.lineTo(x, y);
          }
        });
        if (started) {
          chartCtx.stroke();
        }
      }

      drawSeries('#58a6ff', sample => sample.actualTempF);
      drawSeries('#f2cc60', sample => sample.setpointTempF);
    }

    async function loadPid() {
      try {
        const res = await fetch('/api/pid');
        const data = await res.json();
        setPidInputs(data.kp, data.ki, data.kd);
        document.getElementById('status').textContent = 'Current PID loaded.';
      } catch (e) {
        document.getElementById('status').textContent = 'Failed to load PID values.';
      }
    }

    async function applyPid() {
      const kp = parseFloat(document.getElementById('kp').value);
      const ki = parseFloat(document.getElementById('ki').value);
      const kd = parseFloat(document.getElementById('kd').value);
      if (!Number.isFinite(kp) || !Number.isFinite(ki) || !Number.isFinite(kd)) {
        document.getElementById('status').textContent = 'Enter valid numeric PID values before applying.';
        return;
      }
      document.getElementById('status').textContent = 'Applying PID...';
      try {
        const res = await fetch('/api/pid', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify({ kp, ki, kd }) });
        const data = await res.json();
        if (data.ok) {
          setPidInputs(data.kp, data.ki, data.kd);
          document.getElementById('status').textContent = 'PID applied and saved.';
        } else {
          document.getElementById('status').textContent = 'Failed to apply PID.';
        }
      } catch (e) {
        document.getElementById('status').textContent = 'Failed to apply PID.';
      }
    }

    let activeMethod = 'step_response';

    async function startAutotune() {
      const fan = parseInt(document.getElementById('fan').value) || 255;
      const tauC = parseFloat(document.getElementById('tauC').value) || 0.5;
      let url = `/api/calibrate-pid?fan=${fan}&method=step_response&tau_c=${tauC}`;
      document.getElementById('status').textContent = 'Starting step-response tuning...';
      try {
        const res = await fetch(url, { method: 'POST' });
        if (res.ok) {
          document.getElementById('status').textContent = 'Step-response tuning started.';
          document.getElementById('autotuneSummary').textContent = 'Step-response tuning running...';
          updateTableHeader('step_response');
          loadAutotuneStatus();
          loadAutotuneTrace();
        } else {
          const txt = await res.text();
          document.getElementById('status').textContent = 'Failed to start tuning: ' + txt;
        }
      } catch (e) {
        document.getElementById('status').textContent = 'Failed to start tuning.';
      }
    }

    async function cancelAutotune() {
      document.getElementById('status').textContent = 'Cancelling tuning...';
      try {
        const res = await fetch('/api/calibrate-pid/cancel', { method: 'POST' });
        const txt = await res.text();
        document.getElementById('status').textContent = res.ok ? 'Tuning cancelled.' : 'Failed to cancel tuning.';
        if (!res.ok && txt) {
          document.getElementById('status').textContent += ' ' + txt;
        }
        loadAutotuneStatus();
        loadAutotuneTrace();
      } catch (e) {
        document.getElementById('status').textContent = 'Failed to cancel tuning.';
      }
    }

    async function loadAutotuneTrace() {
      try {
        const res = await fetch('/api/calibrate-pid/trace');
        const data = await res.json();
        renderTrace(data.samples || []);
      } catch (e) {
        if (!latestTraceSamples.length) {
          drawTracePlaceholder('Trace unavailable.');
        }
      }
    }

    function updateTableHeader() {
      const thead = document.getElementById('cycleTableHeader');
      thead.innerHTML = '<tr><th>Band</th><th>Temp</th><th>Kp (process)</th><th>&tau; (s)</th><th>&theta; (s)</th><th>RMSE</th><th>PID</th></tr>';
      document.getElementById('autotuneBestLabel').textContent = 'Bands';
    }

    async function loadAutotuneStatus() {
      try {
        const res = await fetch('/api/calibrate-pid/status');
        const data = await res.json();
        const model = data.model || {};

        document.getElementById('startAutotuneButton').disabled = !!data.running;
        document.getElementById('cancelAutotuneButton').disabled = !data.running;
        document.getElementById('autotunePhase').textContent = data.phase || '--';
        document.getElementById('autotuneProgress').textContent = `${formatNumber(data.progressPercent, 0)}%`;
        document.getElementById('autotuneSetpoint').textContent = Number.isFinite(model.currentSetpoint) ? `${formatNumber(model.currentSetpoint, 1)}F` : '--';
        document.getElementById('autotunePid').textContent = Number.isFinite(model.recommendedKp) ? `Kp ${formatNumber(model.recommendedKp, 2)} Ki ${formatNumber(model.recommendedKi, 3)} Kd ${formatNumber(model.recommendedKd, 2)}` : '--';

        updateTableHeader();
        document.getElementById('autotuneBest').textContent = `${model.completedBands || 0} / ${model.totalBands || 2}`;

        if (data.running) {
          document.getElementById('autotuneOutcome').textContent = 'running';
          document.getElementById('autotuneSummary').textContent = `Step-response tuning in progress. Completed ${model.completedBands || 0} of ${model.totalBands || 2} bands. \u03C4c factor: ${formatNumber(model.tauCFactor, 1)}.`;
        } else if (data.complete) {
          document.getElementById('autotuneOutcome').textContent = model.passed ? 'passed' : 'best found';
          document.getElementById('autotuneSummary').textContent = `Step-response tuning ${model.passed ? 'passed' : 'completed'}: PID Kp ${formatNumber(model.recommendedKp, 2)} Ki ${formatNumber(model.recommendedKi, 3)} Kd ${formatNumber(model.recommendedKd, 2)}, mean RMSE ${formatNumber(model.meanFitRmse, 2)}F.`;
          if (Number.isFinite(model.recommendedKp) && !pidInputsDirty) {
            setPidInputs(model.recommendedKp, model.recommendedKi, model.recommendedKd);
          }
        } else if (data.lastError === 'cancelled') {
          document.getElementById('autotuneOutcome').textContent = 'cancelled';
          document.getElementById('autotuneSummary').textContent = 'Step-response tuning cancelled. Recovery cooling is active.';
        } else if (data.lastError && data.lastError !== 'none') {
          document.getElementById('autotuneOutcome').textContent = 'failed';
          document.getElementById('autotuneSummary').textContent = `Step-response tuning failed: ${data.lastError}.`;
        } else {
          document.getElementById('autotuneOutcome').textContent = 'idle';
        }

        const bands = (model.bands || []).filter(b => b.valid);
        const rows = bands.map(b => {
          return `<tr><td>${b.index}</td><td>${formatNumber(b.targetTemp, 0)}F</td><td>${formatNumber(b.processGain, 4)}</td><td>${formatNumber(b.timeConstant, 1)}</td><td>${formatNumber(b.deadTime, 1)}</td><td>${formatNumber(b.fitRmse, 2)}F</td><td class="small">Kp ${formatNumber(b.kp, 2)} Ki ${formatNumber(b.ki, 3)} Kd ${formatNumber(b.kd, 2)}</td></tr>`;
        });
        document.getElementById('cycleTableBody').innerHTML = rows.length ? rows.join('') : '<tr><td colspan="7" class="small">No band data yet.</td></tr>';
      } catch (e) {
        document.getElementById('autotunePhase').textContent = 'offline';
        document.getElementById('autotuneOutcome').textContent = 'offline';
        document.getElementById('startAutotuneButton').disabled = false;
        document.getElementById('cancelAutotuneButton').disabled = true;
      }
    }

    drawTracePlaceholder('Loading trace...');
  bindPidInputs();
    loadPid();
    loadAutotuneStatus();
    loadAutotuneTrace();
    setInterval(() => {
      loadAutotuneStatus();
      loadAutotuneTrace();
    }, 1500);
  </script>
</body>
</html>
)rawliteral");
  });

  // Profile Editor UI
  server.on("/profile", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_INFO("Profile Editor UI accessed");
    request->send_P(200, "text/html", PROFILE_EDITOR_HTML);
  });

  server.on("/systemlink", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_INFO("SystemLink config UI accessed");
    request->send_P(200, "text/html", SYSTEMLINK_CONFIG_HTML);
  });

  // Start ElegantOTA for over-the-air updates
  ElegantOTA.begin(&server);  // Start ElegantOTA in async mode
  ElegantOTA.setAutoReboot(false);
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
  if (wifiCredentials.ssid.length() == 0) return;

  static unsigned long lastCheck = 0;
  static int reconnectAttempts = 0;
  const unsigned long CHECK_INTERVAL = 10000;  // Check every 10 seconds
  const int MAX_RECONNECT_ATTEMPTS = 3;        // Try 3 times before giving up
  
  if (millis() - lastCheck < CHECK_INTERVAL) {
    return;  // Not time to check yet
  }
  
  lastCheck = millis();
  
  if (WiFi.status() != WL_CONNECTED) {
    // If we've exceeded max attempts, reset counter but don't try this time
    // This creates a "cool down" period of one CHECK_INTERVAL
    if (reconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
      reconnectAttempts = 0;
      DEBUG_PRINTLN("WiFi: Max reconnect attempts reached, pausing...");
      return;
    }

    reconnectAttempts++;
    DEBUG_PRINTF("WiFi disconnected! Attempting reconnect %d/%d\n", reconnectAttempts, MAX_RECONNECT_ATTEMPTS);
    
    // Non-blocking reconnect: just start the process and return
    // The next check (in 10s) will verify if it worked
    WiFi.disconnect();
    WiFi.begin(wifiCredentials.ssid.c_str(), wifiCredentials.password.c_str());
    
  } else {
    if (reconnectAttempts > 0) {
      DEBUG_PRINTF("WiFi reconnected! IP: %s\n", WiFi.localIP().toString().c_str());
      reconnectAttempts = 0;
    }
  }
#endif
}

#endif // NETWORK_HPP


