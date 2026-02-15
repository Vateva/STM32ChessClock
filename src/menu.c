/*
 * menu module implementation
 * handles menu navigation, time editing, mode selection, and drawing
 * independent from game module — communicates through main.c via return actions
 */

#include "menu.h"
#include "display.h"
#include "encoder.h"
#include "button.h"
#include "hardware.h"
#include <string.h>
#include <stdio.h>

// <---- per-player menu state ---->

static menu_state_t menu_states[2];

// <---- per-player open/active flag ---->

static uint8_t menu_open_flags[2] = {FALSE, FALSE};

// <---- encoder long press tracking per player ---->

static uint8_t encoder_push_held[2] = {FALSE, FALSE};

// <---- display cache per player (tracks what is currently drawn) ---->

typedef struct {
    // main menu cache
    uint8_t main_menu_cursor;
    
    // time editor cache
    uint8_t edit_hours;
    uint8_t edit_minutes;
    uint8_t edit_seconds;
    uint8_t edit_bonus_seconds;
    uint8_t time_editor_field;
    
    // mode list cache
    uint8_t mode_list_cursor;
    uint8_t mode_editing;
    time_control_mode_t active_mode;  // which mode has the checkmark
    uint32_t bonus_values[TIME_CONTROL_MODE_COUNT];  // cached bonus values
    
    // reset confirm cache
    uint8_t reset_confirm_cursor;
       
    // blink state
    uint8_t last_blink_visible;
    
    // force full redraw flag
    uint8_t needs_full_redraw;
} menu_display_cache_t;

static menu_display_cache_t menu_cache[2];

// <---- encoder id mapping per player ---->

static const encoder_id_t player_encoders[2] = {
    ENCODER_PLAYER1,
    ENCODER_PLAYER2
};

// <---- button id mapping per player ---->

static const button_id_t player_encoder_push[2] = {
    BUTTON_PLAYER1_ENCODER_PUSH,
    BUTTON_PLAYER2_ENCODER_PUSH
};

static const button_id_t player_back[2] = {
    BUTTON_PLAYER1_BACK,
    BUTTON_PLAYER2_BACK
};

// <---- mode name strings ---->

static const char* mode_names[TIME_CONTROL_MODE_COUNT] = {
    "None",
    "Increment",
    "Delay",
    "Partial",
    "Limited",
    "Byo-yomi"
};

// <---- armed menu item strings ---->

static const char* armed_menu_items[MENU_ARMED_ITEM_COUNT] = {
    "Starting Time",
    "Time Control",
    "Ready!"
};

// <---- paused menu item strings ---->

static const char* paused_menu_items[MENU_PAUSED_ITEM_COUNT] = {
    "Current Time",
    "Current Time Control",
    "Reset",
    "Ready!"
};

// <---- internal helper: get i2c handle for a player ---->

static I2C_HandleTypeDef* get_player_i2c(uint8_t player_index) {
    if (player_index == 0) {
        return hardware_get_i2c1();
    }
    return hardware_get_i2c2();
}

// <---- internal helper: clear a rectangular area on display ---->

// clears a region defined by x start, pixel width, and page range
static void clear_display_area(I2C_HandleTypeDef* i2c,
                               uint8_t x_start, uint8_t width,
                               uint8_t start_page, uint8_t end_page) {
    // buffer: 1 byte data mode prefix + pixel data
    uint8_t data[33];
    data[0] = 0x40;
    uint8_t send_len = (width > 32) ? 32 : width;
    memset(&data[1], 0x00, send_len);

    for (uint8_t page = start_page; page <= end_page; page++) {
        display_set_position(i2c, x_start, page);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, data, send_len + 1, 100);
    }
}

// <---- internal helper: decompose milliseconds into h/m/s ---->

static void ms_to_hms(uint32_t ms, uint8_t* hours, uint8_t* minutes, uint8_t* seconds) {
    uint32_t total_seconds = ms / 1000;
    *hours = total_seconds / 3600;
    *minutes = (total_seconds % 3600) / 60;
    *seconds = total_seconds % 60;
}

// <---- internal helper: compose h/m/s back to milliseconds ---->

static uint32_t hms_to_ms(uint8_t hours, uint8_t minutes, uint8_t seconds) {
    return ((uint32_t)hours * 3600 + (uint32_t)minutes * 60 + (uint32_t)seconds) * 1000;
}

// <---- internal helper: draw main menu screen ---->

static void draw_main_menu(menu_state_t* state, I2C_HandleTypeDef* i2c, menu_display_cache_t* cache) {
    const char** items;
    uint8_t count;

    if (state->is_paused) {
        items = paused_menu_items;
        count = state->item_count;
    } else {
        items = armed_menu_items;
        count = state->item_count;
    }

    // full redraw: clear screen and draw everything
    if (cache->needs_full_redraw) {
        display_clear(i2c);

        // draw all menu items
        for (uint8_t i = 0; i < count; i++) {
            uint8_t page = i + 1;
            
            if (i == state->main_menu_cursor) {
                display_draw_string(i2c, 0, page, ">");
                display_draw_string(i2c, 8, page, items[i]);
            } else {
                display_draw_string(i2c, 8, page, items[i]);
            }
        }
        
        cache->main_menu_cursor = state->main_menu_cursor;
        cache->needs_full_redraw = FALSE;
        return;
    }

    // partial redraw: only update cursor if it moved
    if (cache->main_menu_cursor != state->main_menu_cursor) {
        uint8_t old_page = cache->main_menu_cursor + 1;
        uint8_t new_page = state->main_menu_cursor + 1;
        
        // clear old cursor position
        clear_display_area(i2c, 0, 6, old_page, old_page);
        
        // draw new cursor position
        display_draw_string(i2c, 0, new_page, ">");
        
        cache->main_menu_cursor = state->main_menu_cursor;
    }
}

