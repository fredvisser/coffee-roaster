#ifndef SYSTEMLINK_HPP
#define SYSTEMLINK_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include "DebugLog.hpp"
#include "ProfileManager.hpp"
#include "Types.hpp"

extern Preferences preferences;
extern ProfileManager profileManager;
extern Profiles profile;
extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern double fanTemp;
extern double heaterOutputVal;
extern int setpointProgress;
extern RoasterState roasterState;
extern double kp;
extern double ki;
extern double kd;
extern int finalTempOverride;

static const char *SYSTEMLINK_API_URL_KEY = "sl_api_url";
static const char *SYSTEMLINK_API_KEY_KEY = "sl_api_key";
static const char *SYSTEMLINK_ENABLED_KEY = "sl_enabled";
static const char *SYSTEMLINK_SYSTEM_ID_KEY = "sl_sys_id";
static const char *SYSTEMLINK_WORKSPACE_KEY = "sl_ws_id";
static const char *SYSTEMLINK_PHASE_KEY = "sl_phase";
static const char *SYSTEMLINK_ACTIVE_KEY = "sl_active";
static const char *SYSTEMLINK_PENDING_KEY = "sl_pubpend";
static const char *SYSTEMLINK_BC_PROFILE_ID_KEY = "sl_profid";
static const char *SYSTEMLINK_BC_PROFILE_NAME_KEY = "sl_pname";
static const char *SYSTEMLINK_BC_TARGET_KEY = "sl_ftarget";
static const char *SYSTEMLINK_BC_SP_COUNT_KEY = "sl_spcnt";
static const char *SYSTEMLINK_BC_KP_KEY = "sl_kp";
static const char *SYSTEMLINK_BC_KI_KEY = "sl_ki";
static const char *SYSTEMLINK_BC_KD_KEY = "sl_kd";
static const char *SYSTEMLINK_BC_OVERRIDE_KEY = "sl_ovr";
static const char *SYSTEMLINK_BC_REASON_KEY = "sl_reason";
static const char *SYSTEMLINK_LAST_FAULT_KEY = "sl_fault";
static const char *SYSTEMLINK_LAST_PUB_STATUS_KEY = "sl_pubst";

static const size_t SYSTEMLINK_API_URL_MAX = 96;
static const size_t SYSTEMLINK_WORKSPACE_MAX = 48;
static const size_t SYSTEMLINK_SYSTEM_ID_MAX = 48;
static const size_t SYSTEMLINK_API_KEY_MAX = 160;
static const size_t SYSTEMLINK_PROFILE_ID_MAX = 16;
static const size_t SYSTEMLINK_PROFILE_NAME_MAX = 64;
static const size_t SYSTEMLINK_REASON_MAX = 96;
static const size_t SYSTEMLINK_PHASE_MAX = 32;
static const size_t SYSTEMLINK_STATUS_MAX = 48;
static const size_t SYSTEMLINK_RESET_REASON_MAX = 32;
static const size_t SYSTEMLINK_MAX_TRACE_SAMPLES = 1800;
static const int SYSTEMLINK_STATUS_TAG_RETENTION_DAYS = 30;

static const char *SYSTEMLINK_PROP_RETENTION = "nitagRetention";
static const char *SYSTEMLINK_PROP_HISTORY_TTL_DAYS = "nitagHistoryTTLDays";
static const char *SYSTEMLINK_RETENTION_DURATION = "DURATION";

struct SystemLinkConfig {
  bool enabled;
  char apiUrl[SYSTEMLINK_API_URL_MAX];
  char workspaceId[SYSTEMLINK_WORKSPACE_MAX];
  char systemId[SYSTEMLINK_SYSTEM_ID_MAX];
  char apiKey[SYSTEMLINK_API_KEY_MAX];
};

enum SystemLinkRoastOutcome {
  SYSTEMLINK_OUTCOME_NONE = 0,
  SYSTEMLINK_OUTCOME_PASSED,
  SYSTEMLINK_OUTCOME_TERMINATED,
  SYSTEMLINK_OUTCOME_ERRORED
};

struct RoastTraceSample {
  uint16_t elapsedSeconds;
  int16_t actualTenthsF;
  int16_t targetTenthsF;
  int16_t heaterOutputTenths;
};

struct SystemLinkTelemetrySnapshot {
  bool active;
  float chamberTempF;
  float targetTempF;
  int roastProgress;
  RoasterState state;
  char lastFault[SYSTEMLINK_REASON_MAX];
  char publishStatus[SYSTEMLINK_STATUS_MAX];
  char resetReason[SYSTEMLINK_RESET_REASON_MAX];
  int bootCount;
};

struct SystemLinkRoastSession {
  bool active;
  bool publishPending;
  bool traceOverflow;
  bool publishInProgress;
  uint32_t startedAtMs;
  uint32_t roastingStartedAtMs;
  uint32_t coolingStartedAtMs;
  uint32_t endedAtMs;
  uint16_t sampleCount;
  uint16_t lastRecordedSecond;
  uint16_t setpointCount;
  uint32_t finalTargetTempF;
  SystemLinkRoastOutcome outcome;
  double kp;
  double ki;
  double kd;
  int16_t finalTempOverrideF;
  bool recoveredAfterReset;
  char profileId[SYSTEMLINK_PROFILE_ID_MAX];
  char profileName[SYSTEMLINK_PROFILE_NAME_MAX];
  char outcomeReason[SYSTEMLINK_REASON_MAX];
  char outcomePhase[SYSTEMLINK_PHASE_MAX];
  char phase[SYSTEMLINK_PHASE_MAX];
  char resetReason[SYSTEMLINK_RESET_REASON_MAX];
  RoastTraceSample samples[SYSTEMLINK_MAX_TRACE_SAMPLES];
};

static portMUX_TYPE systemLinkLock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t systemLinkTagTaskHandle = nullptr;
static TaskHandle_t systemLinkPublishTaskHandle = nullptr;
static SystemLinkConfig systemLinkConfig = {
  false,
  "https://dev-api.lifecyclesolutions.ni.com",
  "",
  "",
  ""
};
static SystemLinkTelemetrySnapshot systemLinkTelemetry = {false, 0.0f, 0.0f, 0, IDLE, "none", "idle", "unknown", 0};
static SystemLinkRoastSession systemLinkSession = {};
static SystemLinkRoastSession systemLinkPublishSession = {};
static bool systemLinkPublishPending = false;
static bool systemLinkPublishInProgress = false;
static uint32_t systemLinkLastPublishAttemptMs = 0;
static bool systemLinkTagsProvisioned = false;
static uint32_t systemLinkLastIdleChamberPublishMs = 0;
static bool systemLinkLastTelemetrySentValid = false;
static SystemLinkTelemetrySnapshot systemLinkLastTelemetrySent = {false, 0.0f, 0.0f, 0, IDLE, "", "", "", 0};

