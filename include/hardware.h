/*
 * hardware module
 * handles all low-level hardware initialization and control
 */

#ifndef HARDWARE_H
#define HARDWARE_H

#include "stm32f1xx_hal.h"

// <---- initialization functions ---->

/**
 * initialize all hardware subsystems
 * calls all hardware_init_* functions in correct order
 * must be called after HAL_Init() and before any other hardware use
 */
void hardware_init(void);

/**
 * configure system clock to 8mhz using internal hsi oscillator
 * uses hsi directly without pll for lower power consumption
 */
void hardware_init_system_clock(void);

/**
 * initialize all gpio pins
 * configures buttons (active-low with pull-up)
 * configures encoder pins (input with pull-up)
 */
void hardware_init_gpio(void);

/**
 * initialize both i2c peripherals for displays
 * configures i2c1 (player 1) and i2c2 (player 2)
 * 400khz fast mode, 7-bit addressing
 */
void hardware_init_i2c(void);

/**
 * initialize timers
 * configures TIM2 for 100ms clock ticks (does not start it)
 * configures TIM3 for buzzer pwm at 8500hz (does not start it)
 */
void hardware_init_timers(void);

// <---- clock timer control ---->

/**
 * start clock tick timer (TIM2)
 * generates interrupts every 100ms for game timing
 * call when game starts
 */
void hardware_start_clock_timer(void);

/**
 * stop clock tick timer (TIM2)
 * stops interrupt generation
 * call when game pauses or ends
 */
void hardware_stop_clock_timer(void);

// <---- buzzer control ---->

/**
 * turn buzzer on
 * starts pwm at 8500hz with 90% duty cycle
 */
void hardware_buzzer_on(void);

/**
 * turn buzzer off
 * stops pwm output completely
 */
void hardware_buzzer_off(void);

// <---- timer handle access ---->

/**
 * get tim2 handle for clock tick timer
 * needed by main.c for the TIM2 interrupt handler
 *
 * @return pointer to tim2 handle
 */
TIM_HandleTypeDef* hardware_get_tim2(void);

// <---- i2c handle access ---->

/**
 * get i2c1 handle for player 1 display
 * @return pointer to i2c1 handle
 */
I2C_HandleTypeDef* hardware_get_i2c1(void);

/**
 * get i2c2 handle for player 2 display
 * @return pointer to i2c2 handle
 */
I2C_HandleTypeDef* hardware_get_i2c2(void);

#endif  // HARDWARE_H