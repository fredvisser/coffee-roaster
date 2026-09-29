#ifndef SYSTEMLINK_HPP
#define SYSTEMLINK_HPP

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>
#include <esp_system.h>
#include <esp_crt_bundle.h>
#include "../support/DebugLog.hpp"
#include "../platform/BoardConfig.hpp"
#include "../profiles/ProfileManager.hpp"
#include "../control/StepResponseTuner.hpp"
#include "../platform/RoasterTypes.hpp"

extern Preferences preferences;
extern ProfileManager profileManager;
extern RoastProfile profile;
extern StepResponseTuner stepTuner;
extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;
extern double fanTemp;
extern double heaterOutputVal;
extern double heaterPidTrimVal;
extern double heaterFeedforwardVal;
extern int setpointProgress;
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
#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
static const size_t SYSTEMLINK_MAX_TRACE_SAMPLES = 900;
#else
static const size_t SYSTEMLINK_MAX_TRACE_SAMPLES = 1800;
#endif
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
  double faultBeanTempF;
  double faultFanTempF;
  double faultHeaterOutput;
  RoasterState faultState;
  SystemLinkRoastOutcome outcome;
  double kp;
  double ki;
  double kd;
  int16_t finalTempOverrideF;
  bool pidScheduleConfigured;
  bool recoveredAfterReset;
  char profileId[SYSTEMLINK_PROFILE_ID_MAX];
  char profileName[SYSTEMLINK_PROFILE_NAME_MAX];
  char outcomeReason[SYSTEMLINK_REASON_MAX];
  char outcomePhase[SYSTEMLINK_PHASE_MAX];
  char phase[SYSTEMLINK_PHASE_MAX];
  char resetReason[SYSTEMLINK_RESET_REASON_MAX];
  RoastTraceSample *samples;
};