static bool systemLinkIsTrackedRoastState(RoasterState state);
static void systemLinkCopyString(char *dest, size_t destSize, const String &src);

static void systemLinkInvalidateTagPublishState() {
  systemLinkTagsProvisioned = false;
  systemLinkLastIdleChamberPublishMs = 0;
  systemLinkLastTelemetrySentValid = false;
  memset(&systemLinkLastTelemetrySent, 0, sizeof(systemLinkLastTelemetrySent));
}

static int systemLinkOutcomePriority(SystemLinkRoastOutcome outcome) {
  switch (outcome) {
    case SYSTEMLINK_OUTCOME_PASSED:
      return 1;
    case SYSTEMLINK_OUTCOME_TERMINATED:
      return 2;
    case SYSTEMLINK_OUTCOME_ERRORED:
      return 3;
    case SYSTEMLINK_OUTCOME_NONE:
    default:
      return 0;
  }
}

static void systemLinkAssignOutcome(SystemLinkRoastSession &session,
                                    SystemLinkRoastOutcome outcome,
                                    const char *reason,
                                    const char *outcomePhase) {
  if (outcome == SYSTEMLINK_OUTCOME_NONE) {
    return;
  }

  int currentPriority = systemLinkOutcomePriority(session.outcome);
  int incomingPriority = systemLinkOutcomePriority(outcome);
  if (incomingPriority < currentPriority) {
    return;
  }

  session.outcome = outcome;
  if (reason != nullptr && reason[0] != '\0') {
    systemLinkCopyString(session.outcomeReason, sizeof(session.outcomeReason), String(reason));
  }
  if (outcomePhase != nullptr && outcomePhase[0] != '\0') {
    systemLinkCopyString(session.outcomePhase, sizeof(session.outcomePhase), String(outcomePhase));
  }
}

static int systemLinkTenths(float value) {
  return static_cast<int>(lroundf(value * 10.0f));
}

static bool systemLinkShouldPublishChamberTemp(const SystemLinkTelemetrySnapshot &snapshot) {
  if (systemLinkIsTrackedRoastState(snapshot.state)) {
    return true;
  }

  uint32_t now = millis();
  if (!systemLinkLastTelemetrySentValid || now - systemLinkLastIdleChamberPublishMs >= 300000UL) {
    systemLinkLastIdleChamberPublishMs = now;
    return true;
  }

  return false;
}

static const char *systemLinkOutcomePhaseForCoolingStart(const SystemLinkRoastSession &session,
                                                         SystemLinkRoastOutcome outcome,
                                                         const char *reason) {
  if (outcome == SYSTEMLINK_OUTCOME_PASSED) {
    return "roasting";
  }
  if (outcome == SYSTEMLINK_OUTCOME_TERMINATED && reason != nullptr && strcmp(reason, "user_stop") == 0) {
    return session.roastingStartedAtMs != 0 ? "roasting" : "starting";
  }
  return "cooling";
}

static String systemLinkProfileSetpointsJson(const char *profileId) {
  if (profileId == nullptr || profileId[0] == '\0') {
    return "[]";
  }

  String profileJson = profileManager.getProfile(profileId);
  if (profileJson.length() == 0) {
    return "[]";
  }

  DynamicJsonDocument profileDoc(4096);
  if (deserializeJson(profileDoc, profileJson)) {
    return "[]";
  }

  String setpointsJson;
  serializeJson(profileDoc["setpoints"], setpointsJson);
  return setpointsJson.length() > 0 ? setpointsJson : "[]";
}

static bool systemLinkIsTrackedRoastState(RoasterState state) {
  return state == START_ROAST || state == ROASTING || state == COOLING;
}

static double systemLinkElapsedSeconds(uint32_t startMs, uint32_t endMs) {
  if (endMs <= startMs) {
    return 0.0;
  }
  return static_cast<double>(endMs - startMs) / 1000.0;
}

static double systemLinkStartPhaseSeconds(const SystemLinkRoastSession &session) {
  uint32_t phaseEnd = session.endedAtMs;
  if (session.roastingStartedAtMs != 0) {
    phaseEnd = session.roastingStartedAtMs;
  } else if (session.coolingStartedAtMs != 0) {
    phaseEnd = session.coolingStartedAtMs;
  }
  return systemLinkElapsedSeconds(session.startedAtMs, phaseEnd);
}

static double systemLinkRoastingPhaseSeconds(const SystemLinkRoastSession &session) {
  if (session.roastingStartedAtMs == 0) {
    return 0.0;
  }
  uint32_t phaseEnd = session.coolingStartedAtMs != 0 ? session.coolingStartedAtMs : session.endedAtMs;
  return systemLinkElapsedSeconds(session.roastingStartedAtMs, phaseEnd);
}

static double systemLinkCoolingPhaseSeconds(const SystemLinkRoastSession &session) {
  if (session.coolingStartedAtMs == 0) {
    return 0.0;
  }
  return systemLinkElapsedSeconds(session.coolingStartedAtMs, session.endedAtMs);
}

static const char *systemLinkStateName(RoasterState state) {
  switch (state) {
    case IDLE:
      return "IDLE";
    case START_ROAST:
      return "START_ROAST";
    case ROASTING:
      return "ROASTING";
    case COOLING:
      return "COOLING";
    case ERROR:
      return "ERROR";
    case CALIBRATING:
      return "CALIBRATING";
    default:
      return "UNKNOWN";
  }
}

static const char *systemLinkResetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_UNKNOWN: return "UNKNOWN";
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_EXT: return "EXTERNAL";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_INT_WDT: return "INT_WDT";
    case ESP_RST_TASK_WDT: return "TASK_WDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_SDIO: return "SDIO";
    default: return "OTHER";
  }
}

static void systemLinkCopyString(char *dest, size_t destSize, const String &src) {
  if (destSize == 0) {
    return;
  }
  src.substring(0, destSize - 1).toCharArray(dest, destSize);
  dest[destSize - 1] = '\0';
}

static String systemLinkMaskedKey() {
  if (systemLinkConfig.apiKey[0] == '\0') {
    return "";
  }

  String key(systemLinkConfig.apiKey);
  if (key.length() <= 8) {
    return "stored";
  }

  return String("...") + key.substring(key.length() - 4);
}

static void systemLinkUpdatePublishStatus(const String &status, bool persist = true) {
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkCopyString(systemLinkTelemetry.publishStatus, sizeof(systemLinkTelemetry.publishStatus), status);
  portEXIT_CRITICAL(&systemLinkLock);

  if (persist) {
    preferences.putString(SYSTEMLINK_LAST_PUB_STATUS_KEY, status);
  }
}

