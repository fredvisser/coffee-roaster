#include <WiFi.h>
#include "../../src/platform/BoardConfig.hpp"
#include "../../src/display/DisplayAdapter.hpp"

#ifndef WIFI_TEST_SSID
#define WIFI_TEST_SSID ""
#endif

#ifndef WIFI_TEST_PASSWORD
#define WIFI_TEST_PASSWORD ""
#endif

static const char *kSsid = WIFI_TEST_SSID;
static const char *kPassword = WIFI_TEST_PASSWORD;
static unsigned long lastStatusLogAtMs = 0;

const char *statusName(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:
      return "WL_IDLE_STATUS";
    case WL_NO_SSID_AVAIL:
      return "WL_NO_SSID_AVAIL";
    case WL_SCAN_COMPLETED:
      return "WL_SCAN_COMPLETED";
    case WL_CONNECTED:
      return "WL_CONNECTED";
    case WL_CONNECT_FAILED:
      return "WL_CONNECT_FAILED";
    case WL_CONNECTION_LOST:
      return "WL_CONNECTION_LOST";
    case WL_DISCONNECTED:
      return "WL_DISCONNECTED";
    default:
      return "WL_UNKNOWN";
  }
}

void onWifiEvent(arduino_event_id_t event, arduino_event_info_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_SCAN_DONE:
      Serial.printf("[wifi_probe] scan done: status=%u results=%u\n", info.wifi_scan_done.status, info.wifi_scan_done.number);
      break;
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial.printf("[wifi_probe] connected: channel=%u auth=%d\n", info.wifi_sta_connected.channel, info.wifi_sta_connected.authmode);
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[wifi_probe] disconnected: reason=%s (%d)\n", WiFi.disconnectReasonName(static_cast<wifi_err_reason_t>(info.wifi_sta_disconnected.reason)), info.wifi_sta_disconnected.reason);
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[wifi_probe] got ip: %s\n", WiFi.localIP().toString().c_str());
      break;
    default:
      break;
  }
}

void logVisibleNetworks() {
  Serial.printf("[wifi_probe] scanning for '%s'\n", kSsid);
  int networkCount = WiFi.scanNetworks(false, true);
  Serial.printf("[wifi_probe] scanNetworks returned %d\n", networkCount);
  for (int index = 0; index < networkCount; ++index) {
    Serial.printf(
      "[wifi_probe] ssid[%d]='%s' rssi=%d channel=%d auth=%d\n",
      index,
      WiFi.SSID(index).c_str(),
      WiFi.RSSI(index),
      WiFi.channel(index),
      static_cast<int>(WiFi.encryptionType(index))
    );
  }
  WiFi.scanDelete();
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println();
  Serial.println("[wifi_probe] boot");

  displayBegin(BoardConfig::DisplayBaudRate);

  WiFi.onEvent(onWifiEvent);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.disconnect(true);

  logVisibleNetworks();

  Serial.printf("[wifi_probe] connecting: ssidLen=%u passwordLen=%u\n", strlen(kSsid), strlen(kPassword));
  WiFi.begin(kSsid, kPassword);
}

void loop() {
  displayTick();

  if (millis() - lastStatusLogAtMs < 2000) {
    return;
  }

  lastStatusLogAtMs = millis();
  Serial.printf("[wifi_probe] status=%s (%d) ip=%s rssi=%d\n", statusName(WiFi.status()), static_cast<int>(WiFi.status()), WiFi.localIP().toString().c_str(), WiFi.RSSI());
}