// <---- internal helper: draw time editor screen ---->

static void draw_time_editor(menu_state_t* state, I2C_HandleTypeDef* i2c, menu_display_cache_t* cache) {
    uint32_t blink_phase = (HAL_GetTick() / MENU_BLINK_INTERVAL_MS) % 2;
    uint8_t blink_visible = (blink_phase == 0) ? TRUE : FALSE;
    uint8_t blink_state_changed = (blink_visible != cache->last_blink_visible);

    uint8_t start_page = 2;

    time_control_mode_t mode = state->config->time_control_mode;
    uint8_t is_increment_mode = (mode == TIME_CONTROL_INCREMENT ||
                                 mode == TIME_CONTROL_PARTIAL);
    uint8_t is_countdown_mode = (mode == TIME_CONTROL_DELAY ||
                                 mode == TIME_CONTROL_LIMITED ||
                                 mode == TIME_CONTROL_BYOYOMI);

    // full redraw: draw everything
    if (cache->needs_full_redraw) {
        display_clear(i2c);

        // draw header
        if (state->is_paused && is_increment_mode && state->current_bonus_to_edit != NULL) {
            if (state->time_editor_field != TIME_FIELD_BONUS) {
                char header_str[32];
                snprintf(header_str, sizeof(header_str), "%s - %02ds",
                         mode_names[mode], state->edit_bonus_seconds);
                display_draw_string(i2c, 0, 0, header_str);
            } else {
                char header_str[32];
                snprintf(header_str, sizeof(header_str), "%s - ", mode_names[mode]);
                display_draw_string(i2c, 0, 0, header_str);
                
                uint8_t mode_name_len = strlen(mode_names[mode]);
                uint8_t s_x = (mode_name_len + 3 + 2) * 6;
                display_draw_string(i2c, s_x, 0, "s");
            }
        } else if (state->is_paused) {
            display_draw_string(i2c, 0, 0, "Edit Time");
        } else {
            display_draw_string(i2c, 0, 0, "Starting Time");
        }

        // draw colons
        uint8_t colon_data[] = {0x40, 0x00, 0x66, 0x66, 0x00, 0x00};
        display_set_position(i2c, 40, start_page + 1);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, colon_data, 6, 100);
        display_set_position(i2c, 82, start_page + 1);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, colon_data, 6, 100);

        // draw footer
        display_draw_string(i2c, 10, 7, "Hold to save");

        // draw all non-blinking digits
        if (state->time_editor_field != TIME_FIELD_HOURS) {
            display_draw_large_character(i2c, 6, start_page, '0' + (state->edit_hours / 10));
            display_draw_large_character(i2c, 22, start_page, '0' + (state->edit_hours % 10));
        }
        if (state->time_editor_field != TIME_FIELD_MINUTES) {
            display_draw_large_character(i2c, 48, start_page, '0' + (state->edit_minutes / 10));
            display_draw_large_character(i2c, 64, start_page, '0' + (state->edit_minutes % 10));
        }
        if (state->time_editor_field != TIME_FIELD_SECONDS) {
            display_draw_large_character(i2c, 90, start_page, '0' + (state->edit_seconds / 10));
            display_draw_large_character(i2c, 106, start_page, '0' + (state->edit_seconds % 10));
        }

        // draw non-blinking bonus display
        if (state->current_bonus_to_edit != NULL && is_countdown_mode &&
            state->time_editor_field != TIME_FIELD_BONUS) {
            char bonus_str[8];
            snprintf(bonus_str, sizeof(bonus_str), "%02ds", state->edit_bonus_seconds);
            uint8_t bx = 104;
            for (uint8_t i = 0; bonus_str[i] != '\0'; i++) {
                display_draw_medium_character(i2c, bx, 0, bonus_str[i]);
                bx += 8;
            }
        } else if (state->current_bonus_to_edit != NULL && is_countdown_mode &&
                   state->time_editor_field == TIME_FIELD_BONUS) {
            uint8_t num_digits = (state->edit_bonus_seconds >= 10) ? 2 : 1;
            uint8_t s_x = 104 + (num_digits * 8);
            display_draw_medium_character(i2c, s_x, 0, 's');
        }

        // update cache
        cache->edit_hours = state->edit_hours;
        cache->edit_minutes = state->edit_minutes;
        cache->edit_seconds = state->edit_seconds;
        cache->edit_bonus_seconds = state->edit_bonus_seconds;
        cache->time_editor_field = state->time_editor_field;
        cache->needs_full_redraw = FALSE;
        
        blink_state_changed = TRUE;  // force initial blink draw
    }

    // check if field changed (user pressed encoder to move to next field)
    uint8_t field_changed = (cache->time_editor_field != state->time_editor_field);
    
    if (field_changed) {
        // redraw old field (now static)
        switch (cache->time_editor_field) {
            case TIME_FIELD_HOURS:
                clear_display_area(i2c, 6, 32, start_page, start_page + 2);
                display_draw_large_character(i2c, 6, start_page, '0' + (cache->edit_hours / 10));
                display_draw_large_character(i2c, 22, start_page, '0' + (cache->edit_hours % 10));
                break;
            case TIME_FIELD_MINUTES:
                clear_display_area(i2c, 48, 32, start_page, start_page + 2);
                display_draw_large_character(i2c, 48, start_page, '0' + (cache->edit_minutes / 10));
                display_draw_large_character(i2c, 64, start_page, '0' + (cache->edit_minutes % 10));
                break;
            case TIME_FIELD_SECONDS:
                clear_display_area(i2c, 90, 32, start_page, start_page + 2);
                display_draw_large_character(i2c, 90, start_page, '0' + (cache->edit_seconds / 10));
                display_draw_large_character(i2c, 106, start_page, '0' + (cache->edit_seconds % 10));
                break;
            case TIME_FIELD_BONUS:
                // redraw bonus as static
                if (state->current_bonus_to_edit != NULL && is_countdown_mode) {
                    char bonus_str[8];
                    snprintf(bonus_str, sizeof(bonus_str), "%02ds", cache->edit_bonus_seconds);
                    uint8_t bx = 104;
                    for (uint8_t i = 0; bonus_str[i] != '\0'; i++) {
                        display_draw_medium_character(i2c, bx, 0, bonus_str[i]);
                        bx += 8;
                    }
                } else if (state->current_bonus_to_edit != NULL && is_increment_mode) {
                    uint8_t mode_name_len = strlen(mode_names[mode]);
                    uint8_t value_x = (mode_name_len + 3) * 6;
                    char val_str[8];
                    snprintf(val_str, sizeof(val_str), "%02ds", cache->edit_bonus_seconds);
                    display_draw_string(i2c, value_x, 0, val_str);
                }
                break;
        }
        
        // draw static 's' for new bonus field if needed
        if (state->time_editor_field == TIME_FIELD_BONUS && state->current_bonus_to_edit != NULL) {
            if (is_countdown_mode) {
                uint8_t num_digits = (state->edit_bonus_seconds >= 10) ? 2 : 1;
                uint8_t s_x = 104 + (num_digits * 8);
                display_draw_medium_character(i2c, s_x, 0, 's');
            } else if (is_increment_mode) {
                uint8_t mode_name_len = strlen(mode_names[mode]);
                uint8_t s_x = (mode_name_len + 3 + 2) * 6;
                display_draw_string(i2c, s_x, 0, "s");
            }
        }
        
        cache->time_editor_field = state->time_editor_field;
        blink_state_changed = TRUE;  // force blink redraw
    }

    // check if values changed (encoder adjustment)
    uint8_t hours_changed = (cache->edit_hours != state->edit_hours);
    uint8_t minutes_changed = (cache->edit_minutes != state->edit_minutes);
    uint8_t seconds_changed = (cache->edit_seconds != state->edit_seconds);
    uint8_t bonus_changed = (cache->edit_bonus_seconds != state->edit_bonus_seconds);

    // redraw changed static (non-blinking) fields
    if (hours_changed && state->time_editor_field != TIME_FIELD_HOURS) {
        clear_display_area(i2c, 6, 32, start_page, start_page + 2);
        display_draw_large_character(i2c, 6, start_page, '0' + (state->edit_hours / 10));
        display_draw_large_character(i2c, 22, start_page, '0' + (state->edit_hours % 10));
        cache->edit_hours = state->edit_hours;
    }
    
    if (minutes_changed && state->time_editor_field != TIME_FIELD_MINUTES) {
        clear_display_area(i2c, 48, 32, start_page, start_page + 2);
        display_draw_large_character(i2c, 48, start_page, '0' + (state->edit_minutes / 10));
        display_draw_large_character(i2c, 64, start_page, '0' + (state->edit_minutes % 10));
        cache->edit_minutes = state->edit_minutes;
    }
    
    if (seconds_changed && state->time_editor_field != TIME_FIELD_SECONDS) {
        clear_display_area(i2c, 90, 32, start_page, start_page + 2);
        display_draw_large_character(i2c, 90, start_page, '0' + (state->edit_seconds / 10));
        display_draw_large_character(i2c, 106, start_page, '0' + (state->edit_seconds % 10));
        cache->edit_seconds = state->edit_seconds;
    }
    
    if (bonus_changed && state->time_editor_field != TIME_FIELD_BONUS) {
        if (state->current_bonus_to_edit != NULL && is_countdown_mode) {
            clear_display_area(i2c, 104, 24, 0, 1);
            char bonus_str[8];
            snprintf(bonus_str, sizeof(bonus_str), "%02ds", state->edit_bonus_seconds);
            uint8_t bx = 104;
            for (uint8_t i = 0; bonus_str[i] != '\0'; i++) {
                display_draw_medium_character(i2c, bx, 0, bonus_str[i]);
                bx += 8;
            }
        } else if (state->current_bonus_to_edit != NULL && is_increment_mode) {
            uint8_t mode_name_len = strlen(mode_names[mode]);
            uint8_t value_x = (mode_name_len + 3) * 6;
            clear_display_area(i2c, value_x, 24, 0, 0);
            char val_str[8];
            snprintf(val_str, sizeof(val_str), "%02ds", state->edit_bonus_seconds);
            display_draw_string(i2c, value_x, 0, val_str);
        }
        cache->edit_bonus_seconds = state->edit_bonus_seconds;
    }

    // force blink update if value of currently blinking field changed
    if ((state->time_editor_field == TIME_FIELD_HOURS && hours_changed) ||
        (state->time_editor_field == TIME_FIELD_MINUTES && minutes_changed) ||
        (state->time_editor_field == TIME_FIELD_SECONDS && seconds_changed) ||
        (state->time_editor_field == TIME_FIELD_BONUS && bonus_changed)) {
        blink_state_changed = TRUE;
    }

    // handle blinking of current field
    if (blink_state_changed) {
        switch (state->time_editor_field) {
            case TIME_FIELD_HOURS:
                clear_display_area(i2c, 6, 32, start_page, start_page + 2);
                if (blink_visible) {
                    display_draw_large_character(i2c, 6, start_page, '0' + (state->edit_hours / 10));
                    display_draw_large_character(i2c, 22, start_page, '0' + (state->edit_hours % 10));
                }
                cache->edit_hours = state->edit_hours;
                break;

            case TIME_FIELD_MINUTES:
                clear_display_area(i2c, 48, 32, start_page, start_page + 2);
                if (blink_visible) {
                    display_draw_large_character(i2c, 48, start_page, '0' + (state->edit_minutes / 10));
                    display_draw_large_character(i2c, 64, start_page, '0' + (state->edit_minutes % 10));
                }
                cache->edit_minutes = state->edit_minutes;
                break;

            case TIME_FIELD_SECONDS:
                clear_display_area(i2c, 90, 32, start_page, start_page + 2);
                if (blink_visible) {
                    display_draw_large_character(i2c, 90, start_page, '0' + (state->edit_seconds / 10));
                    display_draw_large_character(i2c, 106, start_page, '0' + (state->edit_seconds % 10));
                }
                cache->edit_seconds = state->edit_seconds;
                break;

            case TIME_FIELD_BONUS:
                if (state->current_bonus_to_edit != NULL) {
                    if (is_countdown_mode) {
                        // always 2 digits zero-padded, fixed 16px wide
                        clear_display_area(i2c, 104, 16, 0, 1);
                        
                        if (blink_visible) {
                            char digits_str[4];
                            snprintf(digits_str, sizeof(digits_str), "%02d", state->edit_bonus_seconds);
                            display_draw_medium_character(i2c, 104, 0, digits_str[0]);
                            display_draw_medium_character(i2c, 112, 0, digits_str[1]);
                        }
                    } else if (is_increment_mode) {
                        uint8_t mode_name_len = strlen(mode_names[mode]);
                        uint8_t value_x = (mode_name_len + 3) * 6;
                        clear_display_area(i2c, value_x, 12, 0, 0);
                        
                        if (blink_visible) {
                            char val_str[4];
                            snprintf(val_str, sizeof(val_str), "%02d", state->edit_bonus_seconds);
                            display_draw_string(i2c, value_x, 0, val_str);
                        }
                    }
                }
                cache->edit_bonus_seconds = state->edit_bonus_seconds;
                break;
        }

        cache->last_blink_visible = blink_visible;
    }
}