static void systemLinkUpdateLastFault(const String &fault, bool persist = true) {
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkCopyString(systemLinkTelemetry.lastFault, sizeof(systemLinkTelemetry.lastFault), fault);
  portEXIT_CRITICAL(&systemLinkLock);

  if (persist) {
    preferences.putString(SYSTEMLINK_LAST_FAULT_KEY, fault);
  }
}

static void systemLinkSetBootContext(int bootCount) {
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkTelemetry.bootCount = bootCount;
  systemLinkCopyString(systemLinkTelemetry.resetReason,
                       sizeof(systemLinkTelemetry.resetReason),
                       systemLinkResetReasonName(esp_reset_reason()));
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkPersistBreadcrumb(const SystemLinkRoastSession &session,
                                        bool active,
                                        bool pending,
                                        const char *phase) {
  preferences.putBool(SYSTEMLINK_ACTIVE_KEY, active);
  preferences.putBool(SYSTEMLINK_PENDING_KEY, pending);
  preferences.putString(SYSTEMLINK_PHASE_KEY, phase);
  preferences.putString(SYSTEMLINK_BC_PROFILE_ID_KEY, session.profileId);
  preferences.putString(SYSTEMLINK_BC_PROFILE_NAME_KEY, session.profileName);
  preferences.putUInt(SYSTEMLINK_BC_TARGET_KEY, session.finalTargetTempF);
  preferences.putUInt(SYSTEMLINK_BC_SP_COUNT_KEY, session.setpointCount);
  preferences.putDouble(SYSTEMLINK_BC_KP_KEY, session.kp);
  preferences.putDouble(SYSTEMLINK_BC_KI_KEY, session.ki);
  preferences.putDouble(SYSTEMLINK_BC_KD_KEY, session.kd);
  preferences.putInt(SYSTEMLINK_BC_OVERRIDE_KEY, session.finalTempOverrideF);
  preferences.putString(SYSTEMLINK_BC_REASON_KEY, session.outcomeReason);
}

static void systemLinkClearBreadcrumb() {
  preferences.putBool(SYSTEMLINK_ACTIVE_KEY, false);
  preferences.putBool(SYSTEMLINK_PENDING_KEY, false);
  preferences.putString(SYSTEMLINK_PHASE_KEY, "idle");
  preferences.remove(SYSTEMLINK_BC_PROFILE_ID_KEY);
  preferences.remove(SYSTEMLINK_BC_PROFILE_NAME_KEY);
  preferences.remove(SYSTEMLINK_BC_TARGET_KEY);
  preferences.remove(SYSTEMLINK_BC_SP_COUNT_KEY);
  preferences.remove(SYSTEMLINK_BC_KP_KEY);
  preferences.remove(SYSTEMLINK_BC_KI_KEY);
  preferences.remove(SYSTEMLINK_BC_KD_KEY);
  preferences.remove(SYSTEMLINK_BC_OVERRIDE_KEY);
  preferences.remove(SYSTEMLINK_BC_REASON_KEY);
}

static void systemLinkPrepareRecoveryPublish() {
  bool hadActive = preferences.getBool(SYSTEMLINK_ACTIVE_KEY, false);
  bool hadPending = preferences.getBool(SYSTEMLINK_PENDING_KEY, false);
  if (!hadActive && !hadPending) {
    return;
  }

  memset(&systemLinkPublishSession, 0, sizeof(SystemLinkRoastSession));
  systemLinkPublishSession.outcome = SYSTEMLINK_OUTCOME_ERRORED;
  systemLinkPublishSession.recoveredAfterReset = true;
  systemLinkPublishSession.finalTargetTempF = preferences.getUInt(SYSTEMLINK_BC_TARGET_KEY, 0);
  systemLinkPublishSession.setpointCount = preferences.getUInt(SYSTEMLINK_BC_SP_COUNT_KEY, 0);
  systemLinkPublishSession.kp = preferences.getDouble(SYSTEMLINK_BC_KP_KEY, kp);
  systemLinkPublishSession.ki = preferences.getDouble(SYSTEMLINK_BC_KI_KEY, ki);
  systemLinkPublishSession.kd = preferences.getDouble(SYSTEMLINK_BC_KD_KEY, kd);
  systemLinkPublishSession.finalTempOverrideF = preferences.getInt(SYSTEMLINK_BC_OVERRIDE_KEY, -1);
  systemLinkCopyString(systemLinkPublishSession.profileId,
                       sizeof(systemLinkPublishSession.profileId),
                       preferences.getString(SYSTEMLINK_BC_PROFILE_ID_KEY, ""));
  systemLinkCopyString(systemLinkPublishSession.profileName,
                       sizeof(systemLinkPublishSession.profileName),
                       preferences.getString(SYSTEMLINK_BC_PROFILE_NAME_KEY, ""));
  String phase = preferences.getString(SYSTEMLINK_PHASE_KEY, hadPending ? "publish_pending" : "roasting");
  String resetReason = systemLinkResetReasonName(esp_reset_reason());
  String reason = String("reset_during_") + phase + ":" + resetReason;
  systemLinkCopyString(systemLinkPublishSession.outcomeReason,
                       sizeof(systemLinkPublishSession.outcomeReason),
                       reason);
  systemLinkCopyString(systemLinkPublishSession.phase,
                       sizeof(systemLinkPublishSession.phase),
                       phase);
  systemLinkCopyString(systemLinkPublishSession.outcomePhase,
                       sizeof(systemLinkPublishSession.outcomePhase),
                       phase);
  systemLinkCopyString(systemLinkPublishSession.resetReason,
                       sizeof(systemLinkPublishSession.resetReason),
                       resetReason);
  systemLinkPublishPending = true;
  systemLinkUpdatePublishStatus("recovery_pending");
  LOG_WARNF("SystemLink: Prepared recovery publish for interrupted roast (%s)", reason.c_str());
}

static bool systemLinkHasRequiredConfig() {
  return systemLinkConfig.enabled &&
         systemLinkConfig.apiKey[0] != '\0' &&
         systemLinkConfig.apiUrl[0] != '\0' &&
         systemLinkConfig.workspaceId[0] != '\0' &&
         systemLinkConfig.systemId[0] != '\0';
}

static String systemLinkBaseUrl(const char *servicePath) {
  String base(systemLinkConfig.apiUrl);
  base.trim();
  if (base.endsWith("/")) {
    base.remove(base.length() - 1);
  }
  return base + servicePath;
}

static bool systemLinkHttpRequest(const String &method,
                                  const String &url,
                                  const String &contentType,
                                  const uint8_t *payload,
                                  size_t payloadLength,
                                  String &responseBody,
                                  int &statusCode) {
  responseBody = "";
  statusCode = -1;

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(5000);

  if (!http.begin(client, url)) {
    LOG_ERRORF("SystemLink: Failed to open %s", url.c_str());
    return false;
  }

  http.addHeader("accept", "application/json");
  http.addHeader("x-ni-api-key", systemLinkConfig.apiKey);
  if (contentType.length() > 0) {
    http.addHeader("Content-Type", contentType);
  }

  esp_task_wdt_reset();
  if (method == "POST") {
    statusCode = http.POST(const_cast<uint8_t *>(payload), payloadLength);
  } else if (method == "PUT") {
    statusCode = http.PUT(const_cast<uint8_t *>(payload), payloadLength);
  } else {
    statusCode = http.sendRequest(method.c_str(), const_cast<uint8_t *>(payload), payloadLength);
  }
  responseBody = http.getString();
  http.end();
  esp_task_wdt_reset();

  return statusCode >= 200 && statusCode < 300;
}

static bool systemLinkParseApiEndpoint(String &host, uint16_t &port) {
  String base(systemLinkConfig.apiUrl);
  base.trim();
  if (base.length() == 0) {
    return false;
  }

  if (base.startsWith("https://")) {
    base.remove(0, 8);
    port = 443;
  } else if (base.startsWith("http://")) {
    base.remove(0, 7);
    port = 80;
  } else {
    port = 443;
  }

  int slash = base.indexOf('/');
  if (slash >= 0) {
    base = base.substring(0, slash);
  }

  int colon = base.indexOf(':');
  if (colon >= 0) {
    port = static_cast<uint16_t>(base.substring(colon + 1).toInt());
    base = base.substring(0, colon);
  }

  base.trim();
  if (base.length() == 0) {
    return false;
  }

  host = base;
  return true;
}

static bool systemLinkPostJson(const String &url,
                               const String &jsonBody,
                               String &responseBody,
                               int &statusCode) {
  return systemLinkHttpRequest("POST",
                               url,
                               "application/json",
                               reinterpret_cast<const uint8_t *>(jsonBody.c_str()),
                               jsonBody.length(),
                               responseBody,
                               statusCode);
}

static bool systemLinkPutJson(const String &url,
                              const String &jsonBody,
                              String &responseBody,
                              int &statusCode) {
  return systemLinkHttpRequest("PUT",
                               url,
                               "application/json",
                               reinterpret_cast<const uint8_t *>(jsonBody.c_str()),
                               jsonBody.length(),
                               responseBody,
                               statusCode);
}

static size_t systemLinkCsvLength(const SystemLinkRoastSession &session) {
  size_t total = strlen("elapsedSeconds,actualTempF,targetTempF,heaterOutput\n");
  char row[64];

  for (uint16_t index = 0; index < session.sampleCount; index++) {
    const RoastTraceSample &sample = session.samples[index];
    int rowLen = snprintf(row,
                          sizeof(row),
                          "%u,%.1f,%.1f,%.1f\n",
                          static_cast<unsigned>(sample.elapsedSeconds),
                          sample.actualTenthsF / 10.0f,
                          sample.targetTenthsF / 10.0f,
                          sample.heaterOutputTenths / 10.0f);
    if (rowLen > 0) {
      total += static_cast<size_t>(rowLen);
    }
  }

  return total;
}

static bool systemLinkUploadTraceFile(const String &filename,
                                     const String &contentType,
                                     const SystemLinkRoastSession &session,
                                     String &uploadedUri) {
  uploadedUri = "";

  String host;
  uint16_t port;
  if (!systemLinkParseApiEndpoint(host, port)) {
    LOG_ERROR("SystemLink: Invalid API URL configuration");
    return false;
  }

  const String boundary = "----CoffeeRoasterSystemLinkBoundary";
  const char *csvHeader = "elapsedSeconds,actualTempF,targetTempF,heaterOutput\n";
  String prefix;
  prefix.reserve(filename.length() + contentType.length() + boundary.length() + 128);
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
  prefix += "Content-Type: " + contentType + "\r\n\r\n";

  String suffix = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = prefix.length() + systemLinkCsvLength(session) + suffix.length();

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(5);
  if (!client.connect(host.c_str(), port)) {
    LOG_ERRORF("SystemLink: Failed to connect to %s:%u", host.c_str(), static_cast<unsigned>(port));
    return false;
  }

  String requestPath = String("/nifile/v1/service-groups/Default/upload-files?workspace=") + systemLinkConfig.workspaceId;
  client.printf("POST %s HTTP/1.1\r\n", requestPath.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.print("Connection: close\r\n");
  client.print("Accept: application/json\r\n");
  client.printf("x-ni-api-key: %s\r\n", systemLinkConfig.apiKey);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %u\r\n\r\n", static_cast<unsigned>(contentLength));
  client.print(prefix);
  client.print(csvHeader);

  char row[64];
  for (uint16_t index = 0; index < session.sampleCount; index++) {
    const RoastTraceSample &sample = session.samples[index];
    int rowLen = snprintf(row,
                          sizeof(row),
                          "%u,%.1f,%.1f,%.1f\n",
                          static_cast<unsigned>(sample.elapsedSeconds),
                          sample.actualTenthsF / 10.0f,
                          sample.targetTenthsF / 10.0f,
                          sample.heaterOutputTenths / 10.0f);
    if (rowLen <= 0 || rowLen >= static_cast<int>(sizeof(row))) {
      client.stop();
      LOG_ERROR("SystemLink: Failed to format CSV row");
      return false;
    }
    client.write(reinterpret_cast<const uint8_t *>(row), static_cast<size_t>(rowLen));
    esp_task_wdt_reset();
  }

  client.print(suffix);
  client.flush();

  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && millis() - waitStart < 7000UL) {
    delay(10);
    esp_task_wdt_reset();
  }

  if (!client.available()) {
    client.stop();
    LOG_ERROR("SystemLink: Timed out waiting for upload response");
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  int statusCode = -1;
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace >= 0 && statusLine.length() >= firstSpace + 4) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }

  while (client.available() || client.connected()) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r" || headerLine.length() == 0) {
      break;
    }
  }

  String responseBody = client.readString();
  client.stop();
  if (statusCode < 200 || statusCode >= 300) {
    LOG_ERRORF("SystemLink: File upload failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, responseBody)) {
    LOG_ERRORF("SystemLink: Failed to parse upload response: %s", responseBody.c_str());
    return false;
  }

  if (doc.is<JsonObject>() && doc["uri"].is<const char *>()) {
    uploadedUri = doc["uri"].as<String>();
    return true;
  }

  if (doc.is<JsonArray>() && doc[0]["uri"].is<const char *>()) {
    uploadedUri = doc[0]["uri"].as<String>();
    return true;
  }

  LOG_ERRORF("SystemLink: Upload response missing uri: %s", responseBody.c_str());
  return false;
}

