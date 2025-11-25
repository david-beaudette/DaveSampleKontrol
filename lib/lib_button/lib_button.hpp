#ifndef LIB_BUTTON_HPP
#define LIB_BUTTON_HPP

#include <Arduino.h>
#include <stdint.h>
#include <stdbool.h>

/*
 * lib_button - header
 *
 * Debounced button handling with optional AVR pin-change interrupt support.
 * See lib_button.cpp for implementation details.
 */

/* Initialize buttons.
 * pins: pointer to array of Arduino digital pin numbers.
 * count: number of pins (max handled by implementation).
 * usePullup: if true, INPUT_PULLUP is used (buttons active-low).
 */
void initButtons(const uint8_t* pins, const uint8_t count, bool usePullup);

/* Configure timing (milliseconds) */
void setButtonDebounceTimeMs(uint16_t ms);
void setButtonLongPressTimeMs(uint16_t ms);
void setButtonDoubleClickTimeMs(uint16_t ms);

/* Must be called regularly (e.g. inside loop()) to update state and generate
 * events. */
void updateButtons(volatile bool* sState);

/* Query functions */
/* Return debounced current state (true = pressed). Does not clear events. */
bool checkIfButtonDown(uint8_t idx);

/* "Was" helpers return true once and clear the corresponding event flag. */
bool checkIfButtonWasPressed(uint8_t idx);
bool checkIfButtonWasReleased(uint8_t idx);
bool checkIfButtonWasLongPressed(uint8_t idx);
bool checkIfButtonWasDoubleClicked(uint8_t idx);

/* Clear all pending events. */
void clearAllButtonEvents(void);

/* Return number of configured buttons. */
uint8_t countButtons(void);

#endif  // LIB_BUTTON_HPP