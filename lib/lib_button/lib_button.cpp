/*
 * lib_button.cpp
 *
 * Button handling for DaveSampleKontrol-derived project.
 *
 * Adds optional AVR pin-change interrupt support to the existing
 * debounced button reads, edge detection, long-press and double-click
 * detection logic.
 *
 * When compiled for AVR the library will register PCINTs for pins that
 * support them and set a lightweight ISR flag when a pin-change occurs.
 * updateButtons() will then read pin states and run the usual logic.
 *
 * For non-AVR platforms or pins without PCINT support the library falls
 * back to polling (reads every updateButtons()).
 */

#include "lib_button.hpp"

#include <Arduino.h>

#if defined(__AVR__)
#include <avr/interrupt.h>
#endif

static const uint8_t MAX_BUTTONS = 16;

// Default timings (ms)
static uint16_t debounceDelay   = 50;
static uint16_t longPressTime   = 1000;
static uint16_t doubleClickTime = 400;

static uint8_t btnCount = 0;
static uint8_t btnPins[MAX_BUTTONS];

// State arrays
static bool          lastRawState[MAX_BUTTONS];
static bool          stableState[MAX_BUTTONS];
static unsigned long lastDebounceTime[MAX_BUTTONS];

// Event flags (set in update(), cleared when read)
static bool evtPressed[MAX_BUTTONS];
static bool evtReleased[MAX_BUTTONS];
static bool evtLongPress[MAX_BUTTONS];
static bool evtDoubleClick[MAX_BUTTONS];

static unsigned long lastChangeTime[MAX_BUTTONS];
static unsigned long lastPressMillis[MAX_BUTTONS];
static uint8_t       clickCount[MAX_BUTTONS];
static bool          longReported[MAX_BUTTONS];

// Interrupt support bookkeeping (AVR)
#if defined(__AVR__)
static volatile bool anyPinChange = false;  // set by ISR(s)

static volatile uint8_t* pinInputReg[MAX_BUTTONS];
static uint8_t           pinBitMask[MAX_BUTTONS];
static bool              hasPCINT[MAX_BUTTONS];
#else
// Non-AVR: always poll
#endif

/*
 * Initialize buttons.
 * pins: array of input pin numbers
 * count: number of pins (<= MAX_BUTTONS)
 * internal pullup is enabled by default (active-low buttons).
 */
void initButtons(const uint8_t* pins, const uint8_t count, bool usePullup) {
  if (count > MAX_BUTTONS) {
    btnCount = MAX_BUTTONS;
  } else {
    btnCount = count;
  }

#if defined(__AVR__)
  // Clear PCMSK/PCICR bits only for our usage; we'll OR bits in for needed
  // groups. We'll accumulate which PCINT groups we need.
  bool needPCINT0 = false, needPCINT1 = false, needPCINT2 = false;
#endif

  unsigned long now = millis();

  for (uint8_t i = 0; i < btnCount; ++i) {
    btnPins[i] = pins[i];
    if (usePullup)
      pinMode(btnPins[i], INPUT_PULLUP);
    else
      pinMode(btnPins[i], INPUT);

#if defined(__AVR__)
    // Try to set up PCINT for this pin if available
    // digitalPinToPCMSK(pin) and digitalPinToPCMSKbit(pin) macros are provided
    // by Arduino AVR core Some pins may not have PCINT support; handle
    // gracefully. Prepare input register pointer and bitmask for fast direct
    // reads.
    pinInputReg[i] = portInputRegister(digitalPinToPort(btnPins[i]));
    pinBitMask[i]  = digitalPinToBitMask(btnPins[i]);

    // Check if the macros for PCMSK exist and return non-null
#ifdef digitalPinToPCMSK
    volatile uint8_t* pcmsk    = digitalPinToPCMSK(btnPins[i]);
    uint8_t           pcmskbit = digitalPinToPCMSKbit(btnPins[i]);
    if (pcmsk != nullptr) {
      // Mark the corresponding PCMSK bit; we will enable PCICR bits below in
      // one shot.
      *pcmsk |= _BV(pcmskbit);
      hasPCINT[i] = true;

      // Determine which PCINT group this pin belongs to so we can enable PCICR
      uint8_t pcicrbit = digitalPinToPCICRbit(btnPins[i]);
      if (pcicrbit == 0)
        needPCINT0 = true;
      else if (pcicrbit == 1)
        needPCINT1 = true;
      else if (pcicrbit == 2)
        needPCINT2 = true;
    } else {
      hasPCINT[i] = false;
    }
#else
    hasPCINT[i] = false;
#endif

#else
    // Non-AVR: nothing special
#endif

    bool raw = digitalRead(btnPins[i]) == LOW;  // active-low -> pressed = true
    lastRawState[i]     = raw;
    stableState[i]      = raw;
    lastDebounceTime[i] = now;
    lastChangeTime[i]   = now;
    lastPressMillis[i]  = 0;
    clickCount[i]       = 0;
    evtPressed[i] = evtReleased[i] = evtLongPress[i] = evtDoubleClick[i] =
        false;
    longReported[i] = false;
  }

#if defined(__AVR__)
  // Enable PCICR bits for needed groups (do this AFTER PCMSK bits were set
  // per-pin)
  if (needPCINT0) PCICR |= _BV(PCIE0);
  if (needPCINT1) PCICR |= _BV(PCIE1);
  if (needPCINT2) PCICR |= _BV(PCIE2);

  // if any PCINT was enabled, clear the ISR flag initially
  anyPinChange = false;
#endif
}