static String systemLinkExtractIdFromUri(const String &uri) {
  int lastSlash = uri.lastIndexOf('/');
  if (lastSlash == -1 || lastSlash + 1 >= uri.length()) {
    return uri;
  }
  return uri.substring(lastSlash + 1);
}

static String systemLinkStatusTypeName(SystemLinkRoastOutcome outcome) {
  switch (outcome) {
    case SYSTEMLINK_OUTCOME_PASSED:
      return "PASSED";
    case SYSTEMLINK_OUTCOME_TERMINATED:
      return "TERMINATED";
    case SYSTEMLINK_OUTCOME_ERRORED:
      return "ERRORED";
    default:
      return "CUSTOM";
  }
}

static String systemLinkStatusDisplayName(SystemLinkRoastOutcome outcome) {
  switch (outcome) {
    case SYSTEMLINK_OUTCOME_PASSED:
      return "Passed";
    case SYSTEMLINK_OUTCOME_TERMINATED:
      return "Terminated";
    case SYSTEMLINK_OUTCOME_ERRORED:
      return "Errored";
    default:
      return "Unknown";
  }
}

static void loadSystemLinkConfig() {
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkConfig.enabled = preferences.getBool(SYSTEMLINK_ENABLED_KEY, false);
  systemLinkCopyString(systemLinkConfig.apiUrl,
                       sizeof(systemLinkConfig.apiUrl),
                       preferences.getString(SYSTEMLINK_API_URL_KEY,
                                             "https://dev-api.lifecyclesolutions.ni.com"));
  systemLinkCopyString(systemLinkConfig.workspaceId,
                       sizeof(systemLinkConfig.workspaceId),
                       preferences.getString(SYSTEMLINK_WORKSPACE_KEY, ""));
  systemLinkCopyString(systemLinkConfig.systemId,
                       sizeof(systemLinkConfig.systemId),
                       preferences.getString(SYSTEMLINK_SYSTEM_ID_KEY, ""));
  systemLinkCopyString(systemLinkConfig.apiKey,
                       sizeof(systemLinkConfig.apiKey),
                       preferences.getString(SYSTEMLINK_API_KEY_KEY, ""));
  systemLinkCopyString(systemLinkTelemetry.lastFault,
                       sizeof(systemLinkTelemetry.lastFault),
                       preferences.getString(SYSTEMLINK_LAST_FAULT_KEY, "none"));
  systemLinkCopyString(systemLinkTelemetry.publishStatus,
                       sizeof(systemLinkTelemetry.publishStatus),
                       preferences.getString(SYSTEMLINK_LAST_PUB_STATUS_KEY, "idle"));
  portEXIT_CRITICAL(&systemLinkLock);
}