// <---- internal helper: draw mode list screen ---->

static void draw_mode_list(menu_state_t* state, I2C_HandleTypeDef* i2c, menu_display_cache_t* cache) {
    uint32_t blink_phase = (HAL_GetTick() / MENU_BLINK_INTERVAL_MS) % 2;
    uint8_t blink_visible = (blink_phase == 0) ? TRUE : FALSE;
    uint8_t blink_state_changed = (blink_visible != cache->last_blink_visible);

    // full redraw: draw everything
    if (cache->needs_full_redraw) {
        display_clear(i2c);

        // draw header
        display_draw_string(i2c, 0, 0, "Edit Time Control");

        // draw all mode names with cursor and checkmark
        for (uint8_t i = 0; i < TIME_CONTROL_MODE_COUNT; i++) {
            uint8_t page = i + 1;

            // draw cursor for highlighted item
            if (i == state->mode_list_cursor) {
                display_draw_string(i2c, 0, page, ">");
            }

            // draw checkmark for active mode
            if (i == state->config->time_control_mode) {
                display_draw_string(i2c, 8, page, "*");
            }

            // draw mode name
            display_draw_string(i2c, 16, page, mode_names[i]);

            // draw bonus value for non-none modes
            if (i != TIME_CONTROL_NONE) {
                uint8_t is_editing_this = (state->mode_editing && i == state->mode_list_cursor);
                
                if (!is_editing_this) {
                    // not editing: draw complete value
                    uint32_t bonus_seconds = state->config->bonus_time_ms[i] / 1000;
                    char value_str[12];
                    snprintf(value_str, sizeof(value_str), "[%02lus]", bonus_seconds);
                    display_draw_string(i2c, 98, page, value_str);
                } else {
                    // editing: draw static brackets and 's', digits will blink
                    display_draw_string(i2c, 98, page, "[");
                    display_draw_string(i2c, 116, page, "s]");
                }
            }
        }

        // update cache
        cache->mode_list_cursor = state->mode_list_cursor;
        cache->mode_editing = state->mode_editing;
        cache->active_mode = state->config->time_control_mode;
        for (uint8_t i = 0; i < TIME_CONTROL_MODE_COUNT; i++) {
            cache->bonus_values[i] = state->config->bonus_time_ms[i];
        }
        cache->needs_full_redraw = FALSE;
        
        if (state->mode_editing) {
            blink_state_changed = TRUE;  // force initial blink
        }
    }

    // check if cursor moved
    if (cache->mode_list_cursor != state->mode_list_cursor) {
        uint8_t old_page = cache->mode_list_cursor + 1;
        uint8_t new_page = state->mode_list_cursor + 1;
        
        // clear old cursor
        clear_display_area(i2c, 0, 6, old_page, old_page);
        
        // draw new cursor
        display_draw_string(i2c, 0, new_page, ">");
        
        cache->mode_list_cursor = state->mode_list_cursor;
    }

    // check if active mode (checkmark) changed
    if (cache->active_mode != state->config->time_control_mode) {
        uint8_t old_page = cache->active_mode + 1;
        uint8_t new_page = state->config->time_control_mode + 1;
        
        // clear old checkmark
        clear_display_area(i2c, 8, 6, old_page, old_page);
        
        // draw new checkmark
        display_draw_string(i2c, 8, new_page, "*");
        
        cache->active_mode = state->config->time_control_mode;
    }

    // check if mode editing state changed
    if (cache->mode_editing != state->mode_editing) {
        uint8_t page = state->mode_list_cursor + 1;
        
        if (state->mode_editing) {
            // entered editing: redraw value area with brackets and static 's'
            clear_display_area(i2c, 98, 30, page, page);
            display_draw_string(i2c, 98, page, "[");
            display_draw_string(i2c, 116, page, "s]");
            blink_state_changed = TRUE;
        } else {
            // exited editing: redraw complete static value
            clear_display_area(i2c, 98, 30, page, page);
            uint32_t bonus_seconds = state->config->bonus_time_ms[state->mode_list_cursor] / 1000;
            char value_str[12];
            snprintf(value_str, sizeof(value_str), "[%02lus]", bonus_seconds);
            display_draw_string(i2c, 98, page, value_str);
        }
        
        cache->mode_editing = state->mode_editing;
    }

    // check if any bonus values changed (when not editing)
    for (uint8_t i = 0; i < TIME_CONTROL_MODE_COUNT; i++) {
        if (i == TIME_CONTROL_NONE) continue;
        
        // skip currently editing mode (handled by blink)
        if (state->mode_editing && i == state->mode_list_cursor) continue;
        
        if (cache->bonus_values[i] != state->config->bonus_time_ms[i]) {
            uint8_t page = i + 1;
            clear_display_area(i2c, 98, 30, page, page);
            
            uint32_t bonus_seconds = state->config->bonus_time_ms[i] / 1000;
            char value_str[12];
            snprintf(value_str, sizeof(value_str), "[%02lus]", bonus_seconds);
            display_draw_string(i2c, 98, page, value_str);
            
            cache->bonus_values[i] = state->config->bonus_time_ms[i];
        }
    }

    // if editing currently selected mode, check if value changed
    if (state->mode_editing) {
        uint8_t cursor_idx = state->mode_list_cursor;
        if (cache->bonus_values[cursor_idx] != state->config->bonus_time_ms[cursor_idx]) {
            blink_state_changed = TRUE;
            cache->bonus_values[cursor_idx] = state->config->bonus_time_ms[cursor_idx];
        }
    }

    // handle blinking of edited value
    if (state->mode_editing && blink_state_changed) {
        uint8_t page = state->mode_list_cursor + 1;
        
        // clear only digit area
        clear_display_area(i2c, 104, 12, page, page);
        
        if (blink_visible) {
            // draw only digits
            uint32_t bonus_seconds = state->config->bonus_time_ms[state->mode_list_cursor] / 1000;
            char digits_str[12];
            snprintf(digits_str, sizeof(digits_str), "%02lu", bonus_seconds);
            display_draw_string(i2c, 104, page, digits_str);
        }

        cache->last_blink_visible = blink_visible;
    }
}

