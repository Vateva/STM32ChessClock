/*
 * configuration file for chess clock
 * contains all pin definitions, hardware addresses, and configurable parameters
 */

#ifndef CONFIG_H
#define CONFIG_H

#include "stm32f1xx_hal.h"

// <---- boolean definitions ---->

#define FALSE 0
#define TRUE  1

// <---- player 1 hardware pin definitions ---->

// player 1 i2c pins for display
#define PLAYER1_I2C_SCL_PIN              GPIO_PIN_6
#define PLAYER1_I2C_SDA_PIN              GPIO_PIN_7
#define PLAYER1_I2C_PORT                 GPIOB
#define PLAYER1_I2C_INSTANCE             I2C1

// player 1 rotary encoder pins
#define PLAYER1_ENCODER_A_PIN            GPIO_PIN_0
#define PLAYER1_ENCODER_B_PIN            GPIO_PIN_1
#define PLAYER1_ENCODER_PORT             GPIOA

// player 1 button pins (all on same port)
#define PLAYER1_ENCODER_PUSH_PIN         GPIO_PIN_2
#define PLAYER1_MENU_BUTTON_PIN          GPIO_PIN_3
#define PLAYER1_BACK_BUTTON_PIN          GPIO_PIN_4
#define PLAYER1_TAP_BUTTON_PIN           GPIO_PIN_15
#define PLAYER1_BUTTON_PORT              GPIOA

// <---- player 2 hardware pin definitions ---->

// player 2 i2c pins for display
#define PLAYER2_I2C_SCL_PIN              GPIO_PIN_10
#define PLAYER2_I2C_SDA_PIN              GPIO_PIN_11
#define PLAYER2_I2C_PORT                 GPIOB
#define PLAYER2_I2C_INSTANCE             I2C2

// player 2 rotary encoder pins
#define PLAYER2_ENCODER_A_PIN            GPIO_PIN_12
#define PLAYER2_ENCODER_B_PIN            GPIO_PIN_13
#define PLAYER2_ENCODER_PORT             GPIOB

// player 2 button pins (split across two ports)
#define PLAYER2_ENCODER_PUSH_PIN         GPIO_PIN_14
#define PLAYER2_MENU_BUTTON_PIN          GPIO_PIN_15
#define PLAYER2_BUTTON_PORT_B            GPIOB
#define PLAYER2_BACK_BUTTON_PIN          GPIO_PIN_8
#define PLAYER2_TAP_BUTTON_PIN           GPIO_PIN_9
#define PLAYER2_BUTTON_PORT_A            GPIOA

// <---- buzzer pin definitions ---->

#define BUZZER_PIN                       GPIO_PIN_6
#define BUZZER_PORT                      GPIOA
#define BUZZER_TIMER_INSTANCE            TIM3
#define BUZZER_TIMER_CHANNEL             TIM_CHANNEL_1

// <---- display configuration (sh1106 oled) ---->

#define DISPLAY_I2C_ADDRESS              (0x3C << 1)  // shift for hal i2c format
#define DISPLAY_WIDTH                    128
#define DISPLAY_HEIGHT                   64
#define DISPLAY_PAGES                    8            // 64 pixels / 8 = 8 pages

// <---- timing configuration ---->

// system clock configuration
#define SYSTEM_CLOCK_FREQUENCY_MHZ       64           // using hsi oscillator

// timer configuration for buzzer pwm
#define BUZZER_PWM_FREQUENCY_HZ          8500         // resonant frequency of piezo
#define BUZZER_PWM_DUTY_CYCLE_PERCENT    90           // percentage (0-100)

// clock tick precision
#define CLOCK_TICK_INTERVAL_MS           100          // update every 100ms (decisecond)

// <---- chess clock defaults and limits ---->

// default time configuration (in milliseconds)
#define DEFAULT_STARTING_TIME_MS         300000       // 5 minutes
#define DEFAULT_BONUS_TIME_MS            0            // default increment/delay amount

// time limits (in milliseconds)
#define MIN_STARTING_TIME_MS             1000         // 1 second minimum
#define MAX_STARTING_TIME_MS             86400000     // 24 hours maximum
#define MIN_BONUS_TIME_MS                0            // no bonus time minimum
#define MAX_BONUS_TIME_MS                60000        // 60 seconds maximum

// time editor configuration
#define TIME_EDITOR_HOURS_MAX            23           // 0-23 hours
#define TIME_EDITOR_MINUTES_MAX          59           // 0-59 minutes
#define TIME_EDITOR_SECONDS_MAX          59           // 0-59 seconds
#define BONUS_TIME_EDITOR_SECONDS_MAX    59           // 0-59 seconds for increment/delay

// <---- encoder configuration ---->

#define ENCODER_CLICKS_PER_ROTATION      20           // physical detents per full rotation
#define ENCODER_PULSES_PER_CLICK         2            // quadrature pulses per click

// <---- button configuration ---->

#define BUTTON_ACTIVE_STATE              GPIO_PIN_RESET  // buttons are active-low
#define BUTTON_DEBOUNCE_TIME_MS          50              // debounce delay
#define MENU_BUTTON_HOLD_TIME_MS         2000            // hold 2 seconds to enter menu

// <---- time control modes ---->

typedef enum {
    TIME_CONTROL_NONE,      // no bonus time
    TIME_CONTROL_INCREMENT, // fischer increment (add X, can accumulate)
    TIME_CONTROL_DELAY,     // bronstein delay (X second buffer before countdown)
    TIME_CONTROL_PARTIAL,   // partial increment (add X, capped at turn start time)
    TIME_CONTROL_LIMITED,   // limited time (must move within X seconds or lose)
    TIME_CONTROL_BYOYOMI    // byo-yomi (X seconds per move after main time expires)
} time_control_mode_t;

#define DEFAULT_TIME_CONTROL_MODE        TIME_CONTROL_NONE
#define TIME_CONTROL_MODE_COUNT          6  // total number of modes

// <---- interrupt priorities ---->

#define ENCODER_INTERRUPT_PRIORITY       5            // encoder gpio exti priority
#define TIMER_INTERRUPT_PRIORITY         3            // clock tick timer priority

#endif // CONFIG_H