static void saveSystemLinkConfig() {
  preferences.putBool(SYSTEMLINK_ENABLED_KEY, systemLinkConfig.enabled);
  preferences.putString(SYSTEMLINK_API_URL_KEY, systemLinkConfig.apiUrl);
  preferences.putString(SYSTEMLINK_WORKSPACE_KEY, systemLinkConfig.workspaceId);
  preferences.putString(SYSTEMLINK_SYSTEM_ID_KEY, systemLinkConfig.systemId);
  if (systemLinkConfig.apiKey[0] == '\0') {
    preferences.remove(SYSTEMLINK_API_KEY_KEY);
  } else {
    preferences.putString(SYSTEMLINK_API_KEY_KEY, systemLinkConfig.apiKey);
  }
}

static String getSystemLinkConfigJSON() {
  DynamicJsonDocument doc(768);
  portENTER_CRITICAL(&systemLinkLock);
  doc["enabled"] = systemLinkConfig.enabled;
  doc["apiUrl"] = systemLinkConfig.apiUrl;
  doc["workspaceId"] = systemLinkConfig.workspaceId;
  doc["systemId"] = systemLinkConfig.systemId;
  doc["hasApiKey"] = systemLinkConfig.apiKey[0] != '\0';
  doc["apiKeyMasked"] = systemLinkMaskedKey();
  portEXIT_CRITICAL(&systemLinkLock);
  String json;
  serializeJson(doc, json);
  return json;
}

static bool updateSystemLinkConfigFromJSON(const String &body, String &errorMessage) {
  DynamicJsonDocument doc(1024);
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    errorMessage = "invalid_json";
    return false;
  }

  portENTER_CRITICAL(&systemLinkLock);
  if (doc["enabled"].is<bool>()) {
    systemLinkConfig.enabled = doc["enabled"].as<bool>();
  }
  if (doc["apiUrl"].is<const char *>()) {
    systemLinkCopyString(systemLinkConfig.apiUrl, sizeof(systemLinkConfig.apiUrl), doc["apiUrl"].as<String>());
  }
  if (doc["workspaceId"].is<const char *>()) {
    systemLinkCopyString(systemLinkConfig.workspaceId, sizeof(systemLinkConfig.workspaceId), doc["workspaceId"].as<String>());
  }
  if (doc["systemId"].is<const char *>()) {
    systemLinkCopyString(systemLinkConfig.systemId, sizeof(systemLinkConfig.systemId), doc["systemId"].as<String>());
  }
  if (doc["clearApiKey"].as<bool>()) {
    systemLinkConfig.apiKey[0] = '\0';
  } else if (doc["apiKey"].is<const char *>()) {
    String apiKey = doc["apiKey"].as<String>();
    apiKey.trim();
    if (apiKey.length() > 0) {
      systemLinkCopyString(systemLinkConfig.apiKey, sizeof(systemLinkConfig.apiKey), apiKey);
    }
  }
  portEXIT_CRITICAL(&systemLinkLock);

  saveSystemLinkConfig();
  systemLinkInvalidateTagPublishState();
  errorMessage = "";
  return true;
}

static String systemLinkTagPath(const char *suffix) {
  String path(systemLinkConfig.systemId);
  path += ".";
  path += suffix;
  return path;
}

static bool systemLinkCreateOrUpdateTag(const String &path,
                                        const char *type,
                                        bool collectAggregates = false,
                                        int retentionDays = 0) {
  DynamicJsonDocument doc(768);
  doc["path"] = path;
  doc["type"] = type;
  doc["workspace"] = systemLinkConfig.workspaceId;
  if (collectAggregates) {
    doc["collectAggregates"] = true;
  }
  if (retentionDays > 0) {
    JsonObject properties = doc.createNestedObject("properties");
    properties[SYSTEMLINK_PROP_RETENTION] = SYSTEMLINK_RETENTION_DURATION;
    properties[SYSTEMLINK_PROP_HISTORY_TTL_DAYS] = String(retentionDays);
  }

  String body;
  serializeJson(doc, body);

  String responseBody;
  int statusCode = -1;
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitag/v2/tags"), body, responseBody, statusCode);
  if (!ok && statusCode != 201 && statusCode != 204) {
    LOG_ERRORF("SystemLink: Failed to create tag %s (%d): %s", path.c_str(), statusCode, responseBody.c_str());
    return false;
  }
  return true;
}

