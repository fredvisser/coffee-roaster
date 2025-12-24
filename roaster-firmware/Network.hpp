#ifndef NETWORK_HPP
#define NETWORK_HPP

#include "Types.hpp"
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ESPmDNS.h>
#include <ElegantOTA.h>  // v3.1.7+ with async mode enabled for ESPAsyncWebServer compatibility
#include "DebugLog.hpp"

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
extern int setpointProgress;
extern int bdcFanMs;
extern int badReadingCount;
extern RoasterState roasterState;  // Defined in Types.hpp
extern Profiles profile;  // Profile configuration

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

// State name helper
const char* getStateName(byte state) {
  switch(state) {
    case 0: return "IDLE";
    case 1: return "START_ROAST";
    case 2: return "ROASTING";
    case 3: return "COOLING";
    case 4: return "ERROR";
    default: return "UNKNOWN";
  }
}

// Serialize system state to JSON
String getSystemStateJSON() {
  StaticJsonDocument<1024> doc;
  
  doc["timestamp"] = millis();
  doc["state"] = getStateName(roasterState);
  doc["uptime"] = millis() / 1000;
  
  JsonObject temps = doc.createNestedObject("temps");
  temps["current"] = round(currentTemp * 10) / 10.0;
  temps["setpoint"] = round(setpointTemp * 10) / 10.0;
  temps["fan"] = round(fanTemp * 10) / 10.0;
  
  JsonObject control = doc.createNestedObject("control");
  control["heater"] = (int)heaterOutputVal;
  control["pwmFan"] = (int)setpointFanSpeed;
  control["bdcFan"] = bdcFanMs;
  
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

// Serialize profile to JSON
String getProfileJSON() {
  StaticJsonDocument<2048> doc;
  
  doc["setpointCount"] = profile.getSetpointCount();
  doc["finalTemp"] = (int)profile.getFinalTargetTemp();
  
  JsonArray setpoints = doc.createNestedArray("setpoints");
  for (int i = 0; i < profile.getSetpointCount(); i++) {
    auto sp = profile.getSetpoint(i);
    JsonObject setpoint = setpoints.createNestedObject();
    setpoint["time"] = sp.time / 1000;  // Convert to seconds
    setpoint["temp"] = sp.temp;
    setpoint["fanSpeed"] = sp.fanSpeed;
  }
  
  String output;
  serializeJson(doc, output);
  return output;
}

// Broadcast system state to all WebSocket clients
void broadcastSystemState() {
  String stateJson = getSystemStateJSON();
  ws.textAll(stateJson);
}

// Broadcast logs to all WebSocket clients
void broadcastLogs(int maxEntries = 10) {
  String logsJson = debugLogger.getLogsJSON(maxEntries);
  ws.textAll(logsJson);
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

  // API endpoint: System state
  server.on("/api/state", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("API: /api/state requested");
    String json = getSystemStateJSON();
    request->send(200, "application/json", json);
  });

  // API endpoint: Profile data
  server.on("/api/profile", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("API: /api/profile requested");
    String json = getProfileJSON();
    request->send(200, "application/json", json);
  });

  // API endpoint: Debug logs
  server.on("/api/logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    LOG_DEBUG("API: /api/logs requested");
    int maxEntries = 50;
    if (request->hasParam("max")) {
      maxEntries = request->getParam("max")->value().toInt();
      maxEntries = constrain(maxEntries, 1, 100);
    }
    String json = debugLogger.getLogsJSON(maxEntries);
    request->send(200, "application/json", json);
  });

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


