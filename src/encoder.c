/*
 * rotary encoder module implementation
 * uses quadrature decoding to detect rotation direction
 */

#include "encoder.h"
#include "stm32f1xx_hal.h"

// <---- encoder state tracking ---->

typedef struct {
    GPIO_TypeDef *port;      // gpio port for encoder pins
    uint16_t pin_a;          // encoder channel a pin
    uint16_t pin_b;          // encoder channel b pin
    uint8_t last_state;      // previous state of channels (2 bits: BA)
    volatile int8_t delta;            // accumulated rotation count since last read (volatile: written by isr, read by main)
    int8_t direction_multiplier; // +1 or -1 for direction inversion
} encoder_state_t;

// array of all encoder states
static encoder_state_t encoders[ENCODER_COUNT];

// <---- encoder configuration table ---->

// maps encoder id to gpio port and pins
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin_a;
    uint16_t pin_b;
    int8_t direction_multiplier;  // +1 or -1 to invert direction if needed
} encoder_config_t;

static const encoder_config_t encoder_configs[ENCODER_COUNT] = {
    {PLAYER1_ENCODER_PORT, PLAYER1_ENCODER_A_PIN, PLAYER1_ENCODER_B_PIN, -1},  // ENCODER_PLAYER1 (inverted)
    {PLAYER2_ENCODER_PORT, PLAYER2_ENCODER_A_PIN, PLAYER2_ENCODER_B_PIN, -1}   // ENCODER_PLAYER2 (inverted)
};

// <---- quadrature decoding lookup table ---->

// quadrature state transition table
// rows: previous state (0-3), columns: current state (0-3)
// values: -1 (ccw), 0 (no change/invalid), +1 (cw)
// state encoding: bit1=B, bit0=A (00, 01, 10, 11)
static const int8_t quadrature_table[4][4] = {
    //  00   01   10   11    <- current state
    {   0,  -1,  +1,   0 },  // 00 <- previous state
    {  +1,   0,   0,  -1 },  // 01
    {  -1,   0,   0,  +1 },  // 10
    {   0,  +1,  -1,   0 }   // 11
};

//click accumulator (4 transitions = 1 physical click)
static int8_t click_fractional[ENCODER_COUNT] = {0, 0};

// <---- initialization ---->

void encoder_init(void) {
    // initialize all encoder states
    for (uint8_t i = 0; i < ENCODER_COUNT; i++) {
        encoders[i].port = encoder_configs[i].port;
        encoders[i].pin_a = encoder_configs[i].pin_a;
        encoders[i].pin_b = encoder_configs[i].pin_b;
        encoders[i].direction_multiplier = encoder_configs[i].direction_multiplier;
        encoders[i].delta = 0;
        click_fractional[i] = 0;
        
        // read initial state of encoder pins
        uint8_t a = (HAL_GPIO_ReadPin(encoders[i].port, encoders[i].pin_a) == GPIO_PIN_SET) ? 1 : 0;
        uint8_t b = (HAL_GPIO_ReadPin(encoders[i].port, encoders[i].pin_b) == GPIO_PIN_SET) ? 1 : 0;
        encoders[i].last_state = (b << 1) | a;  // combine into 2-bit state
    }
}

// <---- internal helper: process encoder state change ---->

// called from isr context to decode quadrature transition for one encoder
static void encoder_process_isr(encoder_id_t encoder_id) {
    encoder_state_t *enc = &encoders[encoder_id];

    // read current state of both encoder pins
    uint8_t a = (HAL_GPIO_ReadPin(enc->port, enc->pin_a) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t b = (HAL_GPIO_ReadPin(enc->port, enc->pin_b) == GPIO_PIN_SET) ? 1 : 0;
    uint8_t current_state = (b << 1) | a;  // combine into 2-bit state (BA)

    // look up rotation direction from quadrature table
    int8_t direction = quadrature_table[enc->last_state][current_state];

    // only accumulate valid transitions (filter out invalid/noise)
    if (direction != 0) {
        enc->delta += (direction * enc->direction_multiplier);
    }

    // update last state
    enc->last_state = current_state;
}

// <---- exti interrupt handlers ---->

// exti0 isr: triggered by pa0 (player 1 encoder channel a)
void EXTI0_IRQHandler(void) {
    // clear the exti pending bit for pin 0
    __HAL_GPIO_EXTI_CLEAR_IT(PLAYER1_ENCODER_A_PIN);

    // process player 1 encoder state change
    encoder_process_isr(ENCODER_PLAYER1);
}

// exti1 isr: triggered by pa1 (player 1 encoder channel b)
void EXTI1_IRQHandler(void) {
    // clear the exti pending bit for pin 1
    __HAL_GPIO_EXTI_CLEAR_IT(PLAYER1_ENCODER_B_PIN);

    // process player 1 encoder state change
    encoder_process_isr(ENCODER_PLAYER1);
}

// exti15_10 isr: triggered by pb12 or pb13 (player 2 encoder channels a and b)
void EXTI15_10_IRQHandler(void) {
    // check which pin triggered and clear it
    if (__HAL_GPIO_EXTI_GET_IT(PLAYER2_ENCODER_A_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(PLAYER2_ENCODER_A_PIN);
    }
    if (__HAL_GPIO_EXTI_GET_IT(PLAYER2_ENCODER_B_PIN) != RESET) {
        __HAL_GPIO_EXTI_CLEAR_IT(PLAYER2_ENCODER_B_PIN);
    }

    // process player 2 encoder state change (same logic regardless of which pin triggered)
    encoder_process_isr(ENCODER_PLAYER2);
}

// <---- encoder reading ---->

static int8_t encoder_get_delta(encoder_id_t encoder) {
    if (encoder >= ENCODER_COUNT) {
        return 0;
    }

    // disable interrupts briefly to prevent isr from modifying delta
    // between the read and the clear (atomic read-and-clear)
    __disable_irq();
    int8_t delta = encoders[encoder].delta;
    encoders[encoder].delta = 0;
    __enable_irq();

    return delta;
}


int8_t encoder_get_clicks(encoder_id_t encoder) {
    if (encoder >= ENCODER_COUNT) {
        return 0;
    }

    // accumulate raw transitions
    click_fractional[encoder] += encoder_get_delta(encoder);

    // convert to whole clicks
    int8_t clicks = 0;
    while (click_fractional[encoder] >= ENCODER_PULSES_PER_CLICK) {
        clicks++;
        click_fractional[encoder] -= ENCODER_PULSES_PER_CLICK;
    }
    while (click_fractional[encoder] <= -ENCODER_PULSES_PER_CLICK) {
        clicks--;
        click_fractional[encoder] += ENCODER_PULSES_PER_CLICK;
    }
    return clicks;
}