static bool systemLinkPutTagValue(const String &path, const char *type, const String &value) {
  DynamicJsonDocument doc(256);
  JsonObject valueObj = doc.createNestedObject("value");
  valueObj["type"] = type;
  valueObj["value"] = value;

  String body;
  serializeJson(doc, body);

  String responseBody;
  int statusCode = -1;
  String url = systemLinkBaseUrl("/nitag/v2/tags/") + systemLinkConfig.workspaceId + "/" + path + "/values/current";
  bool ok = systemLinkPutJson(url, body, responseBody, statusCode);
  if (!ok) {
    LOG_WARNF("SystemLink: Failed to update tag %s (%d)", path.c_str(), statusCode);
  }
  return ok;
}

static bool systemLinkEnsureRealtimeTags() {
  if (systemLinkTagsProvisioned) {
    return true;
  }

  bool ok = true;
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("chamberTemp"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("targetTemp"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("roastState"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("roastProgress"), "INT", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("lastFault"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("publishStatus"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("resetReason"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("bootCount"), "INT", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  systemLinkTagsProvisioned = ok;
  return ok;
}

static void systemLinkPublishRealtimeTags() {
  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!systemLinkEnsureRealtimeTags()) {
    return;
  }

  SystemLinkTelemetrySnapshot snapshot;
  portENTER_CRITICAL(&systemLinkLock);
  snapshot = systemLinkTelemetry;
  portEXIT_CRITICAL(&systemLinkLock);

  snapshot.active = systemLinkIsTrackedRoastState(roasterState);
  snapshot.state = roasterState;
  snapshot.chamberTempF = static_cast<float>(currentTemp);
  snapshot.targetTempF = static_cast<float>(setpointTemp);
  snapshot.roastProgress = setpointProgress;

  if (systemLinkShouldPublishChamberTemp(snapshot)) {
    systemLinkPutTagValue(systemLinkTagPath("chamberTemp"), "DOUBLE", String(snapshot.chamberTempF, 1));
  }

  if ((systemLinkIsTrackedRoastState(snapshot.state) || !systemLinkLastTelemetrySentValid ||
       systemLinkTenths(snapshot.targetTempF) != systemLinkTenths(systemLinkLastTelemetrySent.targetTempF)) &&
      systemLinkIsTrackedRoastState(snapshot.state)) {
    systemLinkPutTagValue(systemLinkTagPath("targetTemp"), "DOUBLE", String(snapshot.targetTempF, 1));
  }

  if (!systemLinkLastTelemetrySentValid || snapshot.state != systemLinkLastTelemetrySent.state) {
    systemLinkPutTagValue(systemLinkTagPath("roastState"), "STRING", String(systemLinkStateName(snapshot.state)));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.roastProgress != systemLinkLastTelemetrySent.roastProgress) {
    systemLinkPutTagValue(systemLinkTagPath("roastProgress"), "INT", String(snapshot.roastProgress));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.lastFault, systemLinkLastTelemetrySent.lastFault) != 0) {
    systemLinkPutTagValue(systemLinkTagPath("lastFault"), "STRING", String(snapshot.lastFault));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.publishStatus, systemLinkLastTelemetrySent.publishStatus) != 0) {
    systemLinkPutTagValue(systemLinkTagPath("publishStatus"), "STRING", String(snapshot.publishStatus));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.resetReason, systemLinkLastTelemetrySent.resetReason) != 0) {
    systemLinkPutTagValue(systemLinkTagPath("resetReason"), "STRING", String(snapshot.resetReason));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.bootCount != systemLinkLastTelemetrySent.bootCount) {
    systemLinkPutTagValue(systemLinkTagPath("bootCount"), "INT", String(snapshot.bootCount));
  }

  systemLinkLastTelemetrySent = snapshot;
  systemLinkLastTelemetrySentValid = true;
}

static void systemLinkTagTask(void *parameter) {
  (void)parameter;
  while (true) {
    if (systemLinkHasRequiredConfig()) {
      systemLinkPublishRealtimeTags();
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

static void initSystemLinkTagTask() {
  if (systemLinkTagTaskHandle != nullptr) {
    return;
  }

  xTaskCreatePinnedToCore(systemLinkTagTask,
                          "systemlink-tags",
                          8192,
                          nullptr,
                          1,
                          &systemLinkTagTaskHandle,
                          0);
}

static void processPendingSystemLinkPublish();

static void systemLinkPublishTask(void *parameter) {
  (void)parameter;
  while (true) {
    processPendingSystemLinkPublish();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

static void initSystemLinkPublishTask() {
  if (systemLinkPublishTaskHandle != nullptr) {
    return;
  }

  xTaskCreatePinnedToCore(systemLinkPublishTask,
                          "systemlink-publish",
                          12288,
                          nullptr,
                          1,
                          &systemLinkPublishTaskHandle,
                          0);
}

static void systemLinkSetSessionPhase(const char *phase) {
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkSession.active) {
    systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), String(phase));
  }
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkMarkRoastStarted() {
  String activeId = profileManager.getActiveProfileId();
  String activeName;
  profileManager.loadProfileMeta(activeId, activeName);

  portENTER_CRITICAL(&systemLinkLock);
  memset(&systemLinkSession, 0, sizeof(systemLinkSession));
  systemLinkSession.active = true;
  systemLinkSession.startedAtMs = millis();
  systemLinkSession.roastingStartedAtMs = 0;
  systemLinkSession.coolingStartedAtMs = 0;
  systemLinkSession.lastRecordedSecond = 0xFFFF;
  systemLinkSession.finalTargetTempF = profile.getFinalTargetTemp();
  systemLinkSession.setpointCount = profile.getSetpointCount();
  systemLinkSession.outcome = SYSTEMLINK_OUTCOME_NONE;
  systemLinkSession.kp = kp;
  systemLinkSession.ki = ki;
  systemLinkSession.kd = kd;
  systemLinkSession.finalTempOverrideF = finalTempOverride;
  systemLinkSession.recoveredAfterReset = false;
  systemLinkCopyString(systemLinkSession.profileId, sizeof(systemLinkSession.profileId), activeId);
  systemLinkCopyString(systemLinkSession.profileName, sizeof(systemLinkSession.profileName), activeName);
  systemLinkCopyString(systemLinkSession.outcomeReason, sizeof(systemLinkSession.outcomeReason), "in_progress");
  systemLinkSession.outcomePhase[0] = '\0';
  systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "starting");
  systemLinkCopyString(systemLinkSession.resetReason,
                       sizeof(systemLinkSession.resetReason),
                       systemLinkResetReasonName(esp_reset_reason()));
  systemLinkTelemetry.active = true;
  systemLinkTelemetry.state = roasterState;
  portEXIT_CRITICAL(&systemLinkLock);

  systemLinkInvalidateTagPublishState();
  systemLinkUpdateLastFault("none");
  systemLinkUpdatePublishStatus("starting");
  systemLinkPersistBreadcrumb(systemLinkSession, true, false, "starting");
}

static void systemLinkMarkRoastingPhaseStarted() {
  bool shouldPersist = false;
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkSession.active && systemLinkSession.roastingStartedAtMs == 0) {
    systemLinkSession.roastingStartedAtMs = millis();
    systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "roasting");
    shouldPersist = true;
  }
  portEXIT_CRITICAL(&systemLinkLock);

  if (shouldPersist) {
    systemLinkUpdatePublishStatus("roasting");
    systemLinkPersistBreadcrumb(systemLinkSession, true, false, "roasting");
  }
}

static void systemLinkMarkCoolingPhaseStarted(SystemLinkRoastOutcome outcome, const char *reason) {
  bool shouldPersist = false;
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkSession.active) {
    if (systemLinkSession.roastingStartedAtMs == 0) {
      systemLinkSession.roastingStartedAtMs = systemLinkSession.startedAtMs;
    }
    if (systemLinkSession.coolingStartedAtMs == 0) {
      systemLinkSession.coolingStartedAtMs = millis();
    }
    systemLinkAssignOutcome(systemLinkSession,
                            outcome,
                            reason,
                            systemLinkOutcomePhaseForCoolingStart(systemLinkSession, outcome, reason));
    systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "cooling");
    shouldPersist = true;
  }
  portEXIT_CRITICAL(&systemLinkLock);

  if (shouldPersist) {
    systemLinkUpdatePublishStatus("cooling");
    systemLinkPersistBreadcrumb(systemLinkSession, true, false, "cooling");
  }
}

