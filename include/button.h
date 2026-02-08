/*
 * button module
 * polling-based
 */

#ifndef BUTTON_H
#define BUTTON_H

#include <stdint.h>
#include "config.h"

// <---- button identification ---->

typedef enum {
  BUTTON_PLAYER1_ENCODER_PUSH,
  BUTTON_PLAYER1_MENU,
  BUTTON_PLAYER1_BACK,
  BUTTON_PLAYER1_TAP,
  BUTTON_PLAYER2_ENCODER_PUSH,
  BUTTON_PLAYER2_MENU,
  BUTTON_PLAYER2_BACK,
  BUTTON_PLAYER2_TAP,
  BUTTON_COUNT
} button_id_t;

// <---- initialization and update ---->

/**
 * initialize button module
 * resets all button states and debounce counters
 * call once at startup after hardware_init()
 */
void button_init(void);

/**
 * update button states
 * reads all button pins, performs debouncing, detects edges
 * call this every main loop iteration
 */
void button_update(void);

// <---- button state functions ---->

/**
 * check if button was just pressed
 * returns true once per button press (edge detection)
 * automatically clears the flag after reading
 *
 * @param button button to check
 * @return TRUE if button was pressed since last check, FALSE otherwise
 */
uint8_t button_was_pressed(button_id_t button);

/**
 * check if button was just released
 * returns true once per button release (edge detection)
 * automatically clears the flag after reading
 *
 * @param button button to check
 * @return TRUE if button was released since last check, FALSE otherwise
 */
uint8_t button_was_released(button_id_t button);

/**
 * check if button has been held down for specified duration
 * continuously returns TRUE while button is held beyond hold_time
 *
 * @param button button to check
 * @param hold_time_ms minimum hold duration in milliseconds
 * @return TRUE if button held for at least hold_time_ms, FALSE otherwise
 */
uint8_t button_is_held(button_id_t button, uint32_t hold_time_ms);

#endif  // BUTTON_H