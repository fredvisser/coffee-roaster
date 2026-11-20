#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_random.h>
#include <freertos/semphr.h>
#include <mbedtls/sha256.h>
#include <time.h>
#include "PairingSessions.hpp"

static portMUX_TYPE serverSecurityMux = portMUX_INITIALIZER_UNLOCKED;
static char serverSecurityHostname[64] = "roaster-dev";
static bool serverSecurityMdnsPending = false;
static SemaphoreHandle_t pairingMutex = nullptr;
static Preferences pairingPrefs;
static bool pairingStorageReady = false;
static PairingSessions pairing;
static int pairingDisplayedCode = -1;

struct PairingLock {
  PairingLock() { xSemaphoreTake(pairingMutex, portMAX_DELAY); }
  ~PairingLock() { xSemaphoreGive(pairingMutex); }
};

static bool serverSecurityHostnameIsValid(const String &hostname) {
  if (!hostname.length() || hostname.length() > 63 || hostname.startsWith("-") || hostname.endsWith("-")) return false;
  for (size_t i = 0; i < hostname.length(); i++) {
    char c = hostname[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')) return false;
  }
  return true;
}

static String serverSecurityGetHostname() {
  char hostname[sizeof(serverSecurityHostname)];
  portENTER_CRITICAL(&serverSecurityMux);
  memcpy(hostname, serverSecurityHostname, sizeof(hostname));
  portEXIT_CRITICAL(&serverSecurityMux);
  return hostname;
}

static void pairingSlotKey(unsigned slot, char (&key)[8]) {
  snprintf(key, sizeof(key), "sess%u", slot);
}

static void serverSecurityInitialize(Preferences &prefs) {
  String hostname = prefs.getString("web_host", "roaster-dev");
  hostname.trim();
  hostname.toLowerCase();
  if (serverSecurityHostnameIsValid(hostname)) hostname.toCharArray(serverSecurityHostname, sizeof(serverSecurityHostname));
  pairingMutex = xSemaphoreCreateMutex();
  pairingStorageReady = pairingMutex && pairingPrefs.begin("web-pair-v1");
  if (!pairingStorageReady) {
    Serial.println("Web pairing storage unavailable");
    return;
  }
  for (unsigned i = 0; i < PairingSessions::Capacity; i++) {
    char key[8];
    pairingSlotKey(i, key);
    auto &session = pairing.sessions[i];
    if (pairingPrefs.getBytesLength(key) == sizeof(session) &&
        pairingPrefs.getBytes(key, &session, sizeof(session)) == sizeof(session)) {
      session.name[sizeof(session.name) - 1] = '\0';
    } else {
      session = {};
    }
  }
}

static bool serverSecurityStoreHostname(Preferences &prefs, String hostname, String &error) {
  hostname.trim();
  hostname.toLowerCase();
  if (!serverSecurityHostnameIsValid(hostname)) {
    error = "hostname_must_be_a_dns_label";
    return false;
  }
  if (hostname == serverSecurityGetHostname()) return true;
  if (!prefs.putString("web_host", hostname)) {
    error = "hostname_persist_failed";
    return false;
  }
  portENTER_CRITICAL(&serverSecurityMux);
  hostname.toCharArray(serverSecurityHostname, sizeof(serverSecurityHostname));
  serverSecurityMdnsPending = true;
  portEXIT_CRITICAL(&serverSecurityMux);
  return true;
}

static bool serverSecurityTakeHostnameChange() {
  portENTER_CRITICAL(&serverSecurityMux);
  bool pending = serverSecurityMdnsPending;
  serverSecurityMdnsPending = false;
  portEXIT_CRITICAL(&serverSecurityMux);
  return pending;
}

static void pairingHex(const uint8_t *bytes, size_t count, char *out) {
  static const char digits[] = "0123456789abcdef";
  for (size_t i = 0; i < count; ++i) {
    out[2 * i] = digits[bytes[i] >> 4];
    out[2 * i + 1] = digits[bytes[i] & 15];
  }
  out[2 * count] = '\0';
}

static void pairingToken(char (&token)[65], uint8_t (&digest)[32]) {
  uint8_t randomBytes[32];
  esp_fill_random(randomBytes, sizeof(randomBytes));
  pairingHex(randomBytes, sizeof(randomBytes), token);
  mbedtls_sha256(reinterpret_cast<const uint8_t *>(token), 64, digest, 0);
}

static bool pairingDigest(const char *token, size_t length, uint8_t (&digest)[32]) {
  if (length != 64) return false;
  for (size_t i = 0; i < length; ++i) {
    if (!((token[i] >= '0' && token[i] <= '9') || (token[i] >= 'a' && token[i] <= 'f'))) return false;
  }
  return mbedtls_sha256(reinterpret_cast<const uint8_t *>(token), length, digest, 0) == 0;
}

static const char pairingCookie[] = "roaster_session";

static bool pairingSameOrigin(AsyncWebServerRequest *request) {
  const auto *origin = request->getHeader("Origin");
  const auto *referer = request->getHeader("Referer");
  const String authority = "http://" + request->host();
  if (origin) return origin->value().equalsIgnoreCase(authority);
  return referer && referer->value().startsWith(authority + "/");
}

// Call under PairingLock; upload chunks arrive before server middleware.
static int pairingFindClient(AsyncWebServerRequest *request) {
  uint8_t digest[32];
  const auto *auth = request->getHeader("Authorization");
  if (auth && auth->value().startsWith("Bearer ") &&
      pairingDigest(auth->value().c_str() + 7, auth->value().length() - 7, digest)) return pairing.lookup(digest, true);

  const bool safe = (request->method() == HTTP_GET || request->method() == HTTP_HEAD) &&
                    !request->url().startsWith("/ota/start");
  if (!safe && !pairingSameOrigin(request)) return -1;
  const auto *cookie = request->getHeader("Cookie");
  if (!cookie) return -1;
  const String &value = cookie->value();
  for (int start = 0; start < static_cast<int>(value.length());) {
    while (value[start] == ' ') ++start;
    int end = value.indexOf(';', start);
    if (end < 0) end = value.length();
    constexpr size_t prefix = sizeof(pairingCookie);
    if (end - start == 64 + prefix && !strncmp(value.c_str() + start, pairingCookie, prefix - 1) &&
        value[start + prefix - 1] == '=' && pairingDigest(value.c_str() + start + prefix, 64, digest)) {
      int slot = pairing.lookup(digest, false);
      if (slot >= 0) return slot;
    }
    start = end + 1;
  }
  return -1;
}

static bool serverSecurityIsAuthenticated(AsyncWebServerRequest *request) {
  if (!pairingStorageReady) return false;
  PairingLock lock;
  return pairingFindClient(request) >= 0;
}

static void pairingReply(AsyncWebServerRequest *request, int status, const String &body, const char *cookie = nullptr) {
  auto *response = request->beginResponse(status, "application/json", body);
  response->addHeader("Cache-Control", "no-store");
  if (cookie) response->addHeader("Set-Cookie", String(pairingCookie) + "=" + cookie + "; Path=/; Max-Age=31536000; HttpOnly; SameSite=Strict");
  request->send(response);
}

static bool pairingPage(const String &url) {
  return url == "/" || url == "/console" || url == "/profile" || url == "/pid" ||
         url == "/server" || url == "/systemlink" || url == "/update";
}

class RoasterAuthenticationMiddleware : public AsyncMiddleware {
public:
  void run(AsyncWebServerRequest *request, ArMiddlewareNext next) override {
    const String &url = request->url();
    const auto method = request->method();
    if ((method == HTTP_GET && (url == "/WebSocket" || url == "/pair")) ||
        (method == HTTP_POST && (url == "/api/pair-request" || url == "/api/pair-claim")) ||
        serverSecurityIsAuthenticated(request)) {
      next();
    } else if (method == HTTP_GET && pairingPage(url)) {
      request->redirect("/pair");
    } else {
      pairingReply(request, 401, "{\"error\":\"pairing_required\"}");
    }
  }
};

static RoasterAuthenticationMiddleware serverSecurityAuthMiddleware;

static void pairingRegisterApi(AsyncWebServer &server) {
  server.on("/api/pair-request", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool cli = request->hasParam("cli") && request->getParam("cli")->value() == "1";
    if ((!cli || request->hasHeader("Origin")) && !pairingSameOrigin(request))
      return pairingReply(request, 403, "{\"error\":\"wrong_origin\"}");
    if (!pairingStorageReady) return pairingReply(request, 503, "{\"error\":\"pairing_storage_unavailable\"}");
    PairingSessions::Session candidate{};
    candidate.cli = cli;
    String name = request->hasParam("name") ? request->getParam("name")->value() : (cli ? "Firmware CLI" : "Browser");
    name.trim();
    if (!name.length() || name.length() >= sizeof(candidate.name))
      return pairingReply(request, 400, "{\"error\":\"name_must_be_1_to_32_bytes\"}");
    name.toCharArray(candidate.name, sizeof(candidate.name));
    char ticket[65];
    pairingToken(ticket, candidate.digest);
    unsigned code = esp_random() % 10000;
    {
      PairingLock lock;
      if (!pairing.request(millis(), candidate, code))
        return pairingReply(request, 409, "{\"error\":\"Open Web access on the roaster and tap Pair browser/CLI. If the session list is full, revoke one first.\"}");
    }
    char body[96];
    snprintf(body, sizeof(body), "{\"code\":\"%04u\",\"ticket\":\"%s\"}", code, ticket);
    pairingReply(request, 200, body);
  });

  server.on("/api/pair-claim", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!pairingStorageReady) return pairingReply(request, 503, "{\"error\":\"pairing_storage_unavailable\"}");
    uint8_t digest[32];
    const auto *ticket = request->getHeader("X-Roaster-Pair");
    if (!ticket || !pairingDigest(ticket->value().c_str(), ticket->value().length(), digest))
      return pairingReply(request, 400, "{\"error\":\"invalid_ticket\"}");
    char token[65];
    PairingSessions::Session candidate{};
    {
      PairingLock lock;
      uint32_t now = millis();
      int slot = pairing.claim(now, digest, candidate);
      if (slot < 0) {
        if (pairing.isPending(now, digest)) return pairingReply(request, 202, "{\"status\":\"pending\"}");
        return pairingReply(request, 410, "{\"error\":\"Pairing expired or was cancelled. Open pairing on the roaster and retry.\"}");
      }
      if ((!candidate.cli || request->hasHeader("Origin")) && !pairingSameOrigin(request))
        return pairingReply(request, 403, "{\"error\":\"wrong_origin\"}");
      char key[8];
      pairingSlotKey(slot, key);
      pairingToken(token, candidate.digest);
      const auto *clientTime = request->getHeader("X-Roaster-Paired-At");
      candidate.pairedAt = PairingSessions::pairedTimestamp(time(nullptr),
                                                           clientTime ? clientTime->value().c_str() : nullptr);
      if (pairingPrefs.putBytes(key, &candidate, sizeof(candidate)) != sizeof(candidate))
        return pairingReply(request, 503, "{\"error\":\"session_persist_failed\"}");
      pairing.sessions[slot] = candidate;
      pairing.close();
    }
    if (candidate.cli) pairingReply(request, 200, String("{\"token\":\"") + token + "\"}");
    else pairingReply(request, 200, "{\"ok\":true}", token);
  });

  server.on("/api/pair-sessions", HTTP_GET, [](AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(2048);
    JsonArray sessions = doc.to<JsonArray>();
    {
      PairingLock lock;
      int current = pairingFindClient(request);
      if (current < 0) return pairingReply(request, 401, "{\"error\":\"pairing_required\"}");
      for (unsigned i = 0; i < PairingSessions::Capacity; i++) {
        const auto &session = pairing.sessions[i];
        if (!session.name[0]) continue;
        char id[17];
        pairingHex(session.digest, 8, id);
        auto item = sessions.createNestedObject();
        item["id"] = String(id);
        item["name"] = session.name;
        item["cli"] = session.cli;
        item["pairedAt"] = session.pairedAt;
        item["current"] = static_cast<int>(i) == current;
      }
    }
    String body;
    serializeJson(doc, body);
    pairingReply(request, 200, body);
  });

  server.on("/api/pair-revoke", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (!request->hasParam("id")) return pairingReply(request, 400, "{\"error\":\"missing_session_id\"}");
    const String &wanted = request->getParam("id")->value();
    PairingLock lock;
    if (pairingFindClient(request) < 0) return pairingReply(request, 401, "{\"error\":\"pairing_required\"}");
    for (unsigned i = 0; i < PairingSessions::Capacity; i++) {
      auto &session = pairing.sessions[i];
      if (!session.name[0]) continue;
      char id[17], key[8];
      pairingHex(session.digest, 8, id);
      if (wanted != id) continue;
      pairingSlotKey(i, key);
      if (!pairingPrefs.remove(key)) return pairingReply(request, 503, "{\"error\":\"session_revoke_failed\"}");
      session = {};
      return pairingReply(request, 200, "{\"ok\":true}");
    }
    pairingReply(request, 404, "{\"error\":\"session_not_found\"}");
  });
}
