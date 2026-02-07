/*
 * display driver implementation for sh1106 oled displays
 */

#include "display.h"
#include "config.h"
#include "fonts.h"
#include <string.h>
#include <stdio.h>

// <---- sh1106 command definitions ---->

#define SH1106_CMD_DISPLAY_OFF        0xAE
#define SH1106_CMD_DISPLAY_ON         0xAF
#define SH1106_CMD_SET_CONTRAST       0x81
#define SH1106_CMD_SET_SEGMENT_REMAP  0xA1
#define SH1106_CMD_SET_SCAN_DIR       0xC8
#define SH1106_CMD_SET_MULTIPLEX      0xA8
#define SH1106_CMD_SET_DISPLAY_OFFSET 0xD3
#define SH1106_CMD_SET_CLOCK_DIV      0xD5
#define SH1106_CMD_SET_PRECHARGE      0xD9
#define SH1106_CMD_SET_COM_PINS       0xDA
#define SH1106_CMD_SET_VCOM_DESELECT  0xDB
#define SH1106_CMD_CHARGE_PUMP        0x8D
#define SH1106_CMD_SET_PAGE_ADDR      0xB0
#define SH1106_CMD_SET_COLUMN_LOW     0x00
#define SH1106_CMD_SET_COLUMN_HIGH    0x10

void display_send_command(I2C_HandleTypeDef *i2c_handle, uint8_t command) {
    uint8_t data[2];
    data[0] = 0x00;  // command mode
    data[1] = command;
    HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, data, 2, 100);
}

// <---- display initialization ---->

void display_init(I2C_HandleTypeDef *i2c_handle) {
    // wait for display to power up
    HAL_Delay(100);
    
    // send initialization sequence
    display_send_command(i2c_handle, SH1106_CMD_DISPLAY_OFF);
    display_send_command(i2c_handle, SH1106_CMD_SET_CLOCK_DIV);
    display_send_command(i2c_handle, 0x80);
    display_send_command(i2c_handle, SH1106_CMD_SET_MULTIPLEX);
    display_send_command(i2c_handle, 0x3F);
    display_send_command(i2c_handle, SH1106_CMD_SET_DISPLAY_OFFSET);
    display_send_command(i2c_handle, 0x00);
    display_send_command(i2c_handle, 0x40 | 0x00);  // set start line
    display_send_command(i2c_handle, SH1106_CMD_CHARGE_PUMP);
    display_send_command(i2c_handle, 0x14);  // enable charge pump
    display_send_command(i2c_handle, SH1106_CMD_SET_SEGMENT_REMAP);
    display_send_command(i2c_handle, SH1106_CMD_SET_SCAN_DIR);
    display_send_command(i2c_handle, SH1106_CMD_SET_COM_PINS);
    display_send_command(i2c_handle, 0x12);
    display_send_command(i2c_handle, SH1106_CMD_SET_CONTRAST);
    display_send_command(i2c_handle, 0xCF);
    display_send_command(i2c_handle, SH1106_CMD_SET_PRECHARGE);
    display_send_command(i2c_handle, 0xF1);
    display_send_command(i2c_handle, SH1106_CMD_SET_VCOM_DESELECT);
    display_send_command(i2c_handle, 0x40);
    display_send_command(i2c_handle, 0xA4);  // display from ram
    display_send_command(i2c_handle, 0xA6);  // normal display (not inverted)
    display_send_command(i2c_handle, SH1106_CMD_DISPLAY_ON);
    
    HAL_Delay(100);
}

// <---- display control ---->

void display_set_position(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page) {
    // sh1106 has 2 pixel horizontal offset
    x_position += 2;
    
    // set page address (0xB0 | page)
    display_send_command(i2c_handle, SH1106_CMD_SET_PAGE_ADDR | (page & 0x07));
    
    // set column address (split into low and high nibbles)
    display_send_command(i2c_handle, SH1106_CMD_SET_COLUMN_LOW | (x_position & 0x0F));
    display_send_command(i2c_handle, SH1106_CMD_SET_COLUMN_HIGH | ((x_position >> 4) & 0x0F));
}

void display_clear(I2C_HandleTypeDef *i2c_handle) {
    uint8_t data[129];
    data[0] = 0x40;  // data mode
    
    // fill with zeros
    for (int i = 1; i < 129; i++) {
        data[i] = 0x00;
    }
    
    // clear all 8 pages
    for (uint8_t page = 0; page < DISPLAY_PAGES; page++) {
        display_set_position(i2c_handle, 0, page);
        HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, data, 129, 1000);
    }
}

