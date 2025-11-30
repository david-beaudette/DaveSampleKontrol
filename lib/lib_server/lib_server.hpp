#ifndef LIB_SERVER_HPP
#define LIB_SERVER_HPP

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>

#ifdef ARDUINO_ARCH_ESP32
#include <ESPmDNS.h>
#endif

/* Initialize server + WiFi and register HTTP routes.
 * ledPtr: pointer to external volatile bool representing LED state.
 * sStatePtr: pointer to external volatile bool array of button states.
 * btnCount: number of buttons in sStatePtr.
 * startMillisPtr: pointer to the same startMillis used by application (used to
 * compute uptime).
 *
 * This function will attempt to connect to WiFi using WiFiCredentials.h
 * (same as was in DaveSampleKontrol.cpp). It will fall back to AP mode.
 */
void serverInit(volatile bool* ledPtr, volatile bool* sStatePtr,
                uint8_t btnCount, unsigned long* startMillisPtr);

/* Call frequently from loop() to let the HTTP server process clients */
void serverHandleClient(void);

/* (Optional) Reconfigure/refresh pointers to external runtime data.
 * Useful if the application wants to change where state is stored later.
 */
void serverSetExternalStatePointers(volatile bool* ledPtr,
                                    volatile bool* sStatePtr, uint8_t btnCount,
                                    unsigned long* startMillisPtr);

/* Expose low-level connect/start functions if needed externally */
void serverConnectWiFi(void);
void serverStart(void);

#endif  // LIB_SERVER_HPP