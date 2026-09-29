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
#include <functional>
#include <mutex>
#include "DebugLog.hpp"
#include "ProfileManager.hpp"
#include "StepResponseTuner.hpp"
#include "Types.hpp"

extern Preferences preferences;
extern ProfileManager profileManager;
extern Profiles profile;
extern StepResponseTuner stepTuner;
extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern double fanTemp;
extern double heaterOutputVal;
extern double heaterPidTrimVal;
extern double heaterFeedforwardVal;
extern int setpointProgress;
extern RoasterState roasterState;
extern double kp;
extern double ki;
extern double kd;
extern double appliedKp;
extern double appliedKi;
extern double appliedKd;
extern bool pidScheduleConfigured;
extern bool pidScheduleActive;
extern int activePidBandIndex;
extern int finalTempOverride;

static const char *SYSTEMLINK_API_URL_KEY = "sl_api_url";
static const char *SYSTEMLINK_API_KEY_KEY = "sl_api_key";
static const char *SYSTEMLINK_ENABLED_KEY = "sl_enabled";
static const char *SYSTEMLINK_INSECURE_TLS_KEY = "sl_insecure";
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
static const char *SYSTEMLINK_BC_OUTCOME_KEY = "sl_outcome";
static const char *SYSTEMLINK_BC_FILE_ID_KEY = "sl_fileid";
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
static const size_t SYSTEMLINK_FILE_ID_MAX = 64;
static const uint8_t SYSTEMLINK_MAX_PUBLISH_ATTEMPTS = 5;
static const uint32_t SYSTEMLINK_HTTP_CONNECT_TIMEOUT_MS = 5000;
static const uint32_t SYSTEMLINK_HTTP_TIMEOUT_MS = 10000;
static const size_t SYSTEMLINK_RECENT_LOG_MAX_CHARS = 4000;
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
  // Skip certificate validation, for on-prem servers with a private CA.
  bool insecureTls;
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
  int16_t fanTempTenthsF;
  int16_t fanOutputTenths;
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
  bool pidScheduleConfigured;
  bool recoveredAfterReset;
  uint8_t publishAttempts;
  char fileId[SYSTEMLINK_FILE_ID_MAX];
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
  "",
  false
};
static SystemLinkTelemetrySnapshot systemLinkTelemetry = {false, 0.0f, 0.0f, 0, IDLE, "none", "idle", "unknown", 0};
static SystemLinkRoastSession systemLinkSession = {};
static SystemLinkRoastSession systemLinkPublishSession = {};
static bool systemLinkPublishPending = false;
static bool systemLinkPublishInProgress = false;
static uint32_t systemLinkLastPublishAttemptMs = 0;
static bool systemLinkTagsProvisioned = false;
static uint32_t systemLinkLastIdleChamberPublishMs = 0;
static uint32_t systemLinkTagRetryAfterMs = 0;
static bool systemLinkLastTelemetrySentValid = false;
static SystemLinkTelemetrySnapshot systemLinkLastTelemetrySent = {false, 0.0f, 0.0f, 0, IDLE, "", "", "", 0};
static uint32_t systemLinkLastErrorSequencePublished = 0;
static volatile bool systemLinkCalibrationPublishPending = false;
// Serializes TLS traffic: halves peak heap use and protects the global CA bundle state.
static std::mutex systemLinkNetMutex;
// Kept open between tag writes so each update doesn't pay for a TLS handshake.
static WiFiClientSecure systemLinkTagClient;
static volatile bool systemLinkTagClientNeedsReset = true;

extern const uint8_t systemLinkCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t systemLinkCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

static bool systemLinkIsTrackedRoastState(RoasterState state);
static void systemLinkCopyString(char *dest, size_t destSize, const char *src);
static void systemLinkCopyString(char *dest, size_t destSize, const String &src);

static void systemLinkInvalidateTagPublishState() {
  systemLinkTagsProvisioned = false;
  systemLinkLastIdleChamberPublishMs = 0;
  systemLinkTagRetryAfterMs = 0;
  systemLinkLastTelemetrySentValid = false;
  systemLinkLastErrorSequencePublished = 0;
  systemLinkTagClientNeedsReset = true;
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
    systemLinkCopyString(session.outcomeReason, sizeof(session.outcomeReason), reason);
  }
  if (outcomePhase != nullptr && outcomePhase[0] != '\0') {
    systemLinkCopyString(session.outcomePhase, sizeof(session.outcomePhase), outcomePhase);
  }
}

static bool systemLinkIsLiveState(RoasterState state) {
  return systemLinkIsTrackedRoastState(state) || state == ERROR || state == CALIBRATING;
}

