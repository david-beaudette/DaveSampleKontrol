// src/dave_sample_kontrol.cpp
//
// Main application for RigKontrol1 - embedded web server on ESP32-S3
// - Connects to WiFi (fallback to AP mode)
// - Exposes a simple HTTP API and a small web UI to view/toggle an LED
//
// Create a header file named WiFiCredentials in the include folder that
// contains . static const char* WIFI_SSID     = "YOUR_SSID"; static const char*
// WIFI_PASSWORD = "YOUR_PASSWORD";

#include "lib_button.hpp"
#include "DFRobotDFPlayerMini.h"

#include "lib_server.hpp"
#include <Arduino.h>

// DF player connected to Serial1
#define FPSerial Serial1
DFRobotDFPlayerMini mp3player;

static const int LED_PIN = 17;  // Built-in LED

// Replace individual button defines with arrays
static const uint8_t BUTTON_COUNT              = 4;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
    46,  // S1 top-left
    45,  // S2 top-right
    21,  // S3 bottom-left
    9    // S4 bottom-right
};

unsigned long startMillis = 0;

// make the switch states volatile so ISR/loop visibility is correct
volatile bool ledState = false;
// sState == true means "pressed" (hardware uses INPUT_PULLUP: pressed -> LOW)
volatile bool sState[BUTTON_COUNT] = {false, false, false, false};

void setup() {
  Serial.begin(115200);
  FPSerial.begin(9600, SERIAL_8N1, RX1, TX1);
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  initButtons(BUTTON_PINS, BUTTON_COUNT, true);

  startMillis = millis();

  // initialize WiFi + HTTP server (library moved to lib_server)
  serverInit(&ledState, sState, BUTTON_COUNT, &startMillis);

  Serial.println();
  Serial.println(F("DFRobot DFPlayer Mini Demo"));
  Serial.println(F("Initializing DFPlayer ... (May take 3~5 seconds)"));

  if (!mp3player.begin(
          FPSerial, /*isACK = */ true,
          /*doReset = */ true)) {  // Use serial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));

  } else {
    Serial.println(F("DFPlayer Mini online."));

    mp3player.volume(10);  // Set volume value. From 0 to 30
    mp3player.play(1);     // Play the first mp3
  }
}

void loop() {
  serverHandleClient();
  unsigned long now = millis();

  // Process button changes coming from ISRs (debounce & update stable state)
  updateButtons(sState);

  // optional: update mDNS (ESPmDNS handles itself mostly)
  // small blink to indicate running: toggle every second
  static unsigned long lastBlink = 0;
  if (now - lastBlink >= 1000) {
    lastBlink = now;
    Serial.printf("Switch values S1 %s, S2 %s, S3 %s, S4 %s\n",
                  sState[0] ? "ON" : "OFF", sState[1] ? "ON" : "OFF",
                  sState[2] ? "ON" : "OFF", sState[3] ? "ON" : "OFF");
  }

  // Ensure physical LED reflects library-updated ledState (server may toggle
  // it)
  static bool prevLedState = false;
  if (ledState != prevLedState) {
    prevLedState = ledState;
    digitalWrite(LED_PIN, ledState ? HIGH : LOW);
  }
}
