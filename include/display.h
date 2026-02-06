/*
 * display driver interface for sh1106 oled displays
 * provides functions for initializing and drawing on the displays
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"
#include "stm32f1xx_hal.h"
#include <stdint.h>

// <---- display initialization ---->

/**
 * initialize sh1106 display
 * must be called before any other display functions
 * 
 * @param i2c_handle pointer to initialized i2c handle
 */
void display_init(I2C_HandleTypeDef *i2c_handle);

// <---- display control ---->

/**
 * clear entire display
 * sets all pixels to off
 * 
 * @param i2c_handle pointer to i2c handle for this display
 */
void display_clear(I2C_HandleTypeDef *i2c_handle);

/**
 * set cursor position for next draw operation
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param x_position horizontal position in pixels (0-127)
 * @param page vertical page (0-7, each page is 8 pixels tall)
 */
void display_set_position(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page);

// <---- text drawing ---->

/**
 * draw single character at specified position
 * uses 5x7 pixel font
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param x_position horizontal position in pixels (0-127)
 * @param page vertical page (0-7)
 * @param character character to draw (ascii 32-122 supported: space through z)
 */
void display_draw_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page, char character);

/**
 * draw string at specified position
 * automatically advances cursor for each character
 * uses 5x7 pixel font
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param x_position starting horizontal position in pixels (0-127)
 * @param page vertical page (0-7)
 * @param text null-terminated string to draw
 */
void display_draw_string(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page, const char *text);

/**
 * draw large character at specified position
 * uses 16x24 pixel font, spans 3 pages vertically
 * supports digits 0-9, colon, and period
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param x_position horizontal position in pixels (0-127)
 * @param start_page starting vertical page (0-5, since it spans 3 pages)
 * @param character character to draw
 */
void display_draw_large_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t start_page, char character);

/**
 * draw medium character at specified position
 * uses 8x16 pixel font, spans 2 pages vertically
 * supports digits 0-9 and 's'
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param x_position horizontal position in pixels (0-127)
 * @param start_page starting vertical page (0-6, since it spans 2 pages)
 * @param character character to draw
 */
void display_draw_medium_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t start_page, char character);

// <---- clock display ---->

/**
 * draw time as large centered clock with mode header
 * always displays HH:MM:SS format
 * shows mode name on top left and bonus time on top right
 * optionally shows "Ready!" below clock when player is ready
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param time_milliseconds main time to display in milliseconds
 * @param mode time control mode (for header label)
 * @param bonus_time_milliseconds bonus time to display (seconds value in top right)
 * @param is_ready if TRUE, displays "Ready!" below the clock
 */
void display_draw_clock(I2C_HandleTypeDef *i2c_handle, uint32_t time_milliseconds, time_control_mode_t mode, uint32_t bonus_time_milliseconds, uint8_t is_ready);

// <---- low-level communication ---->

/**
 * send command to sh1106 display
 * internal function, typically not called directly
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param command command byte to send
 */
void display_send_command(I2C_HandleTypeDef *i2c_handle, uint8_t command);

#endif // DISPLAY_H