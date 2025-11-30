#include "lib_server.hpp"

// application is expected to provide WiFiCredentials.h with WIFI_SSID /
// WIFI_PASSWORD
#include "WiFiCredentials.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#ifdef ARDUINO_ARCH_ESP32
#include <ESPmDNS.h>
#endif

// internal server instance
static WebServer server(80);

// pointers to external state (supplied by main app)
static volatile bool* g_ledPtr      = nullptr;
static volatile bool* g_sStatePtr   = nullptr;
static uint8_t        g_btnCount    = 0;
static unsigned long  g_startMillis = 0;  // copy instead of pointer

static const char* kMdnsNameDefault = "rigkontrol";

static String buildStatusJson() {
  unsigned long uptime = 0;
  uptime               = millis() - g_startMillis;

  String json = "{";
  json += "\"uptime_ms\":" + String(uptime) + ",";
  json += "\"wifi_mode\":\"" +
          String(WiFi.getMode() == WIFI_MODE_AP ? "AP" : "STA") + "\",";

  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"led\":" + String((g_ledPtr && *g_ledPtr) ? "true" : "false");

  if (g_sStatePtr && g_btnCount > 0) {
    for (uint8_t i = 0; i < g_btnCount; ++i) {
      bool v = g_sStatePtr[i];
      json += ",\"s" + String(i + 1) + "\":" + String(v ? "true" : "false");
    }
  }

  json += "}";
  return json;
}

/* HTTP handlers (use static functions so we can register with server) */
static void handleRoot() {
  const char* html =
      "<!doctype html><html><head><meta charset='utf-8'><meta name='viewport' "
      "content='width=device-width,initial-scale=1'>"
      "<title>Dave Sample Kontrol</title>"
      "<style>body{font-family:Arial,Helvetica,sans-serif;margin:1rem}button{"
      "padding:.5rem 1rem;font-size:1rem}</style>"
      "</head><body>"
      "<h1>Dave Sample Kontrol</h1>"
      "<div id='status'>Loading...</div>"
      "<p><button id='toggle'>Toggle LED</button></p>"
      "<script>"
      "async function fetchStatus(){"
      "  try{const r=await fetch('/api/status'); const j=await r.json();"
      "   document.getElementById('status').innerText = 'IP: '+j.ip+' | Mode: "
      "'+j.wifi_mode+' | RSSI: '+j.rssi+' | Uptime ms: '+j.uptime_ms+' | LED: "
      "'+j.led+' | S1: '+j.s1+' | S2: '+j.s2+' | S3: '+j.s3+' | S4: '+j.s4; "
      "  }catch(e){document.getElementById('status').innerText='Error fetching "
      "status';}}"
      "document.getElementById('toggle').addEventListener('click', async ()=>{"
      "  try{const r=await fetch('/api/toggle', {method:'POST'}); await "
      "fetchStatus();}catch(e){alert('Error');}});"
      "fetchStatus(); setInterval(fetchStatus,2000);"
      "</script></body></html>";
  server.send(200, "text/html", html);
}

static void handleStatus() {
  server.send(200, "application/json", buildStatusJson());
}

static void handleToggle() {
  if (g_ledPtr) {
    *g_ledPtr = !(*g_ledPtr);
    // Let the main application actually write to the physical pin, the library
    // does not manipulate pins directly. But for convenience, if user provided
    // a valid pointer and the LED pin is managed externally, it will reflect
    // here.
  }
  server.send(200, "application/json", buildStatusJson());
}

static void handleNotFound() {
  String message = "Not found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";
  server.send(404, "text/plain", message);
}

/* WiFi helpers (moved from main) */
static void startAPMode() {
  String apName =
      "DaveSK-" + String((uint32_t)ESP.getEfuseMac(), HEX).substring(10);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(apName.c_str());
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("Started AP '%s' IP=%s\n", apName.c_str(),
                apIP.toString().c_str());
}

void serverConnectWiFi(void) {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("No SSID configured, starting AP mode");
    startAPMode();
    return;
  }

  Serial.printf("Connecting to WiFi SSID='%s'\n", WIFI_SSID);
  WiFi.mode(WIFI_MODE_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long       start   = millis();
  const unsigned long timeout = 10000;  // 10s
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
    Serial.print(".");
    delay(250);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP=");
    Serial.println(WiFi.localIP());
#ifdef ARDUINO_ARCH_ESP32
    if (MDNS.begin(kMdnsNameDefault)) {
      Serial.println("mDNS responder started: rigkontrol.local");
    }
#endif
  } else {
    Serial.println("Failed to connect, starting AP mode");
    startAPMode();
  }
}

void serverStart(void) {
  // register handlers
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/toggle", HTTP_POST, handleToggle);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started on port 80");
  Serial.println(
      "Open / in a browser (or rigkontrol.local if mDNS is working)");
}

/* Public API */

void serverInit(volatile bool* ledPtr, volatile bool* sStatePtr,
                uint8_t btnCount, unsigned long startMillis) {
  g_ledPtr      = ledPtr;
  g_sStatePtr   = sStatePtr;
  g_btnCount    = btnCount;
  g_startMillis = startMillis;

  serverConnectWiFi();
  serverStart();
}

void serverHandleClient(void) {
  server.handleClient();
}