static void systemLinkRecordRoastSample() {
  portENTER_CRITICAL(&systemLinkLock);
  if (!systemLinkSession.active || !systemLinkIsTrackedRoastState(roasterState)) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  uint16_t elapsedSeconds = static_cast<uint16_t>((millis() - systemLinkSession.startedAtMs) / 1000UL);
  if (elapsedSeconds == systemLinkSession.lastRecordedSecond) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  systemLinkSession.lastRecordedSecond = elapsedSeconds;
  if (systemLinkSession.sampleCount < SYSTEMLINK_MAX_TRACE_SAMPLES) {
    RoastTraceSample &sample = systemLinkSession.samples[systemLinkSession.sampleCount++];
    sample.elapsedSeconds = elapsedSeconds;
    sample.actualTenthsF = static_cast<int16_t>(lroundf(static_cast<float>(currentTemp) * 10.0f));
    sample.targetTenthsF = static_cast<int16_t>(lroundf(static_cast<float>(setpointTemp) * 10.0f));
    sample.heaterOutputTenths = static_cast<int16_t>(lroundf(static_cast<float>(heaterOutputVal) * 10.0f));
  } else {
    systemLinkSession.traceOverflow = true;
  }

  systemLinkTelemetry.active = true;
  systemLinkTelemetry.chamberTempF = static_cast<float>(currentTemp);
  systemLinkTelemetry.targetTempF = static_cast<float>(setpointTemp);
  systemLinkTelemetry.roastProgress = setpointProgress;
  systemLinkTelemetry.state = roasterState;
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkFinishRoast(SystemLinkRoastOutcome outcome, const char *reason) {
  bool persistPending = false;
  portENTER_CRITICAL(&systemLinkLock);
  if (!systemLinkSession.active) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  systemLinkSession.active = false;
  systemLinkSession.endedAtMs = millis();
  systemLinkAssignOutcome(systemLinkSession, outcome, reason, systemLinkSession.phase);
  systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "publish_pending");
  if (!systemLinkPublishPending && !systemLinkPublishInProgress) {
    memcpy(&systemLinkPublishSession, &systemLinkSession, sizeof(SystemLinkRoastSession));
    systemLinkPublishPending = true;
    persistPending = true;
  } else {
    LOG_WARN("SystemLink: Previous publish still active, dropping completed roast publish");
  }
  systemLinkTelemetry.active = false;
  systemLinkTelemetry.state = roasterState;
  portEXIT_CRITICAL(&systemLinkLock);

  if (outcome == SYSTEMLINK_OUTCOME_ERRORED) {
    systemLinkUpdateLastFault(reason);
  }
  systemLinkUpdatePublishStatus("pending");
  if (persistPending) {
    systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "publish_pending");
  }
}

static bool systemLinkParseCreatedResultId(const String &responseBody, String &resultId) {
  resultId = "";
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, responseBody)) {
    return false;
  }

  if (doc["results"][0]["id"].is<const char *>()) {
    resultId = doc["results"][0]["id"].as<String>();
    return true;
  }
  if (doc[0]["id"].is<const char *>()) {
    resultId = doc[0]["id"].as<String>();
    return true;
  }
  if (doc["id"].is<const char *>()) {
    resultId = doc["id"].as<String>();
    return true;
  }
  return false;
}

static String systemLinkPhaseStatusType(const SystemLinkRoastSession &session, const char *phaseName) {
  if (session.outcome != SYSTEMLINK_OUTCOME_NONE && strcmp(session.outcomePhase, phaseName) == 0) {
    return systemLinkStatusTypeName(session.outcome);
  }
  return "DONE";
}