// <---- text drawing ---->

void display_draw_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page, char character) {
    uint8_t font_index;
    
    // map character to font array (ascii 32-122: space through z)
    if (character >= 32 && character <= 122) {
        font_index = character - 32;
    } else {
        font_index = 0;  // default to space
    }
    
    // set cursor position
    display_set_position(i2c_handle, x_position, page);
    
    // send character pixel data (5 columns)
    uint8_t data[6];
    data[0] = 0x40;  // data mode
    for (uint8_t i = 0; i < 5; i++) {
        data[i + 1] = font_5x7[font_index][i];
    }
    
    HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, data, 6, 100);
}

void display_draw_string(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t page, const char *text) {
    uint8_t current_x = x_position;
    
    // draw each character
    while (*text) {
        display_draw_character(i2c_handle, current_x, page, *text);
        current_x += 6;  // advance by character width (5 pixels + 1 spacing)
        text++;
    }
}

// <---- large character drawing ---->

void display_draw_large_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t start_page, char character) {
    uint8_t digit_index;
    
    // only digits 0-9 are supported
    if (character >= '0' && character <= '9') {
        digit_index = character - '0';
    } else {
        return;  // unsupported character
    }
    
    // large character spans 3 pages (24 pixels tall)
    // send data for all 16 columns across 3 pages
    for (uint8_t page_offset = 0; page_offset < 3; page_offset++) {
        display_set_position(i2c_handle, x_position, start_page + page_offset);
        
        // send 16 columns for this page
        uint8_t data[17];
        data[0] = 0x40;  // data mode
        
        for (uint8_t col = 0; col < 16; col++) {
            // each column has 3 bytes (one per page)
            // index: col * 3 + page_offset
            data[col + 1] = font_large_16x24[digit_index][col * 3 + page_offset];
        }
        
        HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, data, 17, 100);
    }
}

// <---- medium character drawing ---->

void display_draw_medium_character(I2C_HandleTypeDef *i2c_handle, uint8_t x_position, uint8_t start_page, char character) {
    uint8_t char_index;
    
    // map character to font array (digits 0-9 or 's')
    if (character >= '0' && character <= '9') {
        char_index = character - '0';
    } else if (character == 's') {
        char_index = 10;
    } else {
        return;  // unsupported character
    }
    
    // medium character spans 2 pages (16 pixels tall)
    // send data for all 8 columns across 2 pages
    for (uint8_t page_offset = 0; page_offset < 2; page_offset++) {
        display_set_position(i2c_handle, x_position, start_page + page_offset);
        
        // send 8 columns for this page
        uint8_t data[9];
        data[0] = 0x40;  // data mode
        
        for (uint8_t col = 0; col < 8; col++) {
            // each column has 2 bytes (one per page)
            // index: col * 2 + page_offset
            data[col + 1] = font_medium_8x16[char_index][col * 2 + page_offset];
        }
        
        HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, data, 9, 100);
    }
}

// <---- clock screen elements ---->

