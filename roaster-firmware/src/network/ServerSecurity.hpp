#ifndef SERVER_SECURITY_HPP
#define SERVER_SECURITY_HPP

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <esp_system.h>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>

static const char SERVER_SECURITY_USERNAME[] = "admin";
static const char SERVER_SECURITY_REALM[] = "Coffee Roaster";
static const char SERVER_SECURITY_HOST_KEY[] = "web_host";
static const char SERVER_SECURITY_PASSWORD_KEY[] = "web_pass";
static const size_t SERVER_SECURITY_HOST_MAX = 63;
static const size_t SERVER_SECURITY_PASSWORD_LENGTH = 8;

static portMUX_TYPE serverSecurityMux = portMUX_INITIALIZER_UNLOCKED;
static char serverSecurityHostname[SERVER_SECURITY_HOST_MAX + 1] = "roaster-dev";
static char serverSecurityPassword[SERVER_SECURITY_PASSWORD_LENGTH + 1] = "";
static volatile bool serverSecurityMdnsPending = false;
static volatile bool serverSecurityDisplayPending = false;

static bool serverSecurityHostnameIsValid(const String &hostname) {
  if (hostname.length() == 0 || hostname.length() > SERVER_SECURITY_HOST_MAX) {
    return false;
  }

  for (size_t index = 0; index < hostname.length(); index++) {
    const char character = hostname[index];
    const bool alpha = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    const bool digit = character >= '0' && character <= '9';
    if (!alpha && !digit && character != '-') {
      return false;
    }
    if ((index == 0 || index + 1 == hostname.length()) && character == '-') {
      return false;
    }
  }

  return true;
}