/*
 * Configure timing parameters (milliseconds).
 */
void setButtonDebounceTimeMs(uint16_t ms) {
  debounceDelay = ms;
}
void setButtonLongPressTimeMs(uint16_t ms) {
  longPressTime = ms;
}
void setButtonDoubleClickTimeMs(uint16_t ms) {
  doubleClickTime = ms;
}

/*
 * Must be called frequently (e.g. inside loop()) to update button state and
 * generate events.
 *
 * With AVR+PCINTs: this only re-reads pins when ISR flagged anyPinChange
 * (lightweight). Pins without PCINT support are polled every call.
 */
void updateButtons(volatile bool* sState) {
  unsigned long now = millis();

#if defined(__AVR__)
  bool doRead = false;
  // If any pin-change happened we should re-read all pins (both PCINT and
  // non-PCINT)
  if (anyPinChange) {
    doRead = true;
    // clear the flag as we'll handle reads now
    anyPinChange = false;
  }
#else
  // Non-AVR: always read
  bool doRead = true;
#endif

  for (uint8_t i = 0; i < btnCount; ++i) {
    bool raw;
#if defined(__AVR__)
    if (hasPCINT[i]) {
      // Only read when ISR has signaled something; otherwise skip to save
      // cycles
      if (!doRead) continue;
      // Direct fast read using PINx register pointer and bitmask
      raw = ((*pinInputReg[i] & pinBitMask[i]) ==
             0);  // active-low -> pressed true
    } else {
      // No PCINT for this pin: poll always
      raw = digitalRead(btnPins[i]) == LOW;
    }
#else
    raw = digitalRead(btnPins[i]) == LOW;
#endif

    // If raw changed, start debounce timer
    if (raw != lastRawState[i]) {
      lastDebounceTime[i] = now;
      lastRawState[i]     = raw;
    }

    // If the raw state has been stable for debounceDelay, consider it the
    // stable state.
    if ((now - lastDebounceTime[i]) >= debounceDelay) {
      if (stableState[i] != raw) {
        // State changed (debounced)
        stableState[i]    = raw;
        lastChangeTime[i] = now;
        longReported[i]   = false;

        if (stableState[i]) {
          // Pressed
          evtPressed[i] = true;

          // Double-click handling: count presses
          if ((now - lastPressMillis[i]) <= doubleClickTime) {
            ++clickCount[i];
          } else {
            clickCount[i] = 1;
          }
          lastPressMillis[i] = now;

          if (clickCount[i] == 2) {
            evtDoubleClick[i]  = true;
            clickCount[i]      = 0;
            lastPressMillis[i] = 0;
          }
        } else {
          // Released
          evtReleased[i] = true;
          // release handling left as before
        }
      }
    }

    // Long press detection: if button has been held long enough and not yet
    // reported
    if (stableState[i] && !longReported[i]) {
      if ((now - lastChangeTime[i]) >= longPressTime) {
        evtLongPress[i] = true;
        longReported[i] = true;
        // on long-press, clear click counting to avoid accidental double-clicks
        clickCount[i]      = 0;
        lastPressMillis[i] = 0;
      }
    }

    // Timeout double-click window: if waiting for second click and time
    // exceeded, reset counter
    if (clickCount[i] == 1 && ((now - lastPressMillis[i]) > doubleClickTime)) {
      clickCount[i]      = 0;
      lastPressMillis[i] = 0;
    }
    sState[i] = checkIfButtonDown(i);
  }
}

/*
 * Query helpers. The "was" functions return true once (they clear the event).
 * isDown returns current debounced state without clearing events.
 */

bool checkIfButtonDown(uint8_t idx) {
  if (idx >= btnCount) return false;
  return stableState[idx];
}

bool checkIfButtonWasPressed(uint8_t idx) {
  if (idx >= btnCount) return false;
  bool v          = evtPressed[idx];
  evtPressed[idx] = false;
  return v;
}

bool checkIfButtonWasReleased(uint8_t idx) {
  if (idx >= btnCount) return false;
  bool v           = evtReleased[idx];
  evtReleased[idx] = false;
  return v;
}

bool checkIfButtonWasLongPressed(uint8_t idx) {
  if (idx >= btnCount) return false;
  bool v            = evtLongPress[idx];
  evtLongPress[idx] = false;
  return v;
}

bool checkIfButtonWasDoubleClicked(uint8_t idx) {
  if (idx >= btnCount) return false;
  bool v              = evtDoubleClick[idx];
  evtDoubleClick[idx] = false;
  return v;
}

/*
 * Convenience: clear all pending events.
 */
void clearAllButtonEvents() {
  for (uint8_t i = 0; i < btnCount; ++i) {
    evtPressed[i] = evtReleased[i] = evtLongPress[i] = evtDoubleClick[i] =
        false;
  }
}

/*
 * Optional: return number of configured buttons.
 */
uint8_t countButtons() {
  return btnCount;
}

#if defined(__AVR__)
/*
 * Lightweight PCINT ISRs: only mark that a pin-change occurred.
 * We do not call any non-ISR-safe functions here (no millis(), no
 * digitalRead()). Define ISRs only for the vectors available on the platform.
 */
#if defined(PCINT0_vect)
ISR(PCINT0_vect) {
  anyPinChange = true;
}
#endif

#if defined(PCINT1_vect)
ISR(PCINT1_vect) {
  anyPinChange = true;
}
#endif

#if defined(PCINT2_vect)
ISR(PCINT2_vect) {
  anyPinChange = true;
}
#endif
#endif