static bool systemLinkShouldPublishChamberTemp(const SystemLinkTelemetrySnapshot &snapshot) {
  if (systemLinkIsLiveState(snapshot.state)) {
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
  if (reason != nullptr && strcmp(reason, "roast_timeout") == 0) {
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

static void systemLinkCopyString(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) {
    return;
  }
  // No heap allocation: several callers hold the systemLinkLock spinlock.
  strlcpy(dest, src != nullptr ? src : "", destSize);
}

static void systemLinkCopyString(char *dest, size_t destSize, const String &src) {
  systemLinkCopyString(dest, destSize, src.c_str());
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
  preferences.putUChar(SYSTEMLINK_BC_OUTCOME_KEY, static_cast<uint8_t>(session.outcome));
  preferences.putString(SYSTEMLINK_BC_FILE_ID_KEY, session.fileId);
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
  preferences.remove(SYSTEMLINK_BC_OUTCOME_KEY);
  preferences.remove(SYSTEMLINK_BC_FILE_ID_KEY);
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
  String reason;
  if (hadActive) {
    // Reset mid-roast: the interruption itself is the outcome.
    reason = String("reset_during_") + phase + ":" + resetReason;
    systemLinkUpdateLastFault(reason);
  } else {
    // Roast had finished and was only waiting to publish; keep its real outcome.
    uint8_t storedOutcome = preferences.getUChar(SYSTEMLINK_BC_OUTCOME_KEY, SYSTEMLINK_OUTCOME_ERRORED);
    if (storedOutcome <= SYSTEMLINK_OUTCOME_ERRORED) {
      systemLinkPublishSession.outcome = static_cast<SystemLinkRoastOutcome>(storedOutcome);
    }
    reason = preferences.getString(SYSTEMLINK_BC_REASON_KEY, "");
    if (reason.length() == 0) {
      reason = String("reset_during_") + phase + ":" + resetReason;
    }
    systemLinkCopyString(systemLinkPublishSession.fileId,
                         sizeof(systemLinkPublishSession.fileId),
                         preferences.getString(SYSTEMLINK_BC_FILE_ID_KEY, ""));
  }
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
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkPublishPending = true;
  portEXIT_CRITICAL(&systemLinkLock);
  systemLinkUpdatePublishStatus("recovery_pending");
  LOG_WARNF("SystemLink: Recovery publish queued after %s reset (%s)", resetReason.c_str(), reason.c_str());
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

static void systemLinkConfigureTls(WiFiClientSecure &client) {
  if (systemLinkConfig.insecureTls) {
    client.setInsecure();
  } else {
    client.setCACertBundle(systemLinkCaBundleStart, systemLinkCaBundleEnd - systemLinkCaBundleStart);
  }
  client.setTimeout(SYSTEMLINK_HTTP_TIMEOUT_MS);  // Stream timeout is in milliseconds.
}

static bool systemLinkResponseContainsError(const String &responseBody, String &errorMessage);

// Short human-readable reason for a failed request, suitable for logs and tags.
static String systemLinkDescribeFailure(int statusCode, const String &responseBody, WiFiClientSecure *client) {
  if (statusCode < 0) {
    String detail = HTTPClient::errorToString(statusCode);
    char tlsError[96] = {0};
    if (client != nullptr && client->lastError(tlsError, sizeof(tlsError)) != 0 && tlsError[0] != '\0') {
      detail += String(" / TLS: ") + tlsError;
    }
    return detail;
  }
  String apiError;
  if (systemLinkResponseContainsError(responseBody, apiError)) {
    return apiError;
  }
  return responseBody.substring(0, 100);
}

static String systemLinkUrlPath(const String &url) {
  int schemeEnd = url.indexOf("://");
  int pathStart = url.indexOf('/', schemeEnd >= 0 ? schemeEnd + 3 : 0);
  return pathStart >= 0 ? url.substring(pathStart) : url;
}

static bool systemLinkHttpRequest(const String &method,
                                  const String &url,
                                  const String &contentType,
                                  const uint8_t *payload,
                                  size_t payloadLength,
                                  String &responseBody,
                                  int &statusCode,
                                  WiFiClientSecure *reuseClient = nullptr) {
  responseBody = "";
  statusCode = -1;

  std::lock_guard<std::mutex> guard(systemLinkNetMutex);

  WiFiClientSecure localClient;
  WiFiClientSecure &client = reuseClient != nullptr ? *reuseClient : localClient;
  if (reuseClient == nullptr) {
    systemLinkConfigureTls(localClient);
  }

  HTTPClient http;
  http.setReuse(reuseClient != nullptr);
  http.setConnectTimeout(SYSTEMLINK_HTTP_CONNECT_TIMEOUT_MS);
  http.setTimeout(SYSTEMLINK_HTTP_TIMEOUT_MS);

  if (!http.begin(client, url)) {
    LOG_ERRORF("SystemLink: Failed to open %s", url.c_str());
    return false;
  }

  http.addHeader("accept", "application/json");
  http.addHeader("x-ni-api-key", systemLinkConfig.apiKey);
  if (contentType.length() > 0) {
    http.addHeader("Content-Type", contentType);
  }

  if (method == "POST") {
    statusCode = http.POST(const_cast<uint8_t *>(payload), payloadLength);
  } else if (method == "PUT") {
    statusCode = http.PUT(const_cast<uint8_t *>(payload), payloadLength);
  } else {
    statusCode = http.sendRequest(method.c_str(), const_cast<uint8_t *>(payload), payloadLength);
  }
  if (statusCode > 0) {
    responseBody = http.getString();
  }
  http.end();

  bool ok = statusCode >= 200 && statusCode < 300;
  if (!ok) {
    // WARN, not ERROR: callers log the ERROR with context, which feeds the lastError tag.
    LOG_WARNF("SystemLink: %s %s -> %d %s",
              method.c_str(),
              systemLinkUrlPath(url).substring(0, 48).c_str(),
              statusCode,
              systemLinkDescribeFailure(statusCode, responseBody, &client).c_str());
    if (reuseClient != nullptr) {
      reuseClient->stop();
    }
  }
  return ok;
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
                               int &statusCode,
                               WiFiClientSecure *reuseClient = nullptr) {
  return systemLinkHttpRequest("POST",
                               url,
                               "application/json",
                               reinterpret_cast<const uint8_t *>(jsonBody.c_str()),
                               jsonBody.length(),
                               responseBody,
                               statusCode,
                               reuseClient);
}

static bool systemLinkPutJson(const String &url,
                              const String &jsonBody,
                              String &responseBody,
                              int &statusCode,
                              WiFiClientSecure *reuseClient = nullptr) {
  return systemLinkHttpRequest("PUT",
                               url,
                               "application/json",
                               reinterpret_cast<const uint8_t *>(jsonBody.c_str()),
                               jsonBody.length(),
                               responseBody,
                               statusCode,
                               reuseClient);
}

static bool systemLinkParseCreatedEntityId(const String &responseBody, String &entityId) {
  entityId = "";
  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, responseBody)) {
    return false;
  }

  if (doc["id"].is<const char *>()) {
    entityId = doc["id"].as<String>();
    return true;
  }
  if (doc["table"]["id"].is<const char *>()) {
    entityId = doc["table"]["id"].as<String>();
    return true;
  }
  if (doc[0]["id"].is<const char *>()) {
    entityId = doc[0]["id"].as<String>();
    return true;
  }
  return false;
}

static const char *SYSTEMLINK_TRACE_CSV_HEADER = "elapsedSeconds,actualTempF,targetTempF,heaterOutput,fanTempF,fanOutput\n";

static int systemLinkFormatCsvRow(const RoastTraceSample &sample, char *row, size_t rowSize) {
  return snprintf(row,
                  rowSize,
                  "%u,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                  static_cast<unsigned>(sample.elapsedSeconds),
                  sample.actualTenthsF / 10.0f,
                  sample.targetTenthsF / 10.0f,
                  sample.heaterOutputTenths / 10.0f,
                  sample.fanTempTenthsF / 10.0f,
                  sample.fanOutputTenths / 10.0f);
}

static size_t systemLinkCsvLength(const SystemLinkRoastSession &session) {
  size_t total = strlen(SYSTEMLINK_TRACE_CSV_HEADER);
  char row[96];

  for (uint16_t index = 0; index < session.sampleCount; index++) {
    int rowLen = systemLinkFormatCsvRow(session.samples[index], row, sizeof(row));
    if (rowLen > 0) {
      total += static_cast<size_t>(rowLen);
    }
  }

  return total;
}

static bool systemLinkResponseContainsError(const String &responseBody, String &errorMessage) {
  errorMessage = "";
  if (responseBody.length() == 0) {
    return false;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, responseBody)) {
    return false;
  }

  JsonVariant error = doc["error"];
  if (!error.is<JsonObject>()) {
    return false;
  }

  if (error["message"].is<const char *>()) {
    errorMessage = error["message"].as<String>();
  } else if (error["name"].is<const char *>()) {
    errorMessage = error["name"].as<String>();
  } else {
    errorMessage = "unknown_error";
  }
  return true;
}