static bool serverSecurityPasswordIsValid(const String &password) {
  if (password.length() != SERVER_SECURITY_PASSWORD_LENGTH) {
    return false;
  }

  for (size_t index = 0; index < password.length(); index++) {
    const char character = password[index];
    const bool alpha = (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z');
    const bool digit = character >= '0' && character <= '9';
    if (!alpha && !digit) {
      return false;
    }
  }

  return true;
}

static void serverSecurityGeneratePassword(char *password) {
  static const char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
  const size_t alphabetLength = sizeof(alphabet) - 1;
  for (size_t index = 0; index < SERVER_SECURITY_PASSWORD_LENGTH; index++) {
    password[index] = alphabet[esp_random() % alphabetLength];
  }
  password[SERVER_SECURITY_PASSWORD_LENGTH] = '\0';
}

static void serverSecurityInitialize(Preferences &prefs) {
  String storedHostname = prefs.getString(SERVER_SECURITY_HOST_KEY, "roaster-dev");
  String hostname = storedHostname;
  hostname.trim();
  hostname.toLowerCase();
  if (!serverSecurityHostnameIsValid(hostname)) {
    hostname = "roaster-dev";
  }

  String password = prefs.getString(SERVER_SECURITY_PASSWORD_KEY, "");
  if (!serverSecurityPasswordIsValid(password)) {
    char generatedPassword[SERVER_SECURITY_PASSWORD_LENGTH + 1];
    serverSecurityGeneratePassword(generatedPassword);
    password = generatedPassword;
    prefs.putString(SERVER_SECURITY_PASSWORD_KEY, password);
  }

  if (hostname != storedHostname) {
    prefs.putString(SERVER_SECURITY_HOST_KEY, hostname);
  }
  portENTER_CRITICAL(&serverSecurityMux);
  hostname.toCharArray(serverSecurityHostname, sizeof(serverSecurityHostname));
  password.toCharArray(serverSecurityPassword, sizeof(serverSecurityPassword));
  portEXIT_CRITICAL(&serverSecurityMux);
}

static String serverSecurityGetHostname() {
  char hostname[SERVER_SECURITY_HOST_MAX + 1];
  portENTER_CRITICAL(&serverSecurityMux);
  memcpy(hostname, serverSecurityHostname, sizeof(hostname));
  portEXIT_CRITICAL(&serverSecurityMux);
  hostname[SERVER_SECURITY_HOST_MAX] = '\0';
  return String(hostname);
}

static String serverSecurityGetPassword() {
  char password[SERVER_SECURITY_PASSWORD_LENGTH + 1];
  portENTER_CRITICAL(&serverSecurityMux);
  memcpy(password, serverSecurityPassword, sizeof(password));
  portEXIT_CRITICAL(&serverSecurityMux);
  password[SERVER_SECURITY_PASSWORD_LENGTH] = '\0';
  return String(password);
}

static bool serverSecurityStoreSettings(Preferences &prefs,
                                        String hostname,
                                        const String &newPassword,
                                        bool changePassword,
                                        bool &hostnameChanged,
                                        bool &passwordChanged,
                                        String &error) {
  hostname.trim();
  hostname.toLowerCase();
  if (!serverSecurityHostnameIsValid(hostname)) {
    error = "hostname_must_be_a_dns_label";
    return false;
  }
  if (changePassword && !serverSecurityPasswordIsValid(newPassword)) {
    error = "password_must_be_8_alphanumeric_characters";
    return false;
  }

  const String oldHostname = serverSecurityGetHostname();
  const String oldPassword = serverSecurityGetPassword();
  hostnameChanged = hostname != oldHostname;
  passwordChanged = changePassword && newPassword != oldPassword;
  if (!hostnameChanged && !passwordChanged) {
    return true;
  }

  if (passwordChanged && prefs.putString(SERVER_SECURITY_PASSWORD_KEY, newPassword) == 0) {
    error = "password_persist_failed";
    return false;
  }
  if (hostnameChanged && prefs.putString(SERVER_SECURITY_HOST_KEY, hostname) == 0) {
    if (passwordChanged) {
      prefs.putString(SERVER_SECURITY_PASSWORD_KEY, oldPassword);
    }
    error = "hostname_persist_failed";
    return false;
  }

  portENTER_CRITICAL(&serverSecurityMux);
  hostname.toCharArray(serverSecurityHostname, sizeof(serverSecurityHostname));
  if (passwordChanged) {
    newPassword.toCharArray(serverSecurityPassword, sizeof(serverSecurityPassword));
  }
  serverSecurityMdnsPending = serverSecurityMdnsPending || hostnameChanged;
  serverSecurityDisplayPending = serverSecurityDisplayPending || hostnameChanged || passwordChanged;
  portEXIT_CRITICAL(&serverSecurityMux);
  return true;
}

static void serverSecurityTakePending(bool &restartMdns, bool &refreshDisplay) {
  portENTER_CRITICAL(&serverSecurityMux);
  restartMdns = serverSecurityMdnsPending;
  refreshDisplay = serverSecurityDisplayPending;
  serverSecurityMdnsPending = false;
  serverSecurityDisplayPending = false;
  portEXIT_CRITICAL(&serverSecurityMux);
}

static bool serverSecurityIsAuthenticated(AsyncWebServerRequest *request) {
  char password[SERVER_SECURITY_PASSWORD_LENGTH + 1];
  portENTER_CRITICAL(&serverSecurityMux);
  memcpy(password, serverSecurityPassword, sizeof(password));
  portEXIT_CRITICAL(&serverSecurityMux);
  password[SERVER_SECURITY_PASSWORD_LENGTH] = '\0';
  return request->authenticate(SERVER_SECURITY_USERNAME, password, SERVER_SECURITY_REALM);
}

static bool serverSecurityRequireAuthentication(AsyncWebServerRequest *request) {
  if (serverSecurityIsAuthenticated(request)) {
    return true;
  }
  request->requestAuthentication(AsyncAuthType::AUTH_BASIC, SERVER_SECURITY_REALM, "Authentication required");
  return false;
}

class RoasterAuthenticationMiddleware : public AsyncMiddleware {
public:
  void run(AsyncWebServerRequest *request, ArMiddlewareNext next) override {
    const bool isWebSocketUpgrade = request->method() == HTTP_GET && request->url() == "/WebSocket";
    if (isWebSocketUpgrade || serverSecurityRequireAuthentication(request)) {
      next();
    }
  }
};

static RoasterAuthenticationMiddleware serverSecurityAuthMiddleware;

#endif