// <---- internal helper: draw reset confirmation screen ---->

static void draw_reset_confirm(menu_state_t* state, I2C_HandleTypeDef* i2c, menu_display_cache_t* cache) {
    // full redraw
    if (cache->needs_full_redraw) {
        display_clear(i2c);

        display_draw_string(i2c, 30, 2, "Reset game?");

        // draw yes/no options
        if (state->reset_confirm_cursor == 0) {
            display_draw_string(i2c, 30, 4, "> Yes");
            display_draw_string(i2c, 30, 5, "  No");
        } else {
            display_draw_string(i2c, 30, 4, "  Yes");
            display_draw_string(i2c, 30, 5, "> No");
        }

        cache->reset_confirm_cursor = state->reset_confirm_cursor;
        cache->needs_full_redraw = FALSE;
        return;
    }

    // partial redraw: only cursor moved
    if (cache->reset_confirm_cursor != state->reset_confirm_cursor) {
        uint8_t old_page = cache->reset_confirm_cursor + 4;
        uint8_t new_page = state->reset_confirm_cursor + 4;
        
        // clear old cursor
        clear_display_area(i2c, 30, 6, old_page, old_page);
        
        // draw text without cursor at old position
        if (cache->reset_confirm_cursor == 0) {
            display_draw_string(i2c, 30, old_page, "  Yes");
        } else {
            display_draw_string(i2c, 30, old_page, "  No");
        }
        
        // draw new cursor
        if (state->reset_confirm_cursor == 0) {
            clear_display_area(i2c, 30, 30, new_page, new_page);
            display_draw_string(i2c, 30, new_page, "> Yes");
        } else {
            clear_display_area(i2c, 30, 30, new_page, new_page);
            display_draw_string(i2c, 30, new_page, "> No");
        }
        
        cache->reset_confirm_cursor = state->reset_confirm_cursor;
    }
}

