/*
 * button module implementation
 */

#include "button.h"
#include "stm32f1xx_hal.h"

// <---- button state tracking ---->

typedef struct {
    GPIO_TypeDef *port;         // gpio port for this button
    uint16_t pin;               // gpio pin for this button
    uint8_t current_state;      // current debounced state (TRUE = pressed)
    uint8_t last_state;         // previous debounced state
    uint8_t raw_state;          // current raw pin reading
    uint32_t state_change_time; // timestamp when raw state last changed
    uint8_t pressed_flag;       // flag for edge detection (was pressed)
    uint8_t released_flag;      // flag for edge detection (was released)
    uint32_t press_start_time;  // timestamp when button was pressed
} button_state_t;

// array of all button states
static button_state_t buttons[BUTTON_COUNT];

// <---- button configuration table ---->

// maps button id to gpio port and pin
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} button_config_t;

static const button_config_t button_configs[BUTTON_COUNT] = {
    {PLAYER1_BUTTON_PORT, PLAYER1_ENCODER_PUSH_PIN},  // BUTTON_PLAYER1_ENCODER_PUSH
    {PLAYER1_BUTTON_PORT, PLAYER1_MENU_BUTTON_PIN},   // BUTTON_PLAYER1_MENU
    {PLAYER1_BUTTON_PORT, PLAYER1_BACK_BUTTON_PIN},   // BUTTON_PLAYER1_BACK
    {PLAYER1_BUTTON_PORT, PLAYER1_TAP_BUTTON_PIN},    // BUTTON_PLAYER1_TAP
    {PLAYER2_BUTTON_PORT_B, PLAYER2_ENCODER_PUSH_PIN}, // BUTTON_PLAYER2_ENCODER_PUSH
    {PLAYER2_BUTTON_PORT_B, PLAYER2_MENU_BUTTON_PIN},  // BUTTON_PLAYER2_MENU
    {PLAYER2_BUTTON_PORT_A, PLAYER2_BACK_BUTTON_PIN},  // BUTTON_PLAYER2_BACK
    {PLAYER2_BUTTON_PORT_A, PLAYER2_TAP_BUTTON_PIN}    // BUTTON_PLAYER2_TAP
};

// <---- initialization ---->

void button_init(void) {
    // initialize all button states
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].port = button_configs[i].port;
        buttons[i].pin = button_configs[i].pin;
        buttons[i].current_state = FALSE;
        buttons[i].last_state = FALSE;
        buttons[i].raw_state = FALSE;
        buttons[i].state_change_time = 0;
        buttons[i].pressed_flag = FALSE;
        buttons[i].released_flag = FALSE;
        buttons[i].press_start_time = 0;
    }
}

// <---- button update ---->

void button_update(void) {
    // update all buttons
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        button_state_t *btn = &buttons[i];
        
        // read raw pin state (buttons are active-low, so invert)
        GPIO_PinState pin_state = HAL_GPIO_ReadPin(btn->port, btn->pin);
        uint8_t new_raw_state = (pin_state == GPIO_PIN_RESET) ? TRUE : FALSE;
        
        // check if raw state changed
        if (new_raw_state != btn->raw_state) {
            // state changed - record timestamp
            btn->raw_state = new_raw_state;
            btn->state_change_time = HAL_GetTick();
        }
        
        // check if raw state has been stable long enough
        uint32_t stable_time = HAL_GetTick() - btn->state_change_time;
        
        if (stable_time >= BUTTON_DEBOUNCE_TIME_MS && btn->raw_state != btn->current_state) {
            // raw state stable for debounce period and different from current state
            // accept the new state
            btn->last_state = btn->current_state;
            btn->current_state = btn->raw_state;
            
            // detect edges and set flags
            if (btn->current_state && !btn->last_state) {
                // rising edge: button just pressed
                btn->pressed_flag = TRUE;
                btn->press_start_time = HAL_GetTick();
            } else if (!btn->current_state && btn->last_state) {
                // falling edge: button just released
                btn->released_flag = TRUE;
                btn->press_start_time = 0;
            }
        }
    }
}

// <---- button state functions ---->

uint8_t button_was_pressed(button_id_t button) {
    if (button >= BUTTON_COUNT) {
        return FALSE;
    }
    
    // check and clear the pressed flag
    if (buttons[button].pressed_flag) {
        buttons[button].pressed_flag = FALSE;  // clear flag after reading
        return TRUE;
    }
    
    return FALSE;
}

uint8_t button_was_released(button_id_t button) {
    if (button >= BUTTON_COUNT) {
        return FALSE;
    }
    
    // check and clear the released flag
    if (buttons[button].released_flag) {
        buttons[button].released_flag = FALSE;  // clear flag after reading
        return TRUE;
    }
    
    return FALSE;
}

uint8_t button_is_held(button_id_t button, uint32_t hold_time_ms) {
    if (button >= BUTTON_COUNT) {
        return FALSE;
    }
    
    button_state_t *btn = &buttons[button];
    
    // check if button is currently pressed and has been held long enough
    if (btn->current_state && btn->press_start_time > 0) {
        uint32_t hold_duration = HAL_GetTick() - btn->press_start_time;
        return (hold_duration >= hold_time_ms) ? TRUE : FALSE;
    }
    
    return FALSE;
}