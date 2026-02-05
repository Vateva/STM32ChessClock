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
 * @param character character to draw (ascii 32-90 supported)
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

// <---- clock display ---->

/**
 * draw time as large centered clock
 * automatically formats based on time value:
 *   - if time >= 1 hour: displays as HH:MM:SS
 *   - if time < 1 hour: displays as MM:SS.d (with deciseconds)
 * 
 * @param i2c_handle pointer to i2c handle for this display
 * @param time_milliseconds time to display in milliseconds
 */
void display_draw_clock(I2C_HandleTypeDef *i2c_handle, uint32_t time_milliseconds);

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