// <---- internal helper: draw save feedback screen ---->

static void draw_save_feedback(I2C_HandleTypeDef* i2c) {
    display_clear(i2c);
    // "Saved!" centered on screen
    display_draw_string(i2c, 40, 3, "Saved!");
}

// <---- internal helper: handle main menu input ---->

static menu_action_t handle_main_menu(menu_state_t* state, uint8_t player_index) {
    // encoder rotates through menu items
    int8_t delta = encoder_get_clicks(player_encoders[player_index]);
    if (delta != 0) {
        int8_t new_cursor = (int8_t)state->main_menu_cursor + delta;
        
        // wrap around
        while (new_cursor < 0) new_cursor += state->item_count;
        state->main_menu_cursor = (uint8_t)(new_cursor % state->item_count);
    }

    // encoder push selects item
    if (button_was_pressed(player_encoder_push[player_index])) {
        if (state->is_paused) {
            // paused menu
            switch (state->main_menu_cursor) {
                case PAUSED_ITEM_CURRENT_TIME:
                    // enter time editor for current time
                    ms_to_hms(*state->current_time_to_edit, &state->edit_hours, &state->edit_minutes, &state->edit_seconds);
                    if (state->current_bonus_to_edit != NULL) {
                        state->edit_bonus_seconds = *state->current_bonus_to_edit / 1000;
                    }
                    state->time_editor_field = TIME_FIELD_HOURS;
                    state->current_screen = MENU_SCREEN_TIME_EDITOR;
                    menu_cache[player_index].needs_full_redraw = TRUE;
                    break;

                case PAUSED_ITEM_TIME_CONTROL:
                    // enter mode list
                    state->mode_list_cursor = (uint8_t)state->config->time_control_mode;
                    state->mode_editing = FALSE;
                    state->current_screen = MENU_SCREEN_MODE_LIST;
                    menu_cache[player_index].needs_full_redraw = TRUE;
                    break;

                case PAUSED_ITEM_RESET:
                    // enter reset confirmation
                    state->reset_confirm_cursor = 1;  // default to "No"
                    state->current_screen = MENU_SCREEN_RESET_CONFIRM;
                    menu_cache[player_index].needs_full_redraw = TRUE;
                    break;

                case PAUSED_ITEM_READY:
                    // exit menu and resume
                    return MENU_ACTION_READY;

                default:
                    break;
            }
        } else {
            // armed menu
            switch (state->main_menu_cursor) {
                case ARMED_ITEM_STARTING_TIME:
                    // enter time editor for starting time
                    ms_to_hms(state->config->starting_time_ms, &state->edit_hours, &state->edit_minutes, &state->edit_seconds);
                    state->time_editor_field = TIME_FIELD_HOURS;
                    state->current_screen = MENU_SCREEN_TIME_EDITOR;
                    menu_cache[player_index].needs_full_redraw = TRUE;
                    break;

                case ARMED_ITEM_TIME_CONTROL:
                    // enter mode list
                    state->mode_list_cursor = (uint8_t)state->config->time_control_mode;
                    state->mode_editing = FALSE;
                    state->current_screen = MENU_SCREEN_MODE_LIST;
                    menu_cache[player_index].needs_full_redraw = TRUE;
                    break;

                case ARMED_ITEM_READY:
                    // exit menu
                    return MENU_ACTION_READY;

                default:
                    break;
            }
        }
    }

    // flush back button presses at main menu level (not used here, prevent stale input)
    button_clear_flags(player_back[player_index]);

    return MENU_ACTION_NONE;
}

