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
void Button_init(const uint8_t* pins, const uint8_t count, bool usePullup);

/* Configure timing (milliseconds) */
void Button_setDebounce(uint16_t ms);
void Button_setLongPress(uint16_t ms);
void Button_setDoubleClick(uint16_t ms);

/* Must be called regularly (e.g. inside loop()) to update state and generate
 * events. */
void Button_update(void);

/* Query functions */
/* Return debounced current state (true = pressed). Does not clear events. */
bool Button_isDown(uint8_t idx);

/* "Was" helpers return true once and clear the corresponding event flag. */
bool Button_wasPressed(uint8_t idx);
bool Button_wasReleased(uint8_t idx);
bool Button_wasLongPressed(uint8_t idx);
bool Button_wasDoubleClicked(uint8_t idx);

/* Clear all pending events. */
void Button_clearAllEvents(void);

/* Return number of configured buttons. */
uint8_t Button_count(void);

#endif  // LIB_BUTTON_HPP