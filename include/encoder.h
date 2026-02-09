/*
 * rotary encoder module
 * reads quadrature encoder inputs and detects rotation
 */

#ifndef ENCODER_H
#define ENCODER_H

#include <stdint.h>
#include "config.h"

// <---- encoder identification ---->

typedef enum {
    ENCODER_PLAYER1,
    ENCODER_PLAYER2,
    ENCODER_COUNT
} encoder_id_t;

// <---- initialization and update ---->

/**
 * initialize encoder module
 * resets all encoder states
 * call once at startup after hardware_init()
 */
void encoder_init(void);


// <---- encoder reading ---->

/**
 * get encoder movement in physical clicks since last call
 * converts raw quadrature transitions (4 per click) to clicks
 *
 * @param encoder encoder to read
 * @return +1 for clockwise click, -1 for counter-clockwise click, 0 for no change
 */
int8_t encoder_get_clicks(encoder_id_t encoder);

#endif // ENCODER_H