// <---- internal helper: handle time editor input ---->

static void handle_time_editor(menu_state_t* state, uint8_t player_index) {
    // encoder adjusts current field value
    int8_t delta = encoder_get_clicks(player_encoders[player_index]);
    if (delta != 0) {
        switch (state->time_editor_field) {
            case TIME_FIELD_HOURS:
                if (delta > 0) {
                    state->edit_hours = (state->edit_hours == TIME_EDITOR_HOURS_MAX) ? 0 : state->edit_hours + 1;
                } else if (delta < 0) {
                    state->edit_hours = (state->edit_hours == 0) ? TIME_EDITOR_HOURS_MAX : state->edit_hours - 1;
                }
                break;
            case TIME_FIELD_MINUTES:
                if (delta > 0) {
                    state->edit_minutes = (state->edit_minutes == TIME_EDITOR_MINUTES_MAX) ? 0 : state->edit_minutes + 1;
                } else if (delta < 0) {
                    state->edit_minutes = (state->edit_minutes == 0) ? TIME_EDITOR_MINUTES_MAX : state->edit_minutes - 1;
                }
                break;
            case TIME_FIELD_SECONDS:
                if (delta > 0) {
                    state->edit_seconds = (state->edit_seconds == TIME_EDITOR_SECONDS_MAX) ? 0 : state->edit_seconds + 1;
                } else if (delta < 0) {
                    state->edit_seconds = (state->edit_seconds == 0) ? TIME_EDITOR_SECONDS_MAX : state->edit_seconds - 1;
                }
                break;
            case TIME_FIELD_BONUS:
                if (delta > 0) {
                    state->edit_bonus_seconds = (state->edit_bonus_seconds == BONUS_TIME_EDITOR_SECONDS_MAX) ? 0 : state->edit_bonus_seconds + 1;
                } else if (delta < 0) {
                    state->edit_bonus_seconds = (state->edit_bonus_seconds == 0) ? BONUS_TIME_EDITOR_SECONDS_MAX : state->edit_bonus_seconds - 1;
                }
                break;
            default:
                break;
        }
    }

    // encoder push moves to next field
    if (button_was_pressed(player_encoder_push[player_index])) {
        uint8_t max_field = state->current_bonus_to_edit ? TIME_FIELD_COUNT_PAUSED : TIME_FIELD_COUNT_ARMED;
        state->time_editor_field = (state->time_editor_field + 1) % max_field;
    }

    // encoder long press saves and returns to main menu
    if (button_is_held(player_encoder_push[player_index], ENCODER_LONG_PRESS_TIME_MS)) {
        if (!encoder_push_held[player_index]) {
            encoder_push_held[player_index] = TRUE;

            // save edited values
            uint32_t new_time_ms = hms_to_ms(state->edit_hours, state->edit_minutes, state->edit_seconds);
            
            if (state->current_time_to_edit != NULL) {
                // editing current time (paused)
                *state->current_time_to_edit = new_time_ms;
                
                // save bonus time if applicable
                if (state->current_bonus_to_edit != NULL) {
                    *state->current_bonus_to_edit = (uint32_t)state->edit_bonus_seconds * 1000;
                }
            } else {
                // editing starting time (armed)
                state->config->starting_time_ms = new_time_ms;
            }

            // show save feedback
            state->current_screen = MENU_SCREEN_SAVE_FEEDBACK;
            state->save_feedback_timestamp = HAL_GetTick();
            menu_cache[player_index].needs_full_redraw = TRUE;
        }
    } else {
        encoder_push_held[player_index] = FALSE;
    }

    // back button cancels and returns to main menu
    if (button_was_pressed(player_back[player_index])) {
        state->current_screen = MENU_SCREEN_MAIN;
        menu_cache[player_index].needs_full_redraw = TRUE;
    }
}