static bool systemLinkCreatePhaseSteps(const String &resultId, const SystemLinkRoastSession &session) {
  if (resultId.length() == 0) {
    return false;
  }

  DynamicJsonDocument doc(3072);
  JsonArray steps = doc.createNestedArray("steps");

  struct StepSpec {
    const char *name;
    const char *phaseName;
    const char *stepType;
    double seconds;
    const char *detail;
  } stepSpecs[] = {
    {"Start Roast", "starting", "Action", systemLinkStartPhaseSeconds(session), "Fan ramp and roast start preparation"},
    {"Roasting", "roasting", "Action", systemLinkRoastingPhaseSeconds(session), "Profile-driven roast control"},
    {"Cooling", "cooling", "Action", systemLinkCoolingPhaseSeconds(session), "Cooling cycle until safe end temperature"}
  };

  for (size_t index = 0; index < sizeof(stepSpecs) / sizeof(stepSpecs[0]); index++) {
    if (stepSpecs[index].seconds <= 0.0) {
      continue;
    }

    JsonObject step = steps.createNestedObject();
    step["resultId"] = resultId;
    step["name"] = stepSpecs[index].name;
    step["stepType"] = stepSpecs[index].stepType;
    step["totalTimeInSeconds"] = stepSpecs[index].seconds;
    JsonObject status = step.createNestedObject("status");
    String statusType = systemLinkPhaseStatusType(session, stepSpecs[index].phaseName);
    status["statusType"] = statusType;
    status["statusName"] = statusType == "DONE" ? "Done" : systemLinkStatusDisplayName(session.outcome);
    JsonObject data = step.createNestedObject("data");
    data["text"] = stepSpecs[index].detail;
    if (strcmp(stepSpecs[index].phaseName, "starting") == 0) {
      JsonArray parameters = data.createNestedArray("parameters");
      JsonObject setpoints = parameters.createNestedObject();
      setpoints["nitmParameterType"] = "ADDITIONAL_RESULTS";
      setpoints["name"] = "profileSetpointsJson";
      setpoints["value"] = systemLinkProfileSetpointsJson(session.profileId);
    }
  }

  if (steps.size() == 0) {
    return true;
  }

  String body;
  serializeJson(doc, body);

  String responseBody;
  int statusCode = -1;
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/steps"), body, responseBody, statusCode);
  if (!ok && statusCode != 201 && statusCode != 200) {
    LOG_ERRORF("SystemLink: Step publish failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }

  LOG_INFOF("SystemLink: Published %u roast phase steps", static_cast<unsigned>(steps.size()));
  return true;
}

static bool systemLinkCreateResult(SystemLinkRoastSession &session, const String &fileId, String &resultId) {
  DynamicJsonDocument doc(4096);
  JsonObject result = doc.createNestedArray("results").createNestedObject();

  result["programName"] = session.profileName[0] != '\0' ? String("Coffee Roaster - ") + session.profileName : "Coffee Roaster Roast";
  JsonObject status = result.createNestedObject("status");
  status["statusType"] = systemLinkStatusTypeName(session.outcome);
  status["statusName"] = systemLinkStatusDisplayName(session.outcome);
  result["systemId"] = systemLinkConfig.systemId;
  result["hostName"] = WiFi.getHostname();
  result["partNumber"] = "coffee-roaster";
  result["operator"] = "coffee-roaster";
  result["totalTimeInSeconds"] = static_cast<double>(session.endedAtMs - session.startedAtMs) / 1000.0;
  result["workspace"] = systemLinkConfig.workspaceId;

  JsonArray keywords = result.createNestedArray("keywords");
  keywords.add("coffee-roaster");
  keywords.add("roast");
  if (session.profileId[0] != '\0') {
    keywords.add(session.profileId);
  }

  JsonObject properties = result.createNestedObject("properties");
  properties["profileId"] = session.profileId;
  properties["profileName"] = session.profileName;
  properties["profileJson"] = profileManager.getProfile(session.profileId);
  properties["setpointCount"] = String(session.setpointCount);
  properties["finalTargetTempF"] = String(session.finalTargetTempF);
  properties["kp"] = String(session.kp, 4);
  properties["ki"] = String(session.ki, 4);
  properties["kd"] = String(session.kd, 4);
  properties["finalTempOverrideF"] = String(session.finalTempOverrideF);
  properties["sampleRateHz"] = "1";
  properties["sampleCount"] = String(session.sampleCount);
  properties["traceOverflow"] = session.traceOverflow ? "true" : "false";
  properties["outcomeReason"] = session.outcomeReason;
  properties["phase"] = session.phase;
  properties["startPhaseSeconds"] = String(systemLinkStartPhaseSeconds(session), 3);
  properties["roastingPhaseSeconds"] = String(systemLinkRoastingPhaseSeconds(session), 3);
  properties["coolingPhaseSeconds"] = String(systemLinkCoolingPhaseSeconds(session), 3);
  properties["resetReason"] = session.resetReason;
  properties["recoveredAfterReset"] = session.recoveredAfterReset ? "true" : "false";

  if (fileId.length() > 0) {
    JsonArray fileIds = result.createNestedArray("fileIds");
    fileIds.add(fileId);
  } else {
    properties["traceUploadError"] = "csv_upload_failed";
  }

  String body;
  serializeJson(doc, body);

  String responseBody;
  int statusCode = -1;
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/results"), body, responseBody, statusCode);
  if (!ok && statusCode != 201) {
    LOG_ERRORF("SystemLink: Result publish failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }

  if (!systemLinkParseCreatedResultId(responseBody, resultId)) {
    LOG_WARNF("SystemLink: Result published but result ID was not found in response: %s", responseBody.c_str());
  }

  LOG_INFOF("SystemLink: Published roast result (%s)", systemLinkStatusTypeName(session.outcome).c_str());
  return true;
}

static void processPendingSystemLinkPublish() {
  if (millis() - systemLinkLastPublishAttemptMs < 15000UL) {
    return;
  }
  bool shouldPublish = false;

  portENTER_CRITICAL(&systemLinkLock);
  shouldPublish = systemLinkPublishPending && !systemLinkPublishInProgress;
  if (shouldPublish) {
    systemLinkPublishInProgress = true;
  }
  portEXIT_CRITICAL(&systemLinkLock);

  if (!shouldPublish) {
    return;
  }

  systemLinkLastPublishAttemptMs = millis();

  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED) {
    portENTER_CRITICAL(&systemLinkLock);
    systemLinkPublishInProgress = false;
    portEXIT_CRITICAL(&systemLinkLock);
    systemLinkUpdatePublishStatus("waiting_for_network");
    return;
  }

  String uploadUri;
  String fileId;
  String filename = String("roast-") + (systemLinkPublishSession.profileId[0] != '\0' ? systemLinkPublishSession.profileId : "session") + ".csv";
  systemLinkUpdatePublishStatus("uploading_csv");
  systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "uploading_csv");
  if (systemLinkUploadTraceFile(filename, "text/csv", systemLinkPublishSession, uploadUri)) {
    fileId = systemLinkExtractIdFromUri(uploadUri);
  } else {
    systemLinkUpdatePublishStatus("csv_upload_failed");
  }

  systemLinkUpdatePublishStatus("creating_result");
  systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "creating_result");
  String resultId;
  bool published = systemLinkCreateResult(systemLinkPublishSession, fileId, resultId);
  bool stepsPublished = true;
  if (published && resultId.length() > 0) {
    stepsPublished = systemLinkCreatePhaseSteps(resultId, systemLinkPublishSession);
  }

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkPublishInProgress = false;
  if (published) {
    systemLinkPublishPending = false;
    memset(&systemLinkPublishSession, 0, sizeof(SystemLinkRoastSession));
  }
  portEXIT_CRITICAL(&systemLinkLock);

  if (published) {
    systemLinkUpdatePublishStatus(stepsPublished ? "published" : "published_steps_failed");
    systemLinkClearBreadcrumb();
  } else {
    systemLinkUpdatePublishStatus("result_publish_failed");
  }
}

#endif // SYSTEMLINK_HPP