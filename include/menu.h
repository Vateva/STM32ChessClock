/*
 * menu module
 * handles menu navigation, time editing, and mode selection
 * independent from game module — communicates through main.c
 */

#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "config.h"

// <---- menu screens ---->

typedef enum {
    MENU_SCREEN_MAIN,               // main list (starting time / time control / ready / [reset])
    MENU_SCREEN_TIME_EDITOR,        // editing time fields with blinking digits
    MENU_SCREEN_MODE_LIST,          // browsing and editing time control modes
    MENU_SCREEN_RESET_CONFIRM,      // yes/no confirmation for reset
    MENU_SCREEN_SAVE_FEEDBACK       // brief "Saved!" display before returning
} menu_screen_t;

// <---- menu actions (returned to main.c to relay to game module) ---->

typedef enum {
    MENU_ACTION_NONE,               // no action, menu still active
    MENU_ACTION_READY,              // player selected "Ready!", exit menu
    MENU_ACTION_RESET               // player confirmed "Reset", exit menu
} menu_action_t;

// <---- main menu item indices ---->

// items differ depending on whether game is paused or not
// armed menu:  Starting Time, Time Control, Ready
// paused menu: Current Time, Time Control, Reset, Ready

#define MENU_ARMED_ITEM_COUNT       3
#define MENU_PAUSED_ITEM_COUNT      4

typedef enum {
    // armed menu items
    ARMED_ITEM_STARTING_TIME = 0,
    ARMED_ITEM_TIME_CONTROL = 1,
    ARMED_ITEM_READY = 2,

    // paused menu items
    PAUSED_ITEM_CURRENT_TIME = 0,
    PAUSED_ITEM_TIME_CONTROL = 1,
    PAUSED_ITEM_RESET = 2,
    PAUSED_ITEM_READY = 3
} menu_item_t;

// <---- time editor field indices ---->

typedef enum {
    TIME_FIELD_HOURS = 0,
    TIME_FIELD_MINUTES = 1,
    TIME_FIELD_SECONDS = 2,
    TIME_FIELD_BONUS = 3,           // only present when editing from paused
    TIME_FIELD_COUNT_ARMED = 3,     // HH, MM, SS
    TIME_FIELD_COUNT_PAUSED = 4     // HH, MM, SS, bonus
} time_field_t;

// <---- per-player menu state ---->

typedef struct {
    // current screen and navigation
    menu_screen_t current_screen;       // which screen is active
    uint8_t main_menu_cursor;           // highlighted item in main menu list
    uint8_t mode_list_cursor;           // highlighted mode in mode list
    uint8_t mode_editing;               // flag: currently editing highlighted mode's bonus value
    uint8_t time_editor_field;          // which field is blinking (time_field_t)
    uint8_t reset_confirm_cursor;       // 0 = yes, 1 = no

    // pointers to values being edited (set on menu_open)
    player_config_t* config;            // player's config (mode, bonus, starting time)
    uint32_t* current_time_to_edit;     // live clock time (NULL if from armed)
    uint32_t* current_bonus_to_edit;    // live bonus time (NULL if from armed)

    // time editor working values (decomposed from milliseconds)
    uint8_t edit_hours;                 // 0-23
    uint8_t edit_minutes;              // 0-59
    uint8_t edit_seconds;              // 0-59
    uint8_t edit_bonus_seconds;         // 0-59

    // context
    uint8_t is_paused;                  // determines which menu variant to show
    uint8_t item_count;                 // number of items in main menu

    // display state
    uint8_t needs_full_redraw;          // flag to force full screen redraw
    uint32_t save_feedback_timestamp;   // when "Saved!" was shown
} menu_state_t;

// <---- initialization ---->

/**
 * initialize menu module
 * resets menu state for both players
 * call once at startup
 */
void menu_init(void);

// <---- menu open/close ---->

/**
 * open menu for a player
 * sets up pointers to editable values and determines menu variant
 *
 * @param player_index 0 or 1
 * @param config pointer to player's config (starting time, mode, bonus)
 * @param current_time pointer to live clock time (NULL if from armed)
 * @param current_bonus pointer to live bonus time (NULL if from armed)
 * @param is_paused TRUE if game was in progress (shows current time + reset options)
 */
void menu_open(uint8_t player_index,
               player_config_t* config,
               uint32_t* current_time,
               uint32_t* current_bonus,
               uint8_t is_paused);

// <---- menu update ---->

/**
 * update menu for a player
 * handles input (encoder, buttons), updates display
 * returns action for main.c to relay to game module
 *
 * @param player_index 0 or 1
 * @return MENU_ACTION_NONE while menu is active,
 *         MENU_ACTION_READY when player selects ready,
 *         MENU_ACTION_RESET when player confirms reset
 */
menu_action_t menu_update(uint8_t player_index);

// <---- menu state query ---->

/**
 * check if a player's menu is currently open
 *
 * @param player_index 0 or 1
 * @return TRUE if menu is open for this player, FALSE otherwise
 */
uint8_t menu_is_open(uint8_t player_index);

#endif  // MENU_H