// <---- internal helper: handle mode list input ---->

static void handle_mode_list(menu_state_t* state, uint8_t player_index) {
    int8_t delta = encoder_get_clicks(player_encoders[player_index]);

    if (state->mode_editing) {
        // encoder adjusts bonus value of highlighted mode
        if (delta != 0) {
            uint32_t current_seconds = state->config->bonus_time_ms[state->mode_list_cursor] / 1000;
            if (delta > 0) {
                if (current_seconds == BONUS_TIME_EDITOR_SECONDS_MAX) {
                    current_seconds = 0;
                } else {
                    current_seconds++;
                }
            } else if (delta < 0) {
                if (current_seconds == 0) {
                    current_seconds = BONUS_TIME_EDITOR_SECONDS_MAX;
                } else {
                    current_seconds--;
                }
            }
            state->config->bonus_time_ms[state->mode_list_cursor] = (uint32_t)current_seconds * 1000;
        }

        // encoder push confirms: move checkmark, stop editing
        if (button_was_pressed(player_encoder_push[player_index])) {
            state->config->time_control_mode = (time_control_mode_t)state->mode_list_cursor;
            state->mode_editing = FALSE;
        }

        // back cancels editing without moving checkmark
        if (button_was_pressed(player_back[player_index])) {
            state->mode_editing = FALSE;
        }
    } else {
        // encoder scrolls through modes
        if (delta != 0) {
            int8_t new_cursor = (int8_t)state->mode_list_cursor + delta;
            while (new_cursor < 0) new_cursor += TIME_CONTROL_MODE_COUNT;
            state->mode_list_cursor = (uint8_t)(new_cursor % TIME_CONTROL_MODE_COUNT);
        }

        // encoder push selects mode
        if (button_was_pressed(player_encoder_push[player_index])) {
            if (state->mode_list_cursor == TIME_CONTROL_NONE) {
                // none: just move checkmark, no value to edit
                state->config->time_control_mode = TIME_CONTROL_NONE;
            } else {
                // start editing this mode's bonus value inline
                state->mode_editing = TRUE;
            }
        }

        // back returns to main menu
        if (button_was_pressed(player_back[player_index])) {
            state->current_screen = MENU_SCREEN_MAIN;
            menu_cache[player_index].needs_full_redraw = TRUE;
        }
    }
}

// <---- internal helper: handle reset confirmation input ---->