// Reads status, headers and body from a raw HTTP/1.1 response, including chunked bodies.
static bool systemLinkReadHttpResponse(WiFiClientSecure &client, int &statusCode, String &body) {
  statusCode = -1;
  body = "";

  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && millis() - waitStart < SYSTEMLINK_HTTP_TIMEOUT_MS) {
    delay(10);
  }
  if (!client.available()) {
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace >= 0 && statusLine.length() >= static_cast<unsigned>(firstSpace + 4)) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }

  bool chunked = false;
  while (client.available() || client.connected()) {
    String headerLine = client.readStringUntil('\n');
    headerLine.trim();
    if (headerLine.length() == 0) {
      break;
    }
    headerLine.toLowerCase();
    if (headerLine.startsWith("transfer-encoding:") && headerLine.indexOf("chunked") >= 0) {
      chunked = true;
    }
  }

  if (!chunked) {
    body = client.readString();
    return true;
  }

  char buffer[128];
  while (client.available() || client.connected()) {
    String sizeLine = client.readStringUntil('\n');
    sizeLine.trim();
    long remaining = strtol(sizeLine.c_str(), nullptr, 16);
    if (remaining <= 0) {
      break;
    }
    while (remaining > 0) {
      size_t readLen = client.readBytes(buffer, min(static_cast<long>(sizeof(buffer)), remaining));
      if (readLen == 0) {
        return false;
      }
      body.concat(buffer, readLen);
      remaining -= static_cast<long>(readLen);
    }
    client.readStringUntil('\n');
  }
  return true;
}