static portMUX_TYPE systemLinkLock = portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t systemLinkWorkerTaskHandle = nullptr;
extern bool otaUpdateInProgress;
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
static SystemLinkRoastSession systemLinkQueuedPublishSession = {};
static bool systemLinkPublishPending = false;
static bool systemLinkPublishInProgress = false;
static bool systemLinkQueuedPublishPending = false;
static portMUX_TYPE systemLinkCalibrationLock = portMUX_INITIALIZER_UNLOCKED;
static bool systemLinkCalibrationPublishPending = false;
static StepResponseTuner::Summary systemLinkPendingCalibrationSummary = {};
static char *systemLinkPendingCalibrationCsv = nullptr;
static size_t systemLinkPendingCalibrationCsvLength = 0;
static char systemLinkPendingCalibrationFileId[80] = {};
static uint32_t systemLinkLastCalibrationAttemptMs = 0;
static volatile uint32_t systemLinkActiveRequestCount = 0;
static uint32_t systemLinkLastPublishAttemptMs = 0;
static String systemLinkPublishFileId;
static bool systemLinkTagsProvisioned = false;
static uint32_t systemLinkLastTagProvisionAttemptMs = 0;  // Cooldown for tag provisioning
static uint32_t systemLinkLastIdleChamberPublishMs = 0;
static bool systemLinkLastTelemetrySentValid = false;
static SystemLinkTelemetrySnapshot systemLinkLastTelemetrySent = {false, 0.0f, 0.0f, 0, IDLE, "", "", "", 0};
extern const uint8_t systemLinkCaBundleStart[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t systemLinkCaBundleEnd[] asm("_binary_x509_crt_bundle_end");

static SystemLinkConfig systemLinkGetConfigSnapshot() {
  SystemLinkConfig snapshot;
  portENTER_CRITICAL(&systemLinkLock);
  snapshot = systemLinkConfig;
  portEXIT_CRITICAL(&systemLinkLock);
  return snapshot;
}

static bool systemLinkIsTrackedRoastState(RoasterState state);
static void systemLinkCopyString(char *dest, size_t destSize, const char *src);
static void systemLinkCopyString(char *dest, size_t destSize, const String &src);
static bool systemLinkParseApiEndpoint(String &host, uint16_t &port);

static void systemLinkActiveRequestEnter() {
  portENTER_CRITICAL(&systemLinkLock);
  systemLinkActiveRequestCount++;
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkActiveRequestLeave() {
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkActiveRequestCount > 0) {
    systemLinkActiveRequestCount--;
  }
  portEXIT_CRITICAL(&systemLinkLock);
}

static bool systemLinkHasActiveRequests() {
  portENTER_CRITICAL(&systemLinkLock);
  bool active = systemLinkActiveRequestCount > 0;
  portEXIT_CRITICAL(&systemLinkLock);
  return active;
}

static bool systemLinkWaitForIdle(uint32_t timeoutMs) {
  uint32_t startedAt = millis();
  while (systemLinkHasActiveRequests()) {
    if (millis() - startedAt >= timeoutMs) {
      return false;
    }
    delay(25);
  }
  return true;
}

static void systemLinkFeedWatchdog() {
  if (xTaskGetCurrentTaskHandle() == systemLinkWorkerTaskHandle) {
    return;
  }
  esp_task_wdt_reset();
}

static RoastTraceSample *systemLinkAllocateTraceBuffer() {
#if ROASTER_TARGET_BOARD == ROASTER_BOARD_JC4827W543C
  if (!psramFound()) {
    return nullptr;
  }
  return static_cast<RoastTraceSample *>(heap_caps_malloc(sizeof(RoastTraceSample) * SYSTEMLINK_MAX_TRACE_SAMPLES,
                                                          MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
#else
  return static_cast<RoastTraceSample *>(malloc(sizeof(RoastTraceSample) * SYSTEMLINK_MAX_TRACE_SAMPLES));
#endif
}

static void systemLinkFreeTraceBuffer(SystemLinkRoastSession &session) {
  if (session.samples != nullptr) {
    free(session.samples);
    session.samples = nullptr;
  }
  session.sampleCount = 0;
  session.lastRecordedSecond = 0xFFFF;
}

static void systemLinkInvalidateTagPublishState() {
  systemLinkTagsProvisioned = false;
  systemLinkLastTagProvisionAttemptMs = 0;  // Reset cooldown on config change
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
    systemLinkCopyString(session.outcomeReason, sizeof(session.outcomeReason), reason);
  }
  if (outcomePhase != nullptr && outcomePhase[0] != '\0') {
    systemLinkCopyString(session.outcomePhase, sizeof(session.outcomePhase), outcomePhase);
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
  return !systemLinkLastTelemetrySentValid || now - systemLinkLastIdleChamberPublishMs >= 300000UL;
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

static void systemLinkCopyString(char *dest, size_t destSize, const char *src) {
  if (destSize == 0) {
    return;
  }
  if (src == nullptr) {
    dest[0] = '\0';
    return;
  }
  size_t length = strnlen(src, destSize - 1);
  memcpy(dest, src, length);
  dest[length] = '\0';
}

static void systemLinkCopyString(char *dest, size_t destSize, const String &src) {
  if (destSize == 0) {
    return;
  }
  src.toCharArray(dest, destSize);
  dest[destSize - 1] = '\0';
}

static String systemLinkMaskedKey(const char *apiKey) {
  if (apiKey[0] == '\0') {
    return "";
  }

  String key(apiKey);
  if (key.length() <= 8) {
    return "stored";
  }

  return String("...") + key.substring(key.length() - 4);
}

static void systemLinkConfigureTls(WiFiClientSecure &client) {
  client.setCACertBundle(systemLinkCaBundleStart,
                         static_cast<size_t>(systemLinkCaBundleEnd - systemLinkCaBundleStart));
  client.setTimeout(5000);
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
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  return config.enabled && strncmp(config.apiUrl, "https://", 8) == 0 &&
         config.apiKey[0] != '\0' && config.apiUrl[0] != '\0' &&
         config.workspaceId[0] != '\0' && config.systemId[0] != '\0';
}

static bool systemLinkCanStartRoast() {
  if (!systemLinkHasRequiredConfig()) {
    return true;
  }

  portENTER_CRITICAL(&systemLinkLock);
  bool full = systemLinkQueuedPublishPending &&
              (systemLinkPublishPending || systemLinkPublishInProgress);
  portEXIT_CRITICAL(&systemLinkLock);
  return !full;
}

static String systemLinkBaseUrl(const char *servicePath) {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  String base(config.apiUrl);
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
  struct ActiveRequestGuard {
    ActiveRequestGuard() { systemLinkActiveRequestEnter(); }
    ~ActiveRequestGuard() { systemLinkActiveRequestLeave(); }
  } activeRequestGuard;

  responseBody = "";
  statusCode = -1;

  if (otaUpdateInProgress) {
    LOG_WARNF("SystemLink: Skipping %s %s because OTA is in progress", method.c_str(), url.c_str());
    return false;
  }

  // Log connection attempt with WiFi status and signal strength
  wl_status_t wifiStatus = WiFi.status();
  int32_t rssi = (wifiStatus == WL_CONNECTED) ? WiFi.RSSI() : 0;
  uint32_t freeHeap = ESP.getFreeHeap();
  uint32_t minFreeHeap = ESP.getMinFreeHeap();
  LOG_DEBUGF("SL %s %s r=%d h=%u/%u",
             method.c_str(),
             url.c_str(),
             rssi,
             static_cast<unsigned>(freeHeap),
             static_cast<unsigned>(minFreeHeap));
  
  if (wifiStatus != WL_CONNECTED) {
    LOG_WARNF("SystemLink: WiFi not connected (status=%d) when attempting %s to %s", 
              wifiStatus, method.c_str(), url.c_str());
    return false;
  }

  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  WiFiClientSecure client;
  systemLinkConfigureTls(client);

  String endpointHost;
  uint16_t endpointPort;
  if (systemLinkParseApiEndpoint(endpointHost, endpointPort)) {
    IPAddress resolvedIp;
    if (WiFi.hostByName(endpointHost.c_str(), resolvedIp)) {
      String resolvedText = resolvedIp.toString();
      LOG_DEBUGF("DNS %s=%s:%u", endpointHost.c_str(), resolvedText.c_str(), static_cast<unsigned>(endpointPort));
    } else {
      LOG_WARNF("SystemLink: DNS resolution failed for %s", endpointHost.c_str());
    }
  }

  HTTPClient http;
  // Increase timeouts: DNS resolution can be slow, TLS handshake can take time
  http.setConnectTimeout(10000);  // 10 seconds for connection + TLS
  http.setTimeout(15000);          // 15 seconds total request timeout

  LOG_DEBUGF("SL begin %s", url.c_str());
  if (!http.begin(client, url)) {
    LOG_ERRORF("SystemLink: http.begin() failed for %s (possible DNS or URL parse issue)", url.c_str());
    return false;
  }

  http.addHeader("accept", "application/json");
  http.addHeader("x-ni-api-key", config.apiKey);
  if (contentType.length() > 0) {
    http.addHeader("Content-Type", contentType);
  }

  systemLinkFeedWatchdog();
  LOG_DEBUGF("%s %dB", method.c_str(), payloadLength);
  
  if (method == "POST") {
    statusCode = http.POST(const_cast<uint8_t *>(payload), payloadLength);
  } else if (method == "PUT") {
    statusCode = http.PUT(const_cast<uint8_t *>(payload), payloadLength);
  } else {
    statusCode = http.sendRequest(method.c_str(), const_cast<uint8_t *>(payload), payloadLength);
  }
  
  responseBody = http.getString();
  http.end();
  systemLinkFeedWatchdog();

  // Log detailed error for connection failures
  if (statusCode < 0) {
    char clientError[128] = {0};
    int secureError = client.lastError(clientError, sizeof(clientError));
    String errorText = HTTPClient::errorToString(statusCode);
    LOG_ERRORF("SystemLink: HTTP request failed (statusCode=%d, error=%s, secureError=%d, secureDetail=%s) for %s %s", 
               statusCode,
               errorText.c_str(),
               secureError,
               clientError[0] != '\0' ? clientError : "none",
               method.c_str(),
               url.c_str());
  } else if (statusCode >= 400) {
    LOG_WARNF("SystemLink: HTTP %d response for %s %s", statusCode, method.c_str(), url.c_str());
  }

  return statusCode >= 200 && statusCode < 300;
}

static bool systemLinkParseApiEndpoint(String &host, uint16_t &port) {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  String base(config.apiUrl);
  base.trim();
  if (base.length() == 0) {
    return false;
  }

  if (!base.startsWith("https://")) {
    return false;
  }
  base.remove(0, 8);
  port = 443;

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

static size_t systemLinkCsvLength(const SystemLinkRoastSession &session) {
  size_t total = strlen("elapsedSeconds,actualTempF,targetTempF,heaterOutput,fanTempF,fanOutput\n");
  char row[96];

  if (session.samples == nullptr) {
    return total;
  }

  for (uint16_t index = 0; index < session.sampleCount; index++) {
    const RoastTraceSample &sample = session.samples[index];
    int rowLen = snprintf(row,
                          sizeof(row),
                          "%u,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                          static_cast<unsigned>(sample.elapsedSeconds),
                          sample.actualTenthsF / 10.0f,
                          sample.targetTenthsF / 10.0f,
                          sample.heaterOutputTenths / 10.0f,
                          sample.fanTempTenthsF / 10.0f,
                          sample.fanOutputTenths / 10.0f);
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

  JsonArray failed = doc["failed"].as<JsonArray>();
  if (failed.size() > 0) {
    errorMessage = "API returned failed entries";
    return true;
  }

  JsonVariant error = doc["error"];
  if (error.is<const char *>()) {
    errorMessage = error.as<String>();
  } else if (error["message"].is<const char *>()) {
    errorMessage = error["message"].as<String>();
  } else if (error["name"].is<const char *>()) {
    errorMessage = error["name"].as<String>();
  } else {
    if (!error.is<JsonObject>()) {
      return false;
    }
    errorMessage = "unknown_error";
  }
  return true;
}

static void systemLinkAppendJsonStringValue(String &body, const String &value) {
  body += '"';
  body += value;
  body += '"';
}

static bool systemLinkUploadTraceFile(const String &filename,
                                     const String &contentType,
                                     const SystemLinkRoastSession &session,
                                     String &uploadedUri) {
  struct ActiveRequestGuard {
    ActiveRequestGuard() { systemLinkActiveRequestEnter(); }
    ~ActiveRequestGuard() { systemLinkActiveRequestLeave(); }
  } activeRequestGuard;

  uploadedUri = "";

  if (otaUpdateInProgress) {
    LOG_WARN("SystemLink: Skipping trace upload because OTA is in progress");
    return false;
  }

  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  String host;
  uint16_t port;
  if (!systemLinkParseApiEndpoint(host, port)) {
    LOG_ERROR("SystemLink: Invalid API URL configuration");
    return false;
  }

  const String boundary = "----CoffeeRoasterSystemLinkBoundary";
  const char *csvHeader = "elapsedSeconds,actualTempF,targetTempF,heaterOutput,fanTempF,fanOutput\n";
  String prefix;
  prefix.reserve(filename.length() + contentType.length() + boundary.length() + 128);
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
  prefix += "Content-Type: " + contentType + "\r\n\r\n";

  String suffix = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = prefix.length() + systemLinkCsvLength(session) + suffix.length();

  WiFiClientSecure client;
  systemLinkConfigureTls(client);
  if (!client.connect(host.c_str(), port)) {
    LOG_ERRORF("SystemLink: Failed to connect to %s:%u", host.c_str(), static_cast<unsigned>(port));
    return false;
  }

  String requestPath = String("/nifile/v1/service-groups/Default/upload-files?workspace=") + config.workspaceId;
  client.printf("POST %s HTTP/1.1\r\n", requestPath.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.print("Connection: close\r\n");
  client.print("Accept: application/json\r\n");
  client.printf("x-ni-api-key: %s\r\n", config.apiKey);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %u\r\n\r\n", static_cast<unsigned>(contentLength));
  client.print(prefix);
  client.print(csvHeader);

  char row[96];
  if (session.samples == nullptr) {
    client.print(suffix);
    client.flush();
    return true;
  }

  for (uint16_t index = 0; index < session.sampleCount; index++) {
    const RoastTraceSample &sample = session.samples[index];
    int rowLen = snprintf(row,
                          sizeof(row),
                          "%u,%.1f,%.1f,%.1f,%.1f,%.1f\n",
                          static_cast<unsigned>(sample.elapsedSeconds),
                          sample.actualTenthsF / 10.0f,
                          sample.targetTenthsF / 10.0f,
                          sample.heaterOutputTenths / 10.0f,
                          sample.fanTempTenthsF / 10.0f,
                          sample.fanOutputTenths / 10.0f);
    if (rowLen <= 0 || rowLen >= static_cast<int>(sizeof(row))) {
      client.stop();
      LOG_ERROR("SystemLink: Failed to format CSV row");
      return false;
    }
    client.write(reinterpret_cast<const uint8_t *>(row), static_cast<size_t>(rowLen));
    systemLinkFeedWatchdog();
  }

  client.print(suffix);
  client.flush();

  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && millis() - waitStart < 7000UL) {
    delay(10);
    systemLinkFeedWatchdog();
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
  SystemLinkConfig config = {};
  config.enabled = preferences.getBool(SYSTEMLINK_ENABLED_KEY, false);
  systemLinkCopyString(config.apiUrl, sizeof(config.apiUrl),
                       preferences.getString(SYSTEMLINK_API_URL_KEY, "https://dev-api.lifecyclesolutions.ni.com"));
  systemLinkCopyString(config.workspaceId, sizeof(config.workspaceId), preferences.getString(SYSTEMLINK_WORKSPACE_KEY, ""));
  systemLinkCopyString(config.systemId, sizeof(config.systemId), preferences.getString(SYSTEMLINK_SYSTEM_ID_KEY, ""));
  systemLinkCopyString(config.apiKey, sizeof(config.apiKey), preferences.getString(SYSTEMLINK_API_KEY_KEY, ""));
  char lastFault[SYSTEMLINK_REASON_MAX];
  char publishStatus[SYSTEMLINK_STATUS_MAX];
  systemLinkCopyString(lastFault, sizeof(lastFault), preferences.getString(SYSTEMLINK_LAST_FAULT_KEY, "none"));
  systemLinkCopyString(publishStatus, sizeof(publishStatus), preferences.getString(SYSTEMLINK_LAST_PUB_STATUS_KEY, "idle"));

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkConfig = config;
  memcpy(systemLinkTelemetry.lastFault, lastFault, sizeof(lastFault));
  memcpy(systemLinkTelemetry.publishStatus, publishStatus, sizeof(publishStatus));
  portEXIT_CRITICAL(&systemLinkLock);
}

static void saveSystemLinkConfig() {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  preferences.putBool(SYSTEMLINK_ENABLED_KEY, config.enabled);
  preferences.putString(SYSTEMLINK_API_URL_KEY, config.apiUrl);
  preferences.putString(SYSTEMLINK_WORKSPACE_KEY, config.workspaceId);
  preferences.putString(SYSTEMLINK_SYSTEM_ID_KEY, config.systemId);
  if (config.apiKey[0] == '\0') {
    preferences.remove(SYSTEMLINK_API_KEY_KEY);
  } else {
    preferences.putString(SYSTEMLINK_API_KEY_KEY, config.apiKey);
  }
}

static String getSystemLinkConfigJSON() {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  DynamicJsonDocument doc(768);
  doc["enabled"] = config.enabled;
  doc["apiUrl"] = config.apiUrl;
  doc["workspaceId"] = config.workspaceId;
  doc["systemId"] = config.systemId;
  doc["hasApiKey"] = config.apiKey[0] != '\0';
  doc["apiKeyMasked"] = systemLinkMaskedKey(config.apiKey);
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

  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  if (doc["enabled"].is<bool>()) {
    config.enabled = doc["enabled"].as<bool>();
  }
  if (doc["apiUrl"].is<const char *>()) {
    const char *apiUrl = doc["apiUrl"].as<const char *>();
    if (strncmp(apiUrl, "https://", 8) != 0) {
      errorMessage = "https_required";
      return false;
    }
    systemLinkCopyString(config.apiUrl, sizeof(config.apiUrl), apiUrl);
  }
  if (doc["workspaceId"].is<const char *>()) {
    systemLinkCopyString(config.workspaceId, sizeof(config.workspaceId), doc["workspaceId"].as<const char *>());
  }
  if (doc["systemId"].is<const char *>()) {
    systemLinkCopyString(config.systemId, sizeof(config.systemId), doc["systemId"].as<const char *>());
  }
  if (doc["clearApiKey"].as<bool>()) {
    config.apiKey[0] = '\0';
  } else if (doc["apiKey"].is<const char *>()) {
    String apiKey = doc["apiKey"].as<String>();
    apiKey.trim();
    if (apiKey.length() > 0) {
      systemLinkCopyString(config.apiKey, sizeof(config.apiKey), apiKey);
    }
  }

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkConfig = config;
  portEXIT_CRITICAL(&systemLinkLock);

  saveSystemLinkConfig();
  systemLinkInvalidateTagPublishState();
  errorMessage = "";
  return true;
}

static String systemLinkTagPath(const char *suffix) {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  String path(config.systemId);
  path += ".";
  path += suffix;
  return path;
}

static bool systemLinkCreateOrUpdateTag(const String &path,
                                        const char *type,
                                        bool collectAggregates = false,
                                        int retentionDays = 0) {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  DynamicJsonDocument doc(768);
  doc["path"] = path;
  doc["type"] = type;
  doc["workspace"] = config.workspaceId;
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

static void systemLinkAddCurrentValue(JsonArray updates,
                                      const SystemLinkConfig &config,
                                      const String &path,
                                      const char *type,
                                      const String &value) {
  JsonObject tagUpdate = updates.createNestedObject();
  tagUpdate["path"] = path;
  tagUpdate["workspace"] = config.workspaceId;
  JsonObject valueUpdate = tagUpdate.createNestedArray("updates").createNestedObject();
  JsonObject tagValue = valueUpdate.createNestedObject("value");
  tagValue["type"] = type;
  tagValue["value"] = value;
}

static bool systemLinkEnsureRealtimeTags() {
  if (systemLinkTagsProvisioned) {
    return true;
  }

  // Implement exponential backoff for failed provisioning attempts
  // Start at 5 seconds, increase on failure
  uint32_t now = millis();
  uint32_t minBackoffMs = 5000;  // 5 seconds minimum before retry
  
  if (systemLinkLastTagProvisionAttemptMs > 0) {
    uint32_t timeSinceLastAttempt = now - systemLinkLastTagProvisionAttemptMs;
    if (timeSinceLastAttempt < minBackoffMs) {
      // Still in backoff period, don't attempt yet
      return false;
    }
  }

  systemLinkLastTagProvisionAttemptMs = now;
  
  bool ok = true;
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("chamberTemp"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("targetTemp"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("fanTemp"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("heaterOutput"), "DOUBLE", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("roastState"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("roastProgress"), "INT", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("lastFault"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("publishStatus"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("resetReason"), "STRING", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  ok &= systemLinkCreateOrUpdateTag(systemLinkTagPath("bootCount"), "INT", true, SYSTEMLINK_STATUS_TAG_RETENTION_DAYS);
  systemLinkTagsProvisioned = ok;
  
  if (!ok) {
    LOG_WARNF("SystemLink: Tag provisioning failed, will retry in %d ms", minBackoffMs);
  }
  
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

  RoasterState state = getRoasterStateSnapshot();
  snapshot.active = systemLinkIsTrackedRoastState(state);
  snapshot.state = state;
  snapshot.chamberTempF = static_cast<float>(currentTemp);
  snapshot.targetTempF = static_cast<float>(setpointTemp);
  snapshot.roastProgress = setpointProgress;

  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  DynamicJsonDocument doc(2048);
  JsonArray updates = doc.to<JsonArray>();
  bool chamberTempUpdateAttempted = systemLinkShouldPublishChamberTemp(snapshot);
  if (chamberTempUpdateAttempted) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("chamberTemp"), "DOUBLE", String(snapshot.chamberTempF, 1));
  }

  if (systemLinkIsTrackedRoastState(snapshot.state) &&
      (!systemLinkLastTelemetrySentValid ||
       systemLinkTenths(snapshot.targetTempF) != systemLinkTenths(systemLinkLastTelemetrySent.targetTempF))) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("targetTemp"), "DOUBLE", String(snapshot.targetTempF, 1));
  }

  if (snapshot.active || chamberTempUpdateAttempted) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("fanTemp"), "DOUBLE", String(fanTemp, 1));
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("heaterOutput"), "DOUBLE", String(heaterOutputVal, 1));
  }

  if (!systemLinkLastTelemetrySentValid || snapshot.state != systemLinkLastTelemetrySent.state) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("roastState"), "STRING", String(systemLinkStateName(snapshot.state)));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.roastProgress != systemLinkLastTelemetrySent.roastProgress) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("roastProgress"), "INT", String(snapshot.roastProgress));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.lastFault, systemLinkLastTelemetrySent.lastFault) != 0) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("lastFault"), "STRING", String(snapshot.lastFault));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.publishStatus, systemLinkLastTelemetrySent.publishStatus) != 0) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("publishStatus"), "STRING", String(snapshot.publishStatus));
  }
  if (!systemLinkLastTelemetrySentValid || strcmp(snapshot.resetReason, systemLinkLastTelemetrySent.resetReason) != 0) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("resetReason"), "STRING", String(snapshot.resetReason));
  }
  if (!systemLinkLastTelemetrySentValid || snapshot.bootCount != systemLinkLastTelemetrySent.bootCount) {
    systemLinkAddCurrentValue(updates, config, systemLinkTagPath("bootCount"), "INT", String(snapshot.bootCount));
  }

  if (updates.size() == 0) {
    return;
  }
  if (doc.overflowed()) {
    LOG_ERROR("SystemLink: Current-value batch JSON overflowed");
    return;
  }

  String body;
  serializeJson(doc, body);
  String responseBody;
  int statusCode = -1;
  bool updated = systemLinkPostJson(systemLinkBaseUrl("/nitag/v2/update-current-values"), body, responseBody, statusCode);
  String apiError;
  if (updated && systemLinkResponseContainsError(responseBody, apiError)) {
    LOG_WARNF("SystemLink: Current-value batch rejected (%d): %s", statusCode, apiError.c_str());
    updated = false;
  }

  if (updated) {
    systemLinkLastTelemetrySent = snapshot;
    systemLinkLastTelemetrySentValid = true;
    if (chamberTempUpdateAttempted && !systemLinkIsTrackedRoastState(snapshot.state)) {
      systemLinkLastIdleChamberPublishMs = millis();
    }
  } else {
    LOG_WARNF("SystemLink: Current-value batch failed (%d)", statusCode);
    if (chamberTempUpdateAttempted && !systemLinkIsTrackedRoastState(snapshot.state)) {
      systemLinkLastIdleChamberPublishMs = millis() - 300000UL;
    }
  }
}

