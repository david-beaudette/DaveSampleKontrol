// src/dave_sample_kontrol.cpp
//
// Main application for RigKontrol1 - embedded web server on ESP32-S3
// - Connects to WiFi (fallback to AP mode)
// - Exposes a simple HTTP API and a small web UI to view/toggle an LED
//
// Create a header file named WiFiCredentials in the include folder that
// contains . static const char* WIFI_SSID     = "YOUR_SSID"; static const char*
// WIFI_PASSWORD = "YOUR_PASSWORD";

#include "WiFiCredentials.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

static const int LED_PIN = 17;  // Built-in LED

// Replace individual button defines with arrays
static const int BUTTON_COUNT              = 4;
static const int BUTTON_PINS[BUTTON_COUNT] = {
    46,  // S1 top-left
    45,  // S2 top-right
    21,  // S3 bottom-left
    9    // S4 bottom-right
};

WebServer server(80);

unsigned long startMillis = 0;

// make the switch states volatile so ISR/loop visibility is correct
volatile bool ledState = false;
// sState == true means "pressed" (hardware uses INPUT_PULLUP: pressed -> LOW)
volatile bool sState[BUTTON_COUNT] = {false, false, false, false};

const unsigned long DEBOUNCE_DELAY_MS = 50;

// robust ISR-side info: last ISR tick + last observed level
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

volatile TickType_t buttonLastISRTick[BUTTON_COUNT] = {0};
volatile uint8_t    buttonLastLevel[BUTTON_COUNT]   = {HIGH, HIGH, HIGH, HIGH};
volatile bool       buttonFlag[BUTTON_COUNT] = {false, false, false, false};

// Common ISR handler called by tiny wrappers (keep ISRs short)
static inline void IRAM_ATTR buttonISR_common(int idx) {
  // capture tick and last observed level in an ISR-safe way
  buttonLastISRTick[idx] = xTaskGetTickCountFromISR();
  // digitalRead is small and safe here for GPIO level
  buttonLastLevel[idx] = digitalRead(BUTTON_PINS[idx]);
  buttonFlag[idx]      = true;
}

// Small ISR wrappers to allow passing an index (avoids code duplication later)
static void IRAM_ATTR buttonISR_0() {
  buttonISR_common(0);
}
static void IRAM_ATTR buttonISR_1() {
  buttonISR_common(1);
}
static void IRAM_ATTR buttonISR_2() {
  buttonISR_common(2);
}
static void IRAM_ATTR buttonISR_3() {
  buttonISR_common(3);
}

String getStatusJson() {
  String json = "{";
  json += "\"uptime_ms\":" + String(millis() - startMillis) + ",";
  json += "\"wifi_mode\":\"" +
          String(WiFi.getMode() == WIFI_MODE_AP ? "AP" : "STA") + "\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI()) + ",";
  json += "\"led\":" + String(ledState ? "true" : "false");
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    json +=
        ",\"s" + String(i + 1) + "\":" + String(sState[i] ? "true" : "false");
  }
  json += "}";
  return json;
}

void handleRoot() {
  // Simple single-file web UI (small HTML + JS)
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
      "'+j.led+' | S1: '+j.s1+' | S2: '+j.s2+' | S3: '+j.s3+' | S4: '+j.s4;"
      "  }catch(e){document.getElementById('status').innerText='Error fetching "
      "status';}}"
      "document.getElementById('toggle').addEventListener('click', async ()=>{"
      "  try{const r=await fetch('/api/toggle', {method:'POST'}); await "
      "fetchStatus();}catch(e){alert('Error');}});"
      "fetchStatus(); setInterval(fetchStatus,2000);"
      "</script></body></html>";
  server.send(200, "text/html", html);
}

void handleStatus() {
  server.send(200, "application/json", getStatusJson());
}

void handleToggle() {
  ledState = !ledState;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  server.send(200, "application/json", getStatusJson());
}

void handleNotFound() {
  String message = "Not found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\n";
  server.send(404, "text/plain", message);
}

void startAPMode() {
  String apName =
      "DaveSK-" + String((uint32_t)ESP.getEfuseMac(), HEX).substring(10);
  WiFi.mode(WIFI_MODE_AP);
  WiFi.softAP(apName.c_str());
  IPAddress apIP = WiFi.softAPIP();
  Serial.printf("Started AP '%s' IP=%s\n", apName.c_str(),
                apIP.toString().c_str());
}

void connectWiFi() {
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
    if (MDNS.begin("rigkontrol")) {
      Serial.println("mDNS responder started: rigkontrol.local");
    }
  } else {
    Serial.println("Failed to connect, starting AP mode");
    startAPMode();
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  for (int i = 0; i < BUTTON_COUNT; ++i) {
    pinMode(BUTTON_PINS[i], INPUT_PULLUP);
  }

  // table of ISR wrappers so we can attach in a loop
  using ISRFunc                                 = void (*)();
  static const ISRFunc buttonISRs[BUTTON_COUNT] = {buttonISR_0, buttonISR_1,
                                                   buttonISR_2, buttonISR_3};
  for (int i = 0; i < BUTTON_COUNT; ++i) {
    attachInterrupt(digitalPinToInterrupt(BUTTON_PINS[i]), buttonISRs[i],
                    CHANGE);
  }

  startMillis = millis();

  connectWiFi();

  // HTTP routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/toggle", HTTP_POST, handleToggle);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started on port 80");
  Serial.println(
      "Open / in a browser (or rigkontrol.local if mDNS is working)");
}

// Helper run in loop() to process pending ISR flags and perform debouncing/read
static void processButtonFlags(unsigned long now) {
  // convert current time to ticks once
  TickType_t       nowTicks      = xTaskGetTickCount();
  const TickType_t debounceTicks = pdMS_TO_TICKS(DEBOUNCE_DELAY_MS);

  for (int i = 0; i < BUTTON_COUNT; ++i) {
    if (!buttonFlag[i]) continue;
    // only accept as stable if enough time elapsed since last ISR for this pin
    if ((nowTicks - buttonLastISRTick[i]) >= debounceTicks) {
      // commit last observed level as stable state
      // INPUT_PULLUP: LOW == pressed -> sState should be true when pressed
      sState[i] = (buttonLastLevel[i] == LOW) ? true : false;
      // clear activity indicators
      buttonFlag[i]        = false;
      buttonLastISRTick[i] = 0;
    }
    // else: keep waiting until debounce window after the most recent ISR has
    // passed
  }
}

void loop() {
  server.handleClient();
  unsigned long now = millis();

  // Process button changes coming from ISRs (debounce & update stable state)
  processButtonFlags(now);

  // optional: update mDNS (ESPmDNS handles itself mostly)
  // small blink to indicate running: toggle every second
  static unsigned long lastBlink = 0;
  if (now - lastBlink >= 1000) {
    lastBlink = now;
    Serial.printf("Switch values S1 %s, S2 %s, S3 %s, S4 %s\n",
                  sState[0] ? "ON" : "OFF", sState[1] ? "ON" : "OFF",
                  sState[2] ? "ON" : "OFF", sState[3] ? "ON" : "OFF");
  }
}