// Streams a multipart file upload to the File service. writeBody must write exactly bodyLength bytes.
static bool systemLinkUploadMultipart(const String &filename,
                                      const String &contentType,
                                      size_t bodyLength,
                                      const std::function<bool(WiFiClientSecure &)> &writeBody,
                                      String &uploadedUri) {
  uploadedUri = "";

  String host;
  uint16_t port;
  if (!systemLinkParseApiEndpoint(host, port)) {
    LOG_ERROR("SystemLink: Invalid API URL configuration");
    return false;
  }

  const String boundary = "----CoffeeRoasterSystemLinkBoundary";
  String prefix;
  prefix.reserve(filename.length() + contentType.length() + boundary.length() + 128);
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
  prefix += "Content-Type: " + contentType + "\r\n\r\n";

  String suffix = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = prefix.length() + bodyLength + suffix.length();

  std::lock_guard<std::mutex> guard(systemLinkNetMutex);

  WiFiClientSecure client;
  systemLinkConfigureTls(client);
  if (!client.connect(host.c_str(), port, static_cast<int32_t>(SYSTEMLINK_HTTP_CONNECT_TIMEOUT_MS))) {
    char tlsError[96] = {0};
    client.lastError(tlsError, sizeof(tlsError));
    LOG_ERRORF("SystemLink: Upload connect to %s:%u failed: %s", host.c_str(), static_cast<unsigned>(port), tlsError);
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
  if (!writeBody(client)) {
    client.stop();
    return false;
  }
  client.print(suffix);
  client.flush();

  int statusCode = -1;
  String responseBody;
  bool gotResponse = systemLinkReadHttpResponse(client, statusCode, responseBody);
  client.stop();
  if (!gotResponse) {
    LOG_ERRORF("SystemLink: Upload of %s timed out waiting for response", filename.c_str());
    return false;
  }
  if (statusCode < 200 || statusCode >= 300) {
    LOG_ERRORF("SystemLink: Upload of %s failed (%d): %s",
               filename.c_str(),
               statusCode,
               systemLinkDescribeFailure(statusCode, responseBody, nullptr).c_str());
    return false;
  }

  JsonDocument doc;
  if (deserializeJson(doc, responseBody)) {
    LOG_ERRORF("SystemLink: Upload response not JSON: %s", responseBody.substring(0, 100).c_str());
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

  LOG_ERRORF("SystemLink: Upload response missing uri: %s", responseBody.substring(0, 100).c_str());
  return false;
}

static bool systemLinkUploadTraceFile(const String &filename, const SystemLinkRoastSession &session, String &uploadedUri) {
  return systemLinkUploadMultipart(filename, "text/csv", systemLinkCsvLength(session), [&session](WiFiClientSecure &client) {
    client.print(SYSTEMLINK_TRACE_CSV_HEADER);
    char row[96];
    for (uint16_t index = 0; index < session.sampleCount; index++) {
      int rowLen = systemLinkFormatCsvRow(session.samples[index], row, sizeof(row));
      if (rowLen <= 0 || rowLen >= static_cast<int>(sizeof(row))) {
        LOG_ERROR("SystemLink: Failed to format CSV row");
        return false;
      }
      client.write(reinterpret_cast<const uint8_t *>(row), static_cast<size_t>(rowLen));
    }
    return true;
  }, uploadedUri);
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
  // Read NVS before taking the spinlock: flash access with interrupts disabled can hang the other core.
  bool enabled = preferences.getBool(SYSTEMLINK_ENABLED_KEY, false);
  bool insecureTls = preferences.getBool(SYSTEMLINK_INSECURE_TLS_KEY, false);
  String apiUrl = preferences.getString(SYSTEMLINK_API_URL_KEY, "https://dev-api.lifecyclesolutions.ni.com");
  String workspaceId = preferences.getString(SYSTEMLINK_WORKSPACE_KEY, "");
  String systemId = preferences.getString(SYSTEMLINK_SYSTEM_ID_KEY, "");
  String apiKey = preferences.getString(SYSTEMLINK_API_KEY_KEY, "");
  String lastFault = preferences.getString(SYSTEMLINK_LAST_FAULT_KEY, "none");
  String publishStatus = preferences.getString(SYSTEMLINK_LAST_PUB_STATUS_KEY, "idle");

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkConfig.enabled = enabled;
  systemLinkConfig.insecureTls = insecureTls;
  systemLinkCopyString(systemLinkConfig.apiUrl, sizeof(systemLinkConfig.apiUrl), apiUrl);
  systemLinkCopyString(systemLinkConfig.workspaceId, sizeof(systemLinkConfig.workspaceId), workspaceId);
  systemLinkCopyString(systemLinkConfig.systemId, sizeof(systemLinkConfig.systemId), systemId);
  systemLinkCopyString(systemLinkConfig.apiKey, sizeof(systemLinkConfig.apiKey), apiKey);
  systemLinkCopyString(systemLinkTelemetry.lastFault, sizeof(systemLinkTelemetry.lastFault), lastFault);
  systemLinkCopyString(systemLinkTelemetry.publishStatus, sizeof(systemLinkTelemetry.publishStatus), publishStatus);
  portEXIT_CRITICAL(&systemLinkLock);
}

static void saveSystemLinkConfig() {
  preferences.putBool(SYSTEMLINK_ENABLED_KEY, systemLinkConfig.enabled);
  preferences.putBool(SYSTEMLINK_INSECURE_TLS_KEY, systemLinkConfig.insecureTls);
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
  SystemLinkConfig config;
  portENTER_CRITICAL(&systemLinkLock);
  config = systemLinkConfig;
  portEXIT_CRITICAL(&systemLinkLock);

  JsonDocument doc;
  doc["enabled"] = config.enabled;
  doc["apiUrl"] = config.apiUrl;
  doc["workspaceId"] = config.workspaceId;
  doc["systemId"] = config.systemId;
  doc["insecureTls"] = config.insecureTls;
  doc["hasApiKey"] = config.apiKey[0] != '\0';
  doc["apiKeyMasked"] = systemLinkMaskedKey();
  String json;
  serializeJson(doc, json);
  return json;
}

static bool updateSystemLinkConfigFromJSON(const String &body, String &errorMessage) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, body);
  if (err) {
    errorMessage = "invalid_json";
    return false;
  }

  // Build the new config outside the spinlock (String/JSON access allocates).
  SystemLinkConfig updated;
  portENTER_CRITICAL(&systemLinkLock);
  updated = systemLinkConfig;
  portEXIT_CRITICAL(&systemLinkLock);

  if (doc["enabled"].is<bool>()) {
    updated.enabled = doc["enabled"].as<bool>();
  }
  if (doc["insecureTls"].is<bool>()) {
    updated.insecureTls = doc["insecureTls"].as<bool>();
  }
  bool urlChanged = false;
  if (doc["apiUrl"].is<const char *>()) {
    String apiUrl = doc["apiUrl"].as<String>();
    apiUrl.trim();
    urlChanged = apiUrl != updated.apiUrl;
    systemLinkCopyString(updated.apiUrl, sizeof(updated.apiUrl), apiUrl);
  }
  if (doc["workspaceId"].is<const char *>()) {
    systemLinkCopyString(updated.workspaceId, sizeof(updated.workspaceId), doc["workspaceId"].as<String>());
  }
  if (doc["systemId"].is<const char *>()) {
    systemLinkCopyString(updated.systemId, sizeof(updated.systemId), doc["systemId"].as<String>());
  }
  String newApiKey = doc["apiKey"].is<const char *>() ? doc["apiKey"].as<String>() : String();
  newApiKey.trim();
  if (doc["clearApiKey"].as<bool>()) {
    updated.apiKey[0] = '\0';
  } else if (newApiKey.length() > 0) {
    systemLinkCopyString(updated.apiKey, sizeof(updated.apiKey), newApiKey);
  } else if (urlChanged) {
    // Never send the stored key to a new host it wasn't entered for.
    updated.apiKey[0] = '\0';
    LOG_WARN("SystemLink: API URL changed without a new API key - stored key cleared");
  }

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkConfig = updated;
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
  if (!systemLinkPostJson(systemLinkBaseUrl("/nitag/v2/tags"), body, responseBody, statusCode, &systemLinkTagClient)) {
    LOG_ERRORF("SystemLink: Failed to create tag %s (%d)", path.c_str(), statusCode);
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
  bool ok = systemLinkPutJson(url, body, responseBody, statusCode, &systemLinkTagClient);
  if (!ok) {
    systemLinkTagRetryAfterMs = millis() + 30000UL;
    if (statusCode == 404) {
      // Tag was deleted server-side; recreate it on the next attempt.
      systemLinkTagsProvisioned = false;
    }
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
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("lastError"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  systemLinkTagsProvisioned = ok;
  return ok;
}

static void systemLinkPublishRealtimeTags() {
  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (systemLinkTagRetryAfterMs != 0 && static_cast<int32_t>(millis() - systemLinkTagRetryAfterMs) < 0) {
    return;
  }

  if (!systemLinkEnsureRealtimeTags()) {
    systemLinkTagRetryAfterMs = millis() + 30000UL;
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

  // Stop at the first failure and leave systemLinkLastTelemetrySent untouched so unsent values are retried.
  bool ok = true;
  auto put = [&ok](const char *suffix, const char *type, const String &value) {
    if (ok) {
      ok = systemLinkPutTagValue(systemLinkTagPath(suffix), type, value);
    }
  };

  if (systemLinkShouldPublishChamberTemp(snapshot)) {
    put("chamberTemp", "DOUBLE", String(snapshot.chamberTempF, 1));
  }
  if (systemLinkIsLiveState(snapshot.state)) {
    put("targetTemp", "DOUBLE", String(snapshot.targetTempF, 1));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.state != systemLinkLastTelemetrySent.state) {
    put("roastState", "STRING", String(systemLinkStateName(snapshot.state)));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.roastProgress != systemLinkLastTelemetrySent.roastProgress) {
    put("roastProgress", "INT", String(snapshot.roastProgress));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.lastFault, systemLinkLastTelemetrySent.lastFault) != 0) {
    put("lastFault", "STRING", String(snapshot.lastFault));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.publishStatus, systemLinkLastTelemetrySent.publishStatus) != 0) {
    put("publishStatus", "STRING", String(snapshot.publishStatus));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.resetReason, systemLinkLastTelemetrySent.resetReason) != 0) {
    put("resetReason", "STRING", String(snapshot.resetReason));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.bootCount != systemLinkLastTelemetrySent.bootCount) {
    put("bootCount", "INT", String(snapshot.bootCount));
  }

  LogEntry lastError;
  uint32_t errorSequence = debugLogger.getLastError(lastError);
  if (errorSequence != 0 && errorSequence != systemLinkLastErrorSequencePublished) {
    put("lastError", "STRING", String("t=") + lastError.timestamp + "ms " + lastError.message);
    if (ok) {
      systemLinkLastErrorSequencePublished = errorSequence;
    }
  }

  if (ok) {
    systemLinkLastTelemetrySent = snapshot;
    systemLinkLastTelemetrySentValid = true;
  }
}

static void systemLinkTagTask(void *parameter) {
  (void)parameter;
  while (true) {
    if (systemLinkTagClientNeedsReset) {
      systemLinkTagClientNeedsReset = false;
      std::lock_guard<std::mutex> guard(systemLinkNetMutex);
      systemLinkTagClient.stop();
      systemLinkConfigureTls(systemLinkTagClient);
    }
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
                          12288,
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
                          16384,
                          nullptr,
                          1,
                          &systemLinkPublishTaskHandle,
                          0);
}

static void systemLinkSetSessionPhase(const char *phase) {
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkSession.active) {
    systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), phase);
  }
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkMarkRoastStarted() {
  if (!systemLinkHasRequiredConfig()) {
    // Otherwise breadcrumbs pile up and a stale roast is published once SystemLink is configured.
    portENTER_CRITICAL(&systemLinkLock);
    systemLinkSession.active = false;
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

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
  systemLinkSession.pidScheduleConfigured = pidScheduleConfigured;
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
    sample.fanTempTenthsF = static_cast<int16_t>(lroundf(static_cast<float>(fanTemp) * 10.0f));
    sample.fanOutputTenths = static_cast<int16_t>(lroundf(static_cast<float>(setpointFanSpeed) * 10.0f));
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
  bool droppedCurrent = false;
  bool replacedPending = false;
  portENTER_CRITICAL(&systemLinkLock);
  if (!systemLinkSession.active) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  systemLinkSession.active = false;
  systemLinkSession.endedAtMs = millis();
  systemLinkAssignOutcome(systemLinkSession, outcome, reason, systemLinkSession.phase);
  systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "publish_pending");
  if (systemLinkPublishInProgress) {
    // The publish task is reading systemLinkPublishSession; it can't be overwritten.
    droppedCurrent = true;
  } else {
    // A still-pending older roast has already failed to publish; the newest roast is more useful.
    replacedPending = systemLinkPublishPending;
    memcpy(&systemLinkPublishSession, &systemLinkSession, sizeof(SystemLinkRoastSession));
    systemLinkPublishPending = true;
    persistPending = true;
  }
  systemLinkTelemetry.active = false;
  systemLinkTelemetry.state = roasterState;
  portEXIT_CRITICAL(&systemLinkLock);

  if (droppedCurrent) {
    LOG_ERRORF("SystemLink: Publish in progress, roast result dropped (outcome reason %s)", reason);
  } else if (replacedPending) {
    LOG_ERROR("SystemLink: Unpublished previous roast replaced by newer roast");
  }
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
  return systemLinkParseCreatedEntityId(responseBody, resultId);
}

