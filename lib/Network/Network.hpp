#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

JsonDocument wsRequestDoc;
String json;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/WebSocket");

extern double currentTemp;
extern double setpointTemp;
extern byte setpointFanSpeed;

// Wifi credentials struct
struct WifiCredentials
{
    String ssid;
    String password;
};

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
    {
        data[len] = 0;
        Serial.printf("%s\n", (char *)data);
        deserializeJson(wsRequestDoc, data);

        if (wsRequestDoc["command"] == "getData")
        {
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
             void *arg, uint8_t *data, size_t len)
{
    switch (type)
    {
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

void initWebSocket()
{
    ws.onEvent(onEvent);
    server.addHandler(&ws);
}

String initializeWifi(WifiCredentials wifiCredentials)
{
    // Connect to Wi-Fi
    WiFi.begin(wifiCredentials.ssid, wifiCredentials.password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 10)
    {
        delay(1000);
        Serial.println("Connecting to WiFi..");
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("Failed to connect to WiFi");
        return "Failed to connect to WiFi";
    }

    // Print ESP Local IP Address
    Serial.println(WiFi.localIP());

    initWebSocket();

    // Route for root / web page
    // server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send_P(200, "text/html", index_html, processor); });

    // Start server
    server.begin();
    return WiFi.localIP().toString();
}

// Send WebSocket message to all connected clients
void sendWsMessage(String message)
{
#ifdef ARDUINO_ARCH_ESP32
    if (WiFi.status() == WL_CONNECTED)
    {
        ws.textAll(message);
    }
#endif
}

// WebSocket cleanup and maintainance
void wsCleanup()
{
#ifdef ARDUINO_ARCH_ESP32
    if (WiFi.status() == WL_CONNECTED)
    {
        ws.cleanupClients();
    }
#endif
}
