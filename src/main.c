/*
 * test program for chess clock hardware
 * tests display, buttons, encoders, and buzzer
 */

#include "stm32f1xx_hal.h"
#include "config.h"
#include "hardware.h"
#include "display.h"
#include "button.h"
#include "encoder.h"
#include <stdio.h>
#include <string.h>

// encoder positions for testing
static int32_t encoder1_position = 0;
static int32_t encoder2_position = 0;

// raw transition counters for debug
static int32_t encoder1_raw_count = 0;
static int32_t encoder2_raw_count = 0;

// last pressed button name
static const char* last_button_name = "NONE";

// button name lookup table
static const char* button_names[BUTTON_COUNT] = {
    "P1_ENC_PUSH",
    "P1_MENU",
    "P1_BACK",
    "P1_TAP",
    "P2_ENC_PUSH",
    "P2_MENU",
    "P2_BACK",
    "P2_TAP"
};

// function prototypes
void system_clock_config(void);
void test_buttons(void);
void test_encoders(void);
void update_display(I2C_HandleTypeDef *i2c, int32_t enc_pos, const char *btn_name);

int main(void) {
    // initialize hal
    HAL_Init();
    
    // initialize all hardware
    hardware_init();
    
    // initialize modules
    button_init();
    encoder_init();
    
    // get i2c handles
    I2C_HandleTypeDef *i2c1 = hardware_get_i2c1();
    I2C_HandleTypeDef *i2c2 = hardware_get_i2c2();
    
    // initialize displays
    HAL_Delay(100);
    display_init(i2c1);
    display_init(i2c2);
    
    // clear displays
    display_clear(i2c1);
    display_clear(i2c2);
    
    // clear displays and show static text once
    display_clear(i2c1);
    display_clear(i2c2);
    display_draw_string(i2c1, 0, 2, "Hardware Test");
    display_draw_string(i2c1, 0, 3, "Press buttons");
    display_draw_string(i2c1, 0, 4, "Turn encoders");
    display_draw_string(i2c2, 0, 2, "Hardware Test");
    display_draw_string(i2c2, 0, 3, "Press buttons");
    display_draw_string(i2c2, 0, 4, "Turn encoders");
    
    // show initial dynamic text
    update_display(i2c1, 0, "NONE");
    update_display(i2c2, 0, "NONE");
    
    // track previous values to detect changes
    int32_t last_enc1_pos = 0;
    int32_t last_enc2_pos = 0;
    int32_t last_enc1_raw = 0;
    int32_t last_enc2_raw = 0;
    const char* last_displayed_button = "NONE";
    uint8_t display_needs_update = FALSE;
    
    // main test loop
    while (1) {
        // update button states (encoder is now interrupt-driven, no polling needed)
        button_update();
        
        // test buttons
        test_buttons();
        
        // test encoders
        test_encoders();
        
        // check if display needs updating (data changed)
        if (encoder1_position != last_enc1_pos || 
            encoder2_position != last_enc2_pos ||
            encoder1_raw_count != last_enc1_raw ||
            encoder2_raw_count != last_enc2_raw ||
            last_button_name != last_displayed_button) {
            
            display_needs_update = TRUE;
            last_enc1_pos = encoder1_position;
            last_enc2_pos = encoder2_position;
            last_enc1_raw = encoder1_raw_count;
            last_enc2_raw = encoder2_raw_count;
            last_displayed_button = last_button_name;
        }
        
        // update displays only when needed (no full clear, just update changed lines)
        static uint32_t last_display_update = 0;
        if (display_needs_update && (HAL_GetTick() - last_display_update) >= 100) {
            last_display_update = HAL_GetTick();
            display_needs_update = FALSE;
            
            // only update dynamic parts (no full clear)
            update_display(i2c1, encoder1_position, last_button_name);
            update_display(i2c2, encoder2_position, last_button_name);
        }
        
        // small delay to prevent cpu spinning too fast
        HAL_Delay(1);
    }
}

void test_buttons(void) {
    // check all buttons
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        if (button_was_pressed((button_id_t)i)) {
            // update last button name
            last_button_name = button_names[i];
            
            // if menu button pressed, toggle buzzer
            if (i == BUTTON_PLAYER1_MENU || i == BUTTON_PLAYER2_MENU) {
                hardware_buzzer_on();
                HAL_Delay(100);  // beep for 100ms
                hardware_buzzer_off();
            }
        }
    }
}

void test_encoders(void) {
    // read encoder deltas (in quadrature transitions)
    int8_t delta1 = encoder_get_delta(ENCODER_PLAYER1);
    int8_t delta2 = encoder_get_delta(ENCODER_PLAYER2);
    
    // track raw transitions for debug
    encoder1_raw_count += delta1;
    encoder2_raw_count += delta2;
    
    // convert from transitions to clicks (4 transitions per click)
    // accumulate fractional clicks
    static int8_t frac1 = 0;
    static int8_t frac2 = 0;
    
    frac1 += delta1;
    frac2 += delta2;
    
    // only update position when we have a full click (4 transitions)
    if (frac1 >= 4) {
        encoder1_position++;
        frac1 -= 4;
    } else if (frac1 <= -4) {
        encoder1_position--;
        frac1 += 4;
    }
    
    if (frac2 >= 4) {
        encoder2_position++;
        frac2 -= 4;
    } else if (frac2 <= -4) {
        encoder2_position--;
        frac2 += 4;
    }
}

void update_display(I2C_HandleTypeDef *i2c, int32_t enc_pos, const char *btn_name) {
    char buffer[32];
    
    // clear only the lines we're updating (page 0, 1, and 5 for debug)
    // clear page 0 (button line)
    display_set_position(i2c, 0, 0);
    uint8_t clear_data[129];
    clear_data[0] = 0x40;  // data mode
    for (int i = 1; i < 129; i++) {
        clear_data[i] = 0x00;
    }
    HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);
    
    // clear page 1 (encoder clicks line)
    display_set_position(i2c, 0, 1);
    HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);
    
    // clear page 5 (raw transitions debug line)
    display_set_position(i2c, 0, 5);
    HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);
    
    // show last button pressed on page 0
    snprintf(buffer, sizeof(buffer), "Btn: %s", btn_name);
    display_draw_string(i2c, 0, 0, buffer);
    
    // show encoder clicks on page 1
    snprintf(buffer, sizeof(buffer), "Clicks: %ld", enc_pos);
    display_draw_string(i2c, 0, 1, buffer);
    
    // show raw transitions on page 5 (debug)
    if (i2c == hardware_get_i2c1()) {
        snprintf(buffer, sizeof(buffer), "Raw: %ld", encoder1_raw_count);
    } else {
        snprintf(buffer, sizeof(buffer), "Raw: %ld", encoder2_raw_count);
    }
    display_draw_string(i2c, 0, 5, buffer);
}

// required for hal
void SysTick_Handler(void) {
    HAL_IncTick();
}