static String systemLinkPhaseStatusType(const SystemLinkRoastSession &session, const char *phaseName) {
  if (session.outcome != SYSTEMLINK_OUTCOME_NONE && strcmp(session.outcomePhase, phaseName) == 0) {
    return systemLinkStatusTypeName(session.outcome);
  }
  return "DONE";
}

static bool systemLinkCreatePhaseSteps(const String &resultId, const SystemLinkRoastSession &session) {
  if (resultId.length() == 0) {
    LOG_WARN("SystemLink: Skipping step creation - empty resultId");
    return false;
  }

  double startSec = systemLinkStartPhaseSeconds(session);
  double roastingSec = systemLinkRoastingPhaseSeconds(session);
  double coolingSec = systemLinkCoolingPhaseSeconds(session);
  LOG_INFOF("SystemLink: Phase seconds - start=%.1f, roasting=%.1f, cooling=%.1f (startMs=%lu, roastingMs=%lu, coolingMs=%lu, endMs=%lu)",
            startSec, roastingSec, coolingSec,
            static_cast<unsigned long>(session.startedAtMs),
            static_cast<unsigned long>(session.roastingStartedAtMs),
            static_cast<unsigned long>(session.coolingStartedAtMs),
            static_cast<unsigned long>(session.endedAtMs));
  LOG_INFOF("SystemLink: Free heap before steps: %u bytes", static_cast<unsigned>(ESP.getFreeHeap()));

  DynamicJsonDocument doc(3072);
  JsonArray steps = doc.createNestedArray("steps");

  struct StepSpec {
    const char *name;
    const char *phaseName;
    const char *stepType;
    double seconds;
    const char *detail;
  } stepSpecs[] = {
    {"Start Roast", "starting", "Action", startSec, "Fan ramp and roast start preparation"},
    {"Roasting", "roasting", "Action", roastingSec, "Profile-driven roast control"},
    {"Cooling", "cooling", "Action", coolingSec, "Cooling cycle until safe end temperature"}
  };

  for (size_t index = 0; index < sizeof(stepSpecs) / sizeof(stepSpecs[0]); index++) {
    if (stepSpecs[index].seconds <= 0.0) {
      LOG_INFOF("SystemLink: Skipping step '%s' (seconds=%.3f)", stepSpecs[index].name, stepSpecs[index].seconds);
      continue;
    }

    JsonObject step = steps.createNestedObject();
    step["resultId"] = resultId;
    step["workspace"] = systemLinkConfig.workspaceId;
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
    LOG_WARN("SystemLink: No phase steps to publish (all phases had zero duration)");
    return true;
  }

  if (doc.overflowed()) {
    LOG_ERRORF("SystemLink: Steps JSON document overflowed (capacity=%u)", 3072u);
  }

  String body;
  serializeJson(doc, body);
  LOG_INFOF("SystemLink: Steps POST body length=%u, steps=%u", static_cast<unsigned>(body.length()), static_cast<unsigned>(steps.size()));

  String responseBody;
  int statusCode = -1;
  static const uint8_t MAX_ATTEMPTS = 3;
  for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/steps"), body, responseBody, statusCode);
    String apiError;
    // Batch endpoints can return 2xx with an error object for items that failed.
    bool partialFailure = ok && systemLinkResponseContainsError(responseBody, apiError);
    if (ok && !partialFailure) {
      LOG_INFOF("SystemLink: Published %u roast phase steps (status=%d, attempt=%u)",
                static_cast<unsigned>(steps.size()),
                statusCode,
                static_cast<unsigned>(attempt));
      return true;
    }

    LOG_ERRORF("SystemLink: Step publish failed (attempt %u, status %d): %s",
               static_cast<unsigned>(attempt),
               statusCode,
               partialFailure ? apiError.c_str() : systemLinkDescribeFailure(statusCode, responseBody, nullptr).c_str());

    // Client errors won't succeed on retry, and retrying a partial success would duplicate steps.
    if (partialFailure || (statusCode >= 400 && statusCode < 500 && statusCode != 429)) {
      break;
    }

    delay(300);
  }

  return false;
}