static void processPendingSystemLinkPublish();
static void processPendingSystemLinkCalibration();

static void systemLinkWorkerTask(void *parameter) {
  (void)parameter;
  uint32_t lastTagPublishMs = 0;

  while (true) {
    if (otaUpdateInProgress) {
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }

    uint32_t now = millis();
    if (systemLinkHasRequiredConfig() && (lastTagPublishMs == 0 || (now - lastTagPublishMs) >= 1000UL)) {
      systemLinkPublishRealtimeTags();
      lastTagPublishMs = now;
    }

    processPendingSystemLinkPublish();
    processPendingSystemLinkCalibration();
    vTaskDelay(pdMS_TO_TICKS(250));
  }
}

static void initSystemLinkWorkerTask() {
  if (systemLinkWorkerTaskHandle != nullptr) {
    return;
  }

  xTaskCreatePinnedToCore(systemLinkWorkerTask,
                          "systemlink",
                          12288,
                          nullptr,
                          1,
                          &systemLinkWorkerTaskHandle,
                          0);
}

static void initSystemLinkTagTask() {
  initSystemLinkWorkerTask();
}

static void initSystemLinkPublishTask() {
  initSystemLinkWorkerTask();
}

static void systemLinkSetSessionPhase(const char *phase) {
  portENTER_CRITICAL(&systemLinkLock);
  if (systemLinkSession.active) {
    systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), phase);
  }
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkMarkRoastStarted(bool validationRun) {
  if (!systemLinkHasRequiredConfig()) {
    return;
  }

  String activeId = profileManager.getActiveProfileId();
  String activeName;
  profileManager.loadProfileMeta(activeId, activeName);
  if (validationRun) {
    activeName = "Validation Roast";
  }
  RoastTraceSample *traceBuffer = systemLinkAllocateTraceBuffer();
  RoasterState state = getRoasterStateSnapshot();

  systemLinkFreeTraceBuffer(systemLinkSession);

  portENTER_CRITICAL(&systemLinkLock);
  memset(&systemLinkSession, 0, sizeof(systemLinkSession));
  systemLinkSession.samples = traceBuffer;
  systemLinkSession.active = true;
  systemLinkSession.startedAtMs = millis();
  systemLinkSession.roastingStartedAtMs = 0;
  systemLinkSession.coolingStartedAtMs = 0;
  systemLinkSession.lastRecordedSecond = 0xFFFF;
  systemLinkSession.finalTargetTempF = profile.getFinalTargetTemp();
  systemLinkSession.setpointCount = profile.getSetpointCount();
  systemLinkSession.outcome = SYSTEMLINK_OUTCOME_NONE;
  systemLinkSession.traceOverflow = traceBuffer == nullptr;
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
  systemLinkTelemetry.state = state;
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
  RoasterState state = getRoasterStateSnapshot();
  portENTER_CRITICAL(&systemLinkLock);
  if (!systemLinkSession.active || !systemLinkIsTrackedRoastState(state)) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  uint16_t elapsedSeconds = static_cast<uint16_t>((millis() - systemLinkSession.startedAtMs) / 1000UL);
  if (elapsedSeconds == systemLinkSession.lastRecordedSecond) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  systemLinkSession.lastRecordedSecond = elapsedSeconds;
  if (systemLinkSession.samples == nullptr) {
    systemLinkSession.traceOverflow = true;
  } else if (systemLinkSession.sampleCount < SYSTEMLINK_MAX_TRACE_SAMPLES) {
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
  systemLinkTelemetry.state = state;
  portEXIT_CRITICAL(&systemLinkLock);
}

static void systemLinkFinishRoast(SystemLinkRoastOutcome outcome, const char *reason) {
  bool persistPending = false;
  bool publishWasDropped = false;
  bool publishWasQueued = false;
  RoasterState state = getRoasterStateSnapshot();
  RoastTraceSample *stalePublishTraceBuffer = nullptr;
  RoastTraceSample *droppedRoastTraceBuffer = nullptr;
  portENTER_CRITICAL(&systemLinkLock);
  if (!systemLinkSession.active) {
    portEXIT_CRITICAL(&systemLinkLock);
    return;
  }

  systemLinkSession.active = false;
  systemLinkSession.endedAtMs = millis();
  systemLinkAssignOutcome(systemLinkSession, outcome, reason, systemLinkSession.phase);
  if (outcome == SYSTEMLINK_OUTCOME_ERRORED) {
    systemLinkSession.faultBeanTempF = currentTemp;
    systemLinkSession.faultFanTempF = fanTemp;
    systemLinkSession.faultHeaterOutput = heaterOutputVal;
    systemLinkSession.faultState = state;
  }
  systemLinkCopyString(systemLinkSession.phase, sizeof(systemLinkSession.phase), "publish_pending");
  if (!systemLinkPublishPending && !systemLinkPublishInProgress) {
    RoastTraceSample *publishTraceBuffer = systemLinkSession.samples;
    stalePublishTraceBuffer = systemLinkPublishSession.samples;
    systemLinkSession.samples = nullptr;
    memcpy(&systemLinkPublishSession, &systemLinkSession, sizeof(SystemLinkRoastSession));
    systemLinkPublishSession.samples = publishTraceBuffer;
    systemLinkPublishPending = true;
    persistPending = true;
  } else if (!systemLinkQueuedPublishPending) {
    RoastTraceSample *publishTraceBuffer = systemLinkSession.samples;
    memcpy(&systemLinkQueuedPublishSession, &systemLinkSession, sizeof(SystemLinkRoastSession));
    systemLinkQueuedPublishSession.samples = publishTraceBuffer;
    systemLinkQueuedPublishPending = true;
    systemLinkSession.samples = nullptr;
    publishWasQueued = true;
  } else {
    droppedRoastTraceBuffer = systemLinkSession.samples;
    systemLinkSession.samples = nullptr;
    publishWasDropped = true;
  }
  systemLinkTelemetry.active = false;
  systemLinkTelemetry.state = state;
  portEXIT_CRITICAL(&systemLinkLock);

  if (stalePublishTraceBuffer != nullptr) {
    free(stalePublishTraceBuffer);
  }
  if (droppedRoastTraceBuffer != nullptr) {
    free(droppedRoastTraceBuffer);
  }
  if (publishWasDropped) {
    LOG_WARN("SystemLink: Previous publish still active, dropping completed roast publish");
  } else if (publishWasQueued) {
    LOG_WARN("SystemLink: Publish busy; queued completed roast in memory");
  }

  if (outcome == SYSTEMLINK_OUTCOME_ERRORED) {
    systemLinkUpdateLastFault(reason);
  }
  systemLinkUpdatePublishStatus(publishWasDropped ? "queue_full_dropped" : (publishWasQueued ? "queued" : "pending"));
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
  LOG_INFOF("SL phases s=%.1f r=%.1f c=%.1f", startSec, roastingSec, coolingSec);
  LOG_INFOF("SL heap=%u", static_cast<unsigned>(ESP.getFreeHeap()));

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
      LOG_INFOF("SL skip %s %.1fs", stepSpecs[index].name, stepSpecs[index].seconds);
      continue;
    }

    JsonObject step = steps.createNestedObject();
    SystemLinkConfig config = systemLinkGetConfigSnapshot();
    step["resultId"] = resultId;
    step["workspace"] = config.workspaceId;
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
  LOG_INFOF("SL steps body=%u count=%u", static_cast<unsigned>(body.length()), static_cast<unsigned>(steps.size()));

  String responseBody;
  int statusCode = -1;
  static const uint8_t MAX_ATTEMPTS = 3;
  for (uint8_t attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
    responseBody = "";
    statusCode = -1;

    bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/steps"), body, responseBody, statusCode);
    String apiError;
    bool hasApiError = systemLinkResponseContainsError(responseBody, apiError);
    if (ok && !hasApiError) {
      LOG_INFOF("SL steps=%u http=%d try=%u",
                static_cast<unsigned>(steps.size()),
                statusCode,
                static_cast<unsigned>(attempt));
      return true;
    }

    LOG_ERRORF("SystemLink: Step publish failed (status=%d, attempt=%u, heap=%u)",
               statusCode,
               static_cast<unsigned>(attempt),
               static_cast<unsigned>(ESP.getFreeHeap()));
    if (hasApiError && apiError.length() > 0) {
      LOG_ERRORF("SystemLink: Step publish API error: %s", apiError.c_str());
    }
    if (responseBody.length() > 0) {
      String snippet = responseBody.substring(0, 120);
      LOG_ERRORF("SystemLink: Step publish response: %s", snippet.c_str());
    }

    if (hasApiError || (statusCode >= 400 && statusCode < 500 && statusCode != 429)) {
      break;
    }

    delay(300);
    systemLinkFeedWatchdog();
  }

  return false;
}

