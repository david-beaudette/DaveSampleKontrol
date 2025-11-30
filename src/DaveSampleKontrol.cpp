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
#include "lib_mp3.hpp"
#include "lib_server.hpp"
#include <Arduino.h>

// MP3 players
static MP3Player mp3Reader1(1, RX1, TX1);
static MP3Player mp3Reader2(2, 8, 5);

// Built-in LED (not visible outside pedalboard case)
static const int LED_PIN = 17;

// 4 pedalboard buttons
static const uint8_t BUTTON_COUNT              = 4;
static const uint8_t BUTTON_PINS[BUTTON_COUNT] = {
    46,  // S1 top-left
    45,  // S2 top-right
    21,  // S3 bottom-left
    9    // S4 bottom-right
};

unsigned long startMillis = 0;

// Buttons and LED state trackers
volatile bool ledState             = false;
volatile bool sState[BUTTON_COUNT] = {false, false, false, false};

// Manage button-driven actions for MP3 players.
// - Updates button state (debounce + events)
// - S1 (idx 0): player1 LEFT -> toggle play/pause
// - S2 (idx 1): player1 RIGHT -> stop
// - S3 (idx 2): player2 LEFT -> toggle play/pause
// - S4 (idx 3): player2 RIGHT -> stop
static void manageButtonActions() {
  // Update debounced states and generate events
  updateButtons(sState);

  // Player 1: S1 (0) = left (toggle), S2 (1) = right (stop)
  if (checkIfButtonWasPressed(0)) {
    mp3Reader1.togglePlayPause();
  }
  if (checkIfButtonWasPressed(1)) {
    mp3Reader1.stopPlayback();
  }

  // Player 2: S3 (2) = left (toggle), S4 (3) = right (stop)
  if (checkIfButtonWasPressed(2)) {
    mp3Reader2.togglePlayPause();
  }
  if (checkIfButtonWasPressed(3)) {
    mp3Reader2.stopPlayback();
  }

  // Note: checkIfButtonWasPressed() clears the "pressed" event for that
  // button automatically when it returns true, satisfying the "clear
  // the button press events after they have been managed" requirement.
}

void setup() {
  Serial.begin(115200);
  // MP3 instance already constructed; give it a moment to settle
  delay(100);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  initButtons(BUTTON_PINS, BUTTON_COUNT, true);

  startMillis = millis();

  // Initialize WiFi + HTTP server
  serverInit(&ledState, sState, BUTTON_COUNT, startMillis);

  Serial.println();
  Serial.println(F("Dave Sample Kontrol Starting..."));
  Serial.println(F("Initializing mp3 players ... (May take 3~5 seconds)"));

  if (!mp3Reader1.begin()) {
    Serial.println(F("Unable to connect to mp3 player 1:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
  } else {
    Serial.println(F("mp3 player 1 is online."));
    mp3Reader1.setVolume(10);  // Set volume value. From 0 to 30
    mp3Reader1.play(1);        // Play the first mp3
  }

  if (!mp3Reader2.begin()) {
    Serial.println(F("Unable to connect to mp3 player 2:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
  } else {
    Serial.println(F("mp3 player 2 is online."));
    mp3Reader2.setVolume(10);  // Set volume value. From 0 to 30
    mp3Reader2.play(1);        // Play the first mp3
  }
}

void loop() {
  serverHandleClient();
  unsigned long now = millis();

  // Process button changes and take action
  manageButtonActions();

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