static bool systemLinkCreateResult(SystemLinkRoastSession &session, const String &fileId, String &resultId, int &statusCode) {
  JsonDocument doc;
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
  properties["pidScheduleConfigured"] = session.pidScheduleConfigured ? "true" : "false";
  properties["outcomeReason"] = session.outcomeReason;
  properties["phase"] = session.phase;
  properties["startPhaseSeconds"] = String(systemLinkStartPhaseSeconds(session), 3);
  properties["roastingPhaseSeconds"] = String(systemLinkRoastingPhaseSeconds(session), 3);
  properties["coolingPhaseSeconds"] = String(systemLinkCoolingPhaseSeconds(session), 3);
  properties["resetReason"] = session.resetReason;
  properties["recoveredAfterReset"] = session.recoveredAfterReset ? "true" : "false";
  properties["publishAttempt"] = String(session.publishAttempts + 1);
  properties["freeHeapBytes"] = String(ESP.getFreeHeap());
  // Device-side WARN/ERROR history (includes earlier failed publish attempts).
  properties["recentWarnings"] = debugLogger.getRecentText(100, LOG_LEVEL_WARN, SYSTEMLINK_RECENT_LOG_MAX_CHARS);

  if (fileId.length() > 0) {
    JsonArray fileIds = result.createNestedArray("fileIds");
    fileIds.add(fileId);
  } else {
    properties["traceUploadError"] = session.sampleCount == 0 ? "no_samples" : "csv_upload_failed";
  }

  if (doc.overflowed()) {
    LOG_ERROR("SystemLink: Result JSON ran out of memory");
    return false;
  }

  String body;
  serializeJson(doc, body);

  String responseBody;
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/results"), body, responseBody, statusCode);
  String apiError;
  if (ok && systemLinkResponseContainsError(responseBody, apiError)) {
    LOG_ERRORF("SystemLink: Result rejected (%d): %s", statusCode, apiError.c_str());
    return false;
  }
  if (!ok) {
    LOG_ERRORF("SystemLink: Result publish failed (%d): %s",
               statusCode,
               systemLinkDescribeFailure(statusCode, responseBody, nullptr).c_str());
    return false;
  }

  if (!systemLinkParseCreatedResultId(responseBody, resultId)) {
    LOG_WARNF("SystemLink: Result published but ID missing from response: %s", responseBody.substring(0, 100).c_str());
  }

  LOG_INFOF("SystemLink: Published roast result (%s)", systemLinkStatusTypeName(session.outcome).c_str());
  return true;
}

static void systemLinkEndPublish(const char *status) {
  // Clear breadcrumbs before releasing the slot so a roast finishing right now keeps its own.
  systemLinkUpdatePublishStatus(status);
  systemLinkClearBreadcrumb();
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkPublishInProgress = false;
  systemLinkPublishPending = false;
  portEXIT_CRITICAL(&systemLinkLock);
}

static void processPendingCalibrationPublish();

static void processPendingSystemLinkPublish() {
  processPendingCalibrationPublish();

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

  if (!systemLinkConfig.enabled) {
    LOG_WARN("SystemLink: Disabled - discarding unpublished roast");
    systemLinkEndPublish("disabled");
    return;
  }

  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED) {
    portENTER_CRITICAL(&systemLinkLock);
    systemLinkPublishInProgress = false;
    portEXIT_CRITICAL(&systemLinkLock);
    systemLinkUpdatePublishStatus(systemLinkHasRequiredConfig() ? "waiting_for_network" : "waiting_for_config", false);
    return;
  }

  SystemLinkRoastSession &session = systemLinkPublishSession;
  LOG_INFOF("SystemLink: Publishing roast (attempt %u/%u, %u samples, reason %s)",
            static_cast<unsigned>(session.publishAttempts + 1),
            static_cast<unsigned>(SYSTEMLINK_MAX_PUBLISH_ATTEMPTS),
            static_cast<unsigned>(session.sampleCount),
            session.outcomeReason);

  // Upload the trace only once so retries don't create duplicate files.
  if (session.fileId[0] == '\0' && session.sampleCount > 0) {
    String uploadUri;
    String filename = String("roast-") + (session.profileId[0] != '\0' ? session.profileId : "session") + ".csv";
    systemLinkUpdatePublishStatus("uploading_csv");
    systemLinkPersistBreadcrumb(session, false, true, "uploading_csv");
    if (systemLinkUploadTraceFile(filename, session, uploadUri)) {
      systemLinkCopyString(session.fileId, sizeof(session.fileId), systemLinkExtractIdFromUri(uploadUri));
    }
  }

  systemLinkUpdatePublishStatus("creating_result");
  systemLinkPersistBreadcrumb(session, false, true, "creating_result");
  String resultId;
  int statusCode = -1;
  bool published = systemLinkCreateResult(session, String(session.fileId), resultId, statusCode);

  if (!published) {
    session.publishAttempts++;
    bool clientError = statusCode >= 400 && statusCode < 500 && statusCode != 408 && statusCode != 429;
    if (clientError || session.publishAttempts >= SYSTEMLINK_MAX_PUBLISH_ATTEMPTS) {
      LOG_ERRORF("SystemLink: Giving up on roast publish after %u attempts (last status %d)",
                 static_cast<unsigned>(session.publishAttempts), statusCode);
      systemLinkEndPublish(clientError ? "publish_rejected" : "publish_abandoned");
      return;
    }
    portENTER_CRITICAL(&systemLinkLock);
    systemLinkPublishInProgress = false;
    portEXIT_CRITICAL(&systemLinkLock);
    systemLinkUpdatePublishStatus(String("result_publish_failed_") + statusCode);
    return;
  }

  bool stepsPublished = true;
  if (resultId.length() > 0) {
    systemLinkUpdatePublishStatus("creating_steps");
    systemLinkPersistBreadcrumb(session, false, true, "creating_steps");
    stepsPublished = systemLinkCreatePhaseSteps(resultId, session);
  }

  systemLinkEndPublish(stepsPublished ? "published" : "published_steps_failed");
}

// ============================================================================
// Step-Response Calibration Publish
// ============================================================================

// Build a calibration CSV string from the step-response tuner's trace and model data.
// Format: elapsedMs,actualTempF,setpointTempF,heaterOutput,phase,band
// Followed by a FOPDT model summary section.
static String systemLinkBuildCalibrationCsv(const StepResponseTuner &tuner) {
  StepResponseTuner::Summary summary = tuner.getSummary();
  uint16_t sampleCount = tuner.getTraceSampleCount();

  // Estimate size: header + rows (~50 bytes each) + model summary (~500 bytes)
  String csv;
  csv.reserve(256 + sampleCount * 50 + 512);

  // Trace data header
  csv += "elapsedMs,actualTempF,setpointTempF,heaterOutput,phaseId\n";

  for (uint16_t i = 0; i < sampleCount; i++) {
    StepResponseTuner::TraceSample s = tuner.getTraceSample(i);
    char row[80];
    snprintf(row, sizeof(row), "%u,%.1f,%.1f,%.1f,%u\n",
             static_cast<unsigned>(s.elapsedMs),
             static_cast<double>(s.actualTempF),
             static_cast<double>(s.setpointTempF),
             static_cast<double>(s.heaterOutput),
             static_cast<unsigned>(s.phaseId));
    csv += row;
  }

  // FOPDT model summary section
  csv += "\n# FOPDT Model Summary\n";
  csv += "# band,targetTempF,baselineTempF,finalTempF,processGain,timeConstantS,deadTimeS,onsetDeadTimeS,noiseSigmaF,fitRmseF,pidKp,pidKi,pidKd\n";

  for (uint8_t i = 0; i < summary.totalBands; i++) {
    const StepResponseTuner::BandResult &br = summary.bands[i];
    if (!br.valid) continue;
    char row[256];
    snprintf(row, sizeof(row), "%u,%.1f,%.1f,%.1f,%.6f,%.2f,%.2f,%.2f,%.3f,%.3f,%.4f,%.6f,%.4f\n",
             static_cast<unsigned>(i + 1),
             br.targetTemp,
             br.model.baselineTemp,
             br.model.finalTemp,
             br.model.processGain,
             br.model.timeConstant,
             br.model.deadTime,
             br.model.onsetDeadTime,
             br.model.noiseStdDev,
             br.model.fitRmse,
             br.kp, br.ki, br.kd);
    csv += row;
  }

  // Global recommended PID
  char pidLine[128];
  snprintf(pidLine, sizeof(pidLine), "\n# Recommended PID: Kp=%.4f Ki=%.6f Kd=%.4f tauCFactor=%.2f\n",
           summary.recommendedKp, summary.recommendedKi, summary.recommendedKd, summary.tauCFactor);
  csv += pidLine;

  return csv;
}

// Upload a calibration CSV string to SystemLink file service.
static bool systemLinkUploadCalibrationCsv(const String &filename,
                                            const String &csvContent,
                                            String &uploadedUri) {
  return systemLinkUploadMultipart(filename, "text/csv", csvContent.length(), [&csvContent](WiFiClientSecure &client) {
    const char *data = csvContent.c_str();
    size_t remaining = csvContent.length();
    while (remaining > 0) {
      size_t chunk = min(remaining, static_cast<size_t>(1024));
      client.write(reinterpret_cast<const uint8_t *>(data), chunk);
      data += chunk;
      remaining -= chunk;
    }
    return true;
  }, uploadedUri);
}