void display_draw_header(I2C_HandleTypeDef *i2c_handle, time_control_mode_t mode, uint32_t bonus_time_milliseconds){
    // convert bonus time to seconds
    uint32_t bonus_seconds = bonus_time_milliseconds / 1000;
    
    // draw header on page 0
    // mode name on left, bonus time display depends on mode
    char header_left[32];  // for mode name + configured time
    uint8_t show_dynamic_countdown = FALSE;
    
    switch (mode) {
        case TIME_CONTROL_NONE:
            snprintf(header_left, sizeof(header_left), "None");
            break;
        case TIME_CONTROL_INCREMENT:
            snprintf(header_left, sizeof(header_left), "Increment - %lus", bonus_seconds);
            break;
        case TIME_CONTROL_DELAY:
            snprintf(header_left, sizeof(header_left), "Delay");
            show_dynamic_countdown = TRUE;
            break;
        case TIME_CONTROL_PARTIAL:
            snprintf(header_left, sizeof(header_left), "Partial - %lus", bonus_seconds);
            break;
        case TIME_CONTROL_LIMITED:
            snprintf(header_left, sizeof(header_left), "Limited");
            show_dynamic_countdown = TRUE;
            break;
        case TIME_CONTROL_BYOYOMI:
            snprintf(header_left, sizeof(header_left), "Byo-yomi - %lus", bonus_seconds);
            break;
        default:
            snprintf(header_left, sizeof(header_left), "Unknown");
            break;
    }
    
    display_draw_string(i2c_handle, 0, 0, header_left);
    
    // draw dynamic countdown on right only for delay and limited modes
    // uses medium font (8x16, spans 2 pages) for better visibility
    if (show_dynamic_countdown) {
        uint32_t bonus_seconds = bonus_time_milliseconds / 1000;
        
        // format as individual digits to draw with medium font
        // calculate number of digits needed
        uint8_t num_digits;
        if (bonus_seconds >= 100) num_digits = 3;
        else if (bonus_seconds >= 10) num_digits = 2;
        else num_digits = 1;
        
        // calculate starting x position for right alignment
        // each medium char is 8 pixels wide, plus 's' = ((num_digits + 1) * 8)
        uint8_t total_width = (num_digits + 1) * 8;
        uint8_t start_x = 128 - total_width;
        uint8_t current_x = start_x;
        
        // draw each digit
        if (num_digits == 3) {
            display_draw_medium_character(i2c_handle, current_x, 0, '0' + (bonus_seconds / 100));
            current_x += 8;
        }
        if (num_digits >= 2) {
            display_draw_medium_character(i2c_handle, current_x, 0, '0' + ((bonus_seconds / 10) % 10));
            current_x += 8;
        }
        display_draw_medium_character(i2c_handle, current_x, 0, '0' + (bonus_seconds % 10));
        current_x += 8;
        
        // draw 's' suffix
        display_draw_medium_character(i2c_handle, current_x, 0, 's');
    }
}

void display_draw_clock(I2C_HandleTypeDef *i2c_handle, uint32_t time_milliseconds) {
    // convert milliseconds to time components
    uint32_t total_seconds = time_milliseconds / 1000;
    uint8_t hours = total_seconds / 3600;
    uint8_t minutes = (total_seconds % 3600) / 60;
    uint8_t seconds = total_seconds % 60;

    // draw large clock centered (shifted down to page 3-5 to make room for header)
    uint8_t start_page = 3;  // pages 3-5 (shifted down from 2-4)
    
    // format as HH:MM:SS (8 characters)
    // layout: HH : MM : SS
    // digit width: 16px, colon width: 6px, spacing: 2px
    // total: 16+16+2+6+2+16+16+2+6+2+16+16 = 116 pixels
    // centered: (128-116)/2 = 6 pixels from left
    
    uint8_t x = 6;
    
    // draw hours
    display_draw_large_character(i2c_handle, x, start_page, '0' + (hours / 10));
    x += 16;
    display_draw_large_character(i2c_handle, x, start_page, '0' + (hours % 10));
    x += 16 + 2;
    
    // draw first colon (two dots vertically centered)
    display_set_position(i2c_handle, x, start_page + 1);
    uint8_t colon_data[] = {0x40, 0x00, 0x66, 0x66, 0x00, 0x00};  // two dots pattern
    HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, colon_data, 6, 100);
    x += 6 + 2;
    
    // draw minutes
    display_draw_large_character(i2c_handle, x, start_page, '0' + (minutes / 10));
    x += 16;
    display_draw_large_character(i2c_handle, x, start_page, '0' + (minutes % 10));
    x += 16 + 2;
    
    // draw second colon
    display_set_position(i2c_handle, x, start_page + 1);
    HAL_I2C_Master_Transmit(i2c_handle, DISPLAY_I2C_ADDRESS, colon_data, 6, 100);
    x += 6 + 2;
    
    // draw seconds
    display_draw_large_character(i2c_handle, x, start_page, '0' + (seconds / 10));
    x += 16;
    display_draw_large_character(i2c_handle, x, start_page, '0' + (seconds % 10));
    
}

void display_draw_footer(I2C_HandleTypeDef *i2c_handle, uint8_t is_ready){

    // draw "Ready!" below clock if player is ready
    if (is_ready) {
        // "Ready!" is 6 characters × 6 pixels = 36 pixels wide
        // center on 128 pixel display: (128-36)/2 = 46 pixels from left
        display_draw_string(i2c_handle, 46, 7, "Ready!");
    }

}
void display_draw_clock_screen(I2C_HandleTypeDef *i2c_handle, uint32_t time_milliseconds, time_control_mode_t mode, uint32_t bonus_time_millisecond, uint8_t is_ready){

    display_draw_header(i2c_handle, mode, bonus_time_millisecond);

    display_draw_clock(i2c_handle, time_milliseconds);

    display_draw_footer(i2c_handle, is_ready);


}