static menu_action_t handle_reset_confirm(menu_state_t* state, uint8_t player_index) {
    // encoder switches between yes/no
    int8_t delta = encoder_get_clicks(player_encoders[player_index]);
    if (delta != 0) {
        state->reset_confirm_cursor = (state->reset_confirm_cursor == 0) ? 1 : 0;
    }

    // encoder push confirms selection
    if (button_was_pressed(player_encoder_push[player_index])) {
        if (state->reset_confirm_cursor == 0) {
            // yes: reset
            return MENU_ACTION_RESET;
        } else {
            // no: cancel, return to main menu
            state->current_screen = MENU_SCREEN_MAIN;
            menu_cache[player_index].needs_full_redraw = TRUE;
        }
    }

    // back button cancels
    if (button_was_pressed(player_back[player_index])) {
        state->current_screen = MENU_SCREEN_MAIN;
        menu_cache[player_index].needs_full_redraw = TRUE;
    }

    return MENU_ACTION_NONE;
}

// <---- internal helper: handle save feedback timeout ---->

static void handle_save_feedback(menu_state_t* state, uint8_t player_index) {
    uint32_t elapsed = HAL_GetTick() - state->save_feedback_timestamp;
    if (elapsed >= MENU_SAVE_FEEDBACK_DURATION_MS) {
        // auto-return to main menu
        state->current_screen = MENU_SCREEN_MAIN;
        menu_cache[player_index].needs_full_redraw = TRUE;
    }
}

// <---- initialization ---->

void menu_init(void) {
    // reset both player menu states
    for (uint8_t i = 0; i < 2; i++) {
        memset(&menu_states[i], 0, sizeof(menu_state_t));
        memset(&menu_cache[i], 0, sizeof(menu_display_cache_t));
        menu_open_flags[i] = FALSE;
        encoder_push_held[i] = FALSE;
    }
}

// <---- menu open ---->

void menu_open(uint8_t player_index,
               player_config_t* config,
               uint32_t* current_time,
               uint32_t* current_bonus,
               uint8_t is_paused) {

    if (player_index > 1) return;

    menu_state_t* state = &menu_states[player_index];
    menu_display_cache_t* cache = &menu_cache[player_index];

    // store pointers to editable values
    state->config = config;
    state->current_time_to_edit = current_time;
    state->current_bonus_to_edit = current_bonus;
    state->is_paused = is_paused;

    // set up menu variant
    state->item_count = is_paused ? MENU_PAUSED_ITEM_COUNT : MENU_ARMED_ITEM_COUNT;

    // reset navigation to top
    state->current_screen = MENU_SCREEN_MAIN;
    state->main_menu_cursor = 0;
    state->mode_list_cursor = (uint8_t)config->time_control_mode;
    state->mode_editing = FALSE;
    state->time_editor_field = TIME_FIELD_HOURS;
    state->reset_confirm_cursor = 1;

    // reset cache and force full redraw
    cache->needs_full_redraw = TRUE;
    cache->last_blink_visible = FALSE;

    // flush stale button presses to prevent immediate action on menu open
    button_clear_flags(player_encoder_push[player_index]);
    button_clear_flags(player_back[player_index]);

    // mark menu as open
    menu_open_flags[player_index] = TRUE;
}

// <---- menu update ---->

menu_action_t menu_update(uint8_t player_index) {
    if (player_index > 1) return MENU_ACTION_NONE;
    if (!menu_open_flags[player_index]) return MENU_ACTION_NONE;

    menu_state_t* state = &menu_states[player_index];
    menu_display_cache_t* cache = &menu_cache[player_index];
    I2C_HandleTypeDef* i2c = get_player_i2c(player_index);
    menu_action_t action = MENU_ACTION_NONE;

    // handle input based on current screen
    switch (state->current_screen) {
        case MENU_SCREEN_MAIN:
            action = handle_main_menu(state, player_index);
            break;

        case MENU_SCREEN_TIME_EDITOR:
            handle_time_editor(state, player_index);
            break;

        case MENU_SCREEN_MODE_LIST:
            handle_mode_list(state, player_index);
            break;

        case MENU_SCREEN_RESET_CONFIRM:
            action = handle_reset_confirm(state, player_index);
            break;

        case MENU_SCREEN_SAVE_FEEDBACK:
            handle_save_feedback(state, player_index);
            break;

        default:
            break;
    }

    // if action is ready or reset, close the menu
    if (action == MENU_ACTION_READY || action == MENU_ACTION_RESET) {
        menu_open_flags[player_index] = FALSE;
        return action;
    }

    // draw current screen (uses cache internally to minimize redraws)
    switch (state->current_screen) {
        case MENU_SCREEN_MAIN:
            draw_main_menu(state, i2c, cache);
            break;

        case MENU_SCREEN_TIME_EDITOR:
            draw_time_editor(state, i2c, cache);
            break;

        case MENU_SCREEN_MODE_LIST:
            draw_mode_list(state, i2c, cache);
            break;

        case MENU_SCREEN_RESET_CONFIRM:
            draw_reset_confirm(state, i2c, cache);
            break;

        case MENU_SCREEN_SAVE_FEEDBACK:
            if (cache->needs_full_redraw) {
                draw_save_feedback(i2c);
                cache->needs_full_redraw = FALSE;
            }
            break;

        default:
            break;
    }

    return MENU_ACTION_NONE;
}

// <---- menu state query ---->

uint8_t menu_is_open(uint8_t player_index) {
    if (player_index > 1) return FALSE;
    return menu_open_flags[player_index];
}