static bool systemLinkCreateResult(SystemLinkRoastSession &session,
                                   const String &fileId,
                                   String &resultId,
                                   int &statusCode,
                                   bool &apiRejected) {
  apiRejected = false;
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  DynamicJsonDocument doc(4096);
  JsonObject result = doc.createNestedArray("results").createNestedObject();

  result["programName"] = session.profileName[0] != '\0' ? String("Coffee Roaster - ") + session.profileName : "Coffee Roaster Roast";
  JsonObject status = result.createNestedObject("status");
  status["statusType"] = systemLinkStatusTypeName(session.outcome);
  status["statusName"] = systemLinkStatusDisplayName(session.outcome);
  result["systemId"] = config.systemId;
  result["hostName"] = WiFi.getHostname();
  result["partNumber"] = "coffee-roaster";
  result["operator"] = "coffee-roaster";
  result["totalTimeInSeconds"] = static_cast<double>(session.endedAtMs - session.startedAtMs) / 1000.0;
  result["workspace"] = config.workspaceId;

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
  properties["phase"] = session.outcomePhase[0] != '\0' ? session.outcomePhase : session.phase;
  properties["firmwareVersion"] = VERSION;
  if (session.outcome == SYSTEMLINK_OUTCOME_ERRORED && !session.recoveredAfterReset) {
    properties["faultBeanTempF"] = String(session.faultBeanTempF, 1);
    properties["faultFanTempF"] = String(session.faultFanTempF, 1);
    properties["faultHeaterOutput"] = String(session.faultHeaterOutput, 1);
    properties["faultState"] = systemLinkStateName(session.faultState);
    properties["recentLogs"] = debugLogger.getLogsJSON(4);
  }
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
  bool ok = systemLinkPostJson(systemLinkBaseUrl("/nitestmonitor/v2/results"), body, responseBody, statusCode);
  if (!ok && statusCode != 201) {
    LOG_ERRORF("SystemLink: Result publish failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }

  String apiError;
  if (systemLinkResponseContainsError(responseBody, apiError)) {
    apiRejected = true;
    LOG_ERRORF("SystemLink: Result rejected by API (%d): %s", statusCode, apiError.c_str());
    return false;
  }

  if (!systemLinkParseCreatedResultId(responseBody, resultId)) {
    LOG_WARNF("SystemLink: Result published but result ID was not found in response: %s", responseBody.c_str());
  }

  LOG_INFOF("SL result=%s", systemLinkStatusTypeName(session.outcome).c_str());
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
  String fileId = systemLinkPublishFileId;
  String filename = String("roast-") + (systemLinkPublishSession.profileId[0] != '\0' ? systemLinkPublishSession.profileId : "session") + ".csv";
  if (fileId.length() == 0) {
    systemLinkUpdatePublishStatus("uploading_csv");
    systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "uploading_csv");
    if (systemLinkUploadTraceFile(filename, "text/csv", systemLinkPublishSession, uploadUri)) {
      fileId = systemLinkExtractIdFromUri(uploadUri);
      systemLinkPublishFileId = fileId;
    } else {
      systemLinkUpdatePublishStatus("csv_upload_failed");
    }
  }

  systemLinkUpdatePublishStatus("creating_result");
  systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "creating_result");
  String resultId;
  int resultStatusCode = -1;
  bool apiRejected = false;
  bool published = systemLinkCreateResult(systemLinkPublishSession, fileId, resultId, resultStatusCode, apiRejected);
  bool permanentFailure = apiRejected || (!published && resultStatusCode >= 400 && resultStatusCode < 500 && resultStatusCode != 429);
  bool stepsPublished = published && resultId.length() > 0;
  if (published && resultId.length() > 0) {
    systemLinkUpdatePublishStatus("creating_steps");
    systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "creating_steps");
    stepsPublished = systemLinkCreatePhaseSteps(resultId, systemLinkPublishSession);
  }
  RoastTraceSample *completedPublishTraceBuffer = nullptr;
  bool promotedQueuedPublish = false;

  portENTER_CRITICAL(&systemLinkLock);
  systemLinkPublishInProgress = false;
  if (published || permanentFailure) {
    completedPublishTraceBuffer = systemLinkPublishSession.samples;
    systemLinkPublishSession.samples = nullptr;
    memset(&systemLinkPublishSession, 0, sizeof(SystemLinkRoastSession));
    if (systemLinkQueuedPublishPending) {
      memcpy(&systemLinkPublishSession,
             &systemLinkQueuedPublishSession,
             sizeof(SystemLinkRoastSession));
      memset(&systemLinkQueuedPublishSession, 0, sizeof(SystemLinkRoastSession));
      systemLinkQueuedPublishPending = false;
      promotedQueuedPublish = true;
    } else {
      systemLinkPublishPending = false;
    }
  }
  portEXIT_CRITICAL(&systemLinkLock);

  if (completedPublishTraceBuffer != nullptr) {
    free(completedPublishTraceBuffer);
  }

  if (published) {
    systemLinkPublishFileId = "";
    if (promotedQueuedPublish) {
      systemLinkUpdatePublishStatus("pending");
      systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "publish_pending");
    } else {
      systemLinkUpdatePublishStatus(stepsPublished ? "published" : "published_steps_failed");
      systemLinkClearBreadcrumb();
    }
  } else if (permanentFailure) {
    systemLinkPublishFileId = "";
    LOG_ERRORF("SystemLink: Discarding permanently rejected result (HTTP %d)", resultStatusCode);
    if (promotedQueuedPublish) {
      systemLinkUpdatePublishStatus("pending");
      systemLinkPersistBreadcrumb(systemLinkPublishSession, false, true, "publish_pending");
    } else {
      char rejectedStatus[SYSTEMLINK_STATUS_MAX];
      if (apiRejected) {
        systemLinkCopyString(rejectedStatus, sizeof(rejectedStatus), "rejected_api");
      } else {
        snprintf(rejectedStatus, sizeof(rejectedStatus), "rejected_%d", resultStatusCode);
      }
      systemLinkUpdatePublishStatus(rejectedStatus);
      systemLinkClearBreadcrumb();
    }
  } else {
    systemLinkUpdatePublishStatus("result_publish_failed");
  }
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
                                            const char *csvContent,
                                            size_t csvLength,
                                            String &uploadedUri) {
  struct ActiveRequestGuard {
    ActiveRequestGuard() { systemLinkActiveRequestEnter(); }
    ~ActiveRequestGuard() { systemLinkActiveRequestLeave(); }
  } activeRequestGuard;

  uploadedUri = "";

  if (otaUpdateInProgress) {
    LOG_WARN("SystemLink: Skipping calibration upload because OTA is in progress");
    return false;
  }
  String host;
  uint16_t port;
  if (!systemLinkParseApiEndpoint(host, port)) {
    LOG_ERROR("SystemLink: Invalid API URL for calibration upload");
    return false;
  }

  const String boundary = "----CoffeeRoasterCalibrationBoundary";
  SystemLinkConfig config = systemLinkGetConfigSnapshot();
  String prefix;
  prefix.reserve(filename.length() + boundary.length() + 128);
  prefix += "--" + boundary + "\r\n";
  prefix += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
  prefix += "Content-Type: text/csv\r\n\r\n";

  String suffix = "\r\n--" + boundary + "--\r\n";
  size_t contentLength = prefix.length() + csvLength + suffix.length();

  WiFiClientSecure client;
  systemLinkConfigureTls(client);
  if (!client.connect(host.c_str(), port)) {
    LOG_ERRORF("SystemLink: Calibration upload connect failed to %s:%u", host.c_str(), static_cast<unsigned>(port));
    return false;
  }

  String requestPath = String("/nifile/v1/service-groups/Default/upload-files?workspace=") + config.workspaceId;
  client.printf("POST %s HTTP/1.1\r\n", requestPath.c_str());
  client.printf("Host: %s\r\n", host.c_str());
  client.print("Connection: close\r\n");
  client.print("Accept: application/json\r\n");
  client.printf("x-ni-api-key: %s\r\n", config.apiKey);
  client.printf("Content-Type: multipart/form-data; boundary=%s\r\n", boundary.c_str());
  client.printf("Content-Length: %u\r\n\r\n", static_cast<unsigned>(contentLength));

  client.print(prefix);
  // Write CSV in chunks to avoid large single write
  const char *data = csvContent;
  size_t remaining = csvLength;
  while (remaining > 0) {
    size_t chunk = min(remaining, static_cast<size_t>(1024));
    client.write(reinterpret_cast<const uint8_t *>(data), chunk);
    data += chunk;
    remaining -= chunk;
    systemLinkFeedWatchdog();
  }
  client.print(suffix);
  client.flush();

  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && millis() - waitStart < 7000UL) {
    delay(10);
    systemLinkFeedWatchdog();
  }

  if (!client.available()) {
    client.stop();
    LOG_ERROR("SystemLink: Calibration upload timed out");
    return false;
  }

  String statusLine = client.readStringUntil('\n');
  statusLine.trim();
  int statusCode = -1;
  int firstSpace = statusLine.indexOf(' ');
  if (firstSpace >= 0 && statusLine.length() >= static_cast<unsigned>(firstSpace + 4)) {
    statusCode = statusLine.substring(firstSpace + 1, firstSpace + 4).toInt();
  }

  while (client.available() || client.connected()) {
    String headerLine = client.readStringUntil('\n');
    if (headerLine == "\r" || headerLine.length() == 0) break;
  }

  String responseBody = client.readString();
  client.stop();

  if (statusCode < 200 || statusCode >= 300) {
    LOG_ERRORF("SystemLink: Calibration upload failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }

  DynamicJsonDocument doc(1024);
  if (deserializeJson(doc, responseBody)) {
    LOG_ERRORF("SystemLink: Failed to parse calibration upload response: %s", responseBody.c_str());
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

  LOG_ERROR("SystemLink: No URI in calibration upload response");
  return false;
}

// Create a test result for a calibration run.
static bool systemLinkCreateCalibrationResult(const StepResponseTuner::Summary &summary,
                                               const String &fileId,
                                               String &resultId) {
  SystemLinkConfig config = systemLinkGetConfigSnapshot();

  DynamicJsonDocument doc(4096);
  JsonObject result = doc.createNestedArray("results").createNestedObject();

  result["programName"] = "Coffee Roaster - PID Calibration (Step-Response)";
  JsonObject status = result.createNestedObject("status");
  status["statusType"] = summary.passed ? "PASSED" : "FAILED";
  status["statusName"] = summary.passed ? "Calibration Passed" : "Calibration Failed";
  result["systemId"] = config.systemId;
  result["hostName"] = WiFi.getHostname();
  result["partNumber"] = "coffee-roaster";
  result["operator"] = "coffee-roaster";
  result["workspace"] = config.workspaceId;

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
  if (!ok && statusCode != 201) {
    LOG_ERRORF("SystemLink: Calibration result publish failed (%d): %s", statusCode, responseBody.c_str());
    return false;
  }
  String apiError;
  if (systemLinkResponseContainsError(responseBody, apiError)) {
    LOG_ERRORF("SystemLink: Calibration result rejected: %s", apiError.c_str());
    return false;
  }

  if (!systemLinkParseCreatedResultId(responseBody, resultId)) {
    LOG_WARN("SystemLink: Calibration result published but ID not found in response");
  }

  LOG_INFOF("SL calibration id=%s", resultId.c_str());
  return true;
}

// Top-level function: build CSV, upload file, create test result.
// Called from the main .ino when step-response tuning completes.
static void systemLinkPublishCalibration(const StepResponseTuner &tuner) {
  if (!systemLinkHasRequiredConfig()) {
    LOG_INFO("SystemLink: Skipping calibration publish (not configured)");
    return;
  }

  LOG_INFO("SystemLink: Publishing step-response calibration data");

  String csvContent = systemLinkBuildCalibrationCsv(tuner);
  if (csvContent.length() == 0) {
    LOG_WARN("SystemLink: Empty calibration CSV, skipping publish");
    return;
  }

  char *csvCopy = static_cast<char *>(malloc(csvContent.length() + 1));
  if (csvCopy == nullptr) {
    LOG_ERROR("SystemLink: Could not allocate calibration publish payload");
    return;
  }
  memcpy(csvCopy, csvContent.c_str(), csvContent.length() + 1);

  StepResponseTuner::Summary summary = tuner.getSummary();
  portENTER_CRITICAL(&systemLinkCalibrationLock);
  if (systemLinkCalibrationPublishPending) {
    portEXIT_CRITICAL(&systemLinkCalibrationLock);
    free(csvCopy);
    LOG_WARN("SystemLink: Calibration publish already queued; dropping newer calibration payload");
    return;
  }
  systemLinkPendingCalibrationSummary = summary;
  systemLinkPendingCalibrationCsv = csvCopy;
  systemLinkPendingCalibrationCsvLength = csvContent.length();
  systemLinkCalibrationPublishPending = true;
  portEXIT_CRITICAL(&systemLinkCalibrationLock);

  LOG_INFO("SystemLink: Calibration publish queued for background worker");
}

static void processPendingSystemLinkCalibration() {
  if (!systemLinkHasRequiredConfig() || WiFi.status() != WL_CONNECTED || otaUpdateInProgress) {
    return;
  }
  if (millis() - systemLinkLastCalibrationAttemptMs < 15000UL) {
    return;
  }

  char *csvContent = nullptr;
  size_t csvLength = 0;
  StepResponseTuner::Summary summary;
  portENTER_CRITICAL(&systemLinkCalibrationLock);
  if (systemLinkCalibrationPublishPending) {
    csvContent = systemLinkPendingCalibrationCsv;
    csvLength = systemLinkPendingCalibrationCsvLength;
    summary = systemLinkPendingCalibrationSummary;
  }
  portEXIT_CRITICAL(&systemLinkCalibrationLock);
  if (csvContent == nullptr) {
    return;
  }
  systemLinkLastCalibrationAttemptMs = millis();

  String uploadUri;
  String fileId(systemLinkPendingCalibrationFileId);
  if (fileId.length() == 0) {
    if (!systemLinkUploadCalibrationCsv("calibration-step-response.csv", csvContent, csvLength, uploadUri)) {
      return;
    }
    fileId = systemLinkExtractIdFromUri(uploadUri);
    systemLinkCopyString(systemLinkPendingCalibrationFileId, sizeof(systemLinkPendingCalibrationFileId), fileId);
  }

  String resultId;
  if (systemLinkCreateCalibrationResult(summary, fileId, resultId)) {
    portENTER_CRITICAL(&systemLinkCalibrationLock);
    systemLinkPendingCalibrationCsv = nullptr;
    systemLinkPendingCalibrationCsvLength = 0;
    systemLinkCalibrationPublishPending = false;
    systemLinkPendingCalibrationFileId[0] = '\0';
    portEXIT_CRITICAL(&systemLinkCalibrationLock);
    free(csvContent);
    LOG_INFOF("SL calibration created=%s", resultId.c_str());
  }
}

#endif // SYSTEMLINK_HPP