// Create a test result for a calibration run.
static bool systemLinkCreateCalibrationResult(const StepResponseTuner &tuner,
                                               const String &fileId,
                                               String &resultId) {
  StepResponseTuner::Summary summary = tuner.getSummary();
  bool passed = tuner.isComplete() && summary.passed;

  JsonDocument doc;
  JsonObject result = doc.createNestedArray("results").createNestedObject();

  result["programName"] = "Coffee Roaster - PID Calibration (Step-Response)";
  JsonObject status = result.createNestedObject("status");
  status["statusType"] = passed ? "PASSED" : "FAILED";
  status["statusName"] = passed ? "Calibration Passed" : "Calibration Failed";
  result["systemId"] = systemLinkConfig.systemId;
  result["hostName"] = WiFi.getHostname();
  result["partNumber"] = "coffee-roaster";
  result["operator"] = "coffee-roaster";
  result["workspace"] = systemLinkConfig.workspaceId;

  JsonArray keywords = result.createNestedArray("keywords");
  keywords.add("coffee-roaster");
  keywords.add("calibration");
  keywords.add("step-response");
  keywords.add("FOPDT");
  keywords.add("SIMC");

  JsonObject properties = result.createNestedObject("properties");
  properties["calibrationType"] = "step_response_simc";
  properties["tauCFactor"] = String(summary.tauCFactor, 2);
  properties["totalBands"] = String(summary.totalBands);
  properties["validBandCount"] = String(summary.validBandCount);
  properties["completedBands"] = String(summary.completedBands);
  properties["meanFitRmseF"] = String(summary.meanFitRmse, 3);
  properties["worstFitRmseF"] = String(summary.worstFitRmse, 3);
  properties["maxTempF"] = String(summary.maxTemp, 1);
  properties["totalSamples"] = String(summary.totalSamples);
  properties["completed"] = tuner.isComplete() ? "true" : "false";
  properties["lastError"] = tuner.getLastError();
  properties["freeHeapBytes"] = String(ESP.getFreeHeap());
  properties["recentWarnings"] = debugLogger.getRecentText(100, LOG_LEVEL_WARN, SYSTEMLINK_RECENT_LOG_MAX_CHARS);

  // Recommended PID gains
  properties["recommendedKp"] = String(summary.recommendedKp, 4);
  properties["recommendedKi"] = String(summary.recommendedKi, 6);
  properties["recommendedKd"] = String(summary.recommendedKd, 4);

  // Per-band FOPDT model parameters
  for (uint8_t i = 0; i < summary.totalBands; i++) {
    const StepResponseTuner::BandResult &br = summary.bands[i];
    if (!br.valid) continue;
    String prefix = "band" + String(i + 1) + "_";
    properties[prefix + "targetTempF"] = String(br.targetTemp, 1);
    properties[prefix + "processGain"] = String(br.model.processGain, 6);
    properties[prefix + "timeConstantS"] = String(br.model.timeConstant, 2);
    properties[prefix + "deadTimeS"] = String(br.model.deadTime, 2);
    properties[prefix + "onsetDeadTimeS"] = String(br.model.onsetDeadTime, 2);
    properties[prefix + "noiseSigmaF"] = String(br.model.noiseStdDev, 3);
    properties[prefix + "fitRmseF"] = String(br.model.fitRmse, 3);
    properties[prefix + "baselineTempF"] = String(br.model.baselineTemp, 1);
    properties[prefix + "finalTempF"] = String(br.model.finalTemp, 1);
    properties[prefix + "pidKp"] = String(br.kp, 4);
    properties[prefix + "pidKi"] = String(br.ki, 6);
    properties[prefix + "pidKd"] = String(br.kd, 4);
  }

  if (fileId.length() > 0) {
    JsonArray fileIds = result.createNestedArray("fileIds");
    fileIds.add(fileId);
  }

  String body;
  serializeJson(doc, body);

  String responseBody;
  int statusCode = -1;
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/results"), body, responseBody, statusCode);
  String apiError;
  if (ok && systemLinkResponseContainsError(responseBody, apiError)) {
    LOG_ERRORF("SystemLink: Calibration result rejected (%d): %s", statusCode, apiError.c_str());
    return false;
  }
  if (!ok) {
    LOG_ERRORF("SystemLink: Calibration result publish failed (%d): %s",
               statusCode,
               systemLinkDescribeFailure(statusCode, responseBody, nullptr).c_str());
    return false;
  }

  if (!systemLinkParseCreatedResultId(responseBody, resultId)) {
    LOG_WARN("SystemLink: Calibration result published but ID not found in response");
  }

  LOG_INFOF("SystemLink: Published calibration result (id=%s)", resultId.c_str());
  return true;
}

// Build CSV, upload file, create test result. Runs on the publish task.
static void systemLinkPublishCalibration(const StepResponseTuner &tuner) {
  LOG_INFO("SystemLink: Publishing step-response calibration data");

  String uploadUri;
  String fileId;
  {
    String csvContent = systemLinkBuildCalibrationCsv(tuner);
    if (systemLinkUploadCalibrationCsv("calibration-step-response.csv", csvContent, uploadUri)) {
      fileId = systemLinkExtractIdFromUri(uploadUri);
      LOG_INFOF("SystemLink: Calibration CSV uploaded (fileId=%s)", fileId.c_str());
    } else {
      LOG_WARN("SystemLink: Calibration CSV upload failed, publishing result without file");
    }
  }

  String resultId;
  if (systemLinkCreateCalibrationResult(tuner, fileId, resultId)) {
    LOG_INFOF("SystemLink: Calibration result created (resultId=%s)", resultId.c_str());
  } else {
    LOG_ERROR("SystemLink: Failed to create calibration test result");
  }
}

// Called from loop() when tuning ends; the upload happens on the publish task so the control loop never blocks.
static void systemLinkQueueCalibrationPublish() {
  if (!systemLinkConfig.enabled) {
    return;
  }
  systemLinkCalibrationPublishPending = true;
}

// New calibrations must wait while a publish can run: the publish task reads the tuner's trace in place.
static bool systemLinkCalibrationPublishBusy() {
  return systemLinkCalibrationPublishPending && systemLinkHasRequiredConfig() && WiFi.status() == WL_CONNECTED;
}

static void processPendingCalibrationPublish() {
  if (!systemLinkCalibrationPublishPending) {
    return;
  }
  if (!systemLinkConfig.enabled) {
    systemLinkCalibrationPublishPending = false;
    return;
  }
  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED) {
    return;
  }
  systemLinkPublishCalibration(stepTuner);
  systemLinkCalibrationPublishPending = false;
}

#endif // SYSTEMLINK_HPP