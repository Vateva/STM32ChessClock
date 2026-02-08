/*
 * game module implementation
 * manages game state machine, player clocks, time control logic,
 * input handling, and display updates
 */

#include "game.h"
#include "hardware.h"
#include "display.h"
#include "button.h"
#include "config.h"
#include <string.h>

// <---- display cache (tracks what is currently drawn per player) ---->

// used to avoid redrawing unchanged elements
typedef struct {
    uint32_t time_ms;                   // last drawn main time
    uint32_t bonus_ms;                  // last drawn bonus time
    time_control_mode_t mode;           // last drawn mode
    uint8_t is_ready;                   // last drawn ready state
    uint8_t in_menu;                    // last drawn menu state
    uint8_t is_loser;                   // last drawn loser state
    game_phase_t phase;                 // last drawn phase
    uint8_t is_blink_visible;           // last drawn blink state (for finished)
    uint8_t needs_full_redraw;          // flag to force full screen redraw
} display_cache_t;

// <---- static game state ---->

static game_t game;

// <---- display cache for both players ---->

static display_cache_t display_cache[PLAYER_COUNT];

// <---- menu hold edge detection (trigger once per hold) ---->

static uint8_t menu_hold_triggered[PLAYER_COUNT];

// <---- buzzer state for finished beep pattern ---->

typedef enum {
    BUZZER_IDLE,            // not playing
    BUZZER_BEEP_1,          // first beep
    BUZZER_PAUSE_1,         // pause after first beep
    BUZZER_BEEP_2,          // second beep
    BUZZER_PAUSE_2,         // pause after second beep
    BUZZER_BEEP_3,          // third beep
    BUZZER_DONE             // pattern complete
} buzzer_pattern_state_t;

static buzzer_pattern_state_t buzzer_state;
static uint32_t buzzer_timestamp;       // when current buzzer step started

// buzzer pattern timing in milliseconds
#define BUZZER_BEEP_DURATION_MS     150
#define BUZZER_PAUSE_DURATION_MS    100

// <---- finished state blink timing ---->

#define FINISHED_BLINK_INTERVAL_MS  500

// <---- button to player mapping helpers ---->

// maps player id to their button ids for cleaner code
static const button_id_t tap_buttons[PLAYER_COUNT] = {
    BUTTON_PLAYER1_TAP,
    BUTTON_PLAYER2_TAP
};

static const button_id_t menu_buttons[PLAYER_COUNT] = {
    BUTTON_PLAYER1_MENU,
    BUTTON_PLAYER2_MENU
};

// <---- i2c handle lookup per player ---->

static I2C_HandleTypeDef* get_player_i2c(player_id_t player) {
    if (player == PLAYER_1) {
        return hardware_get_i2c1();
    }
    return hardware_get_i2c2();
}

// <---- internal helper: set default config for a player ---->

static void set_default_config(player_config_t* config) {
    config->starting_time_ms = DEFAULT_STARTING_TIME_MS;
    config->time_control_mode = DEFAULT_TIME_CONTROL_MODE;
    config->bonus_time_ms = DEFAULT_BONUS_TIME_MS;
}

// <---- internal helper: load starting config into current times ---->

// copies configured starting values into the live clock fields
static void load_starting_times(player_state_t* player) {
    player->current_time_ms = player->config.starting_time_ms;
    player->current_bonus_ms = player->config.bonus_time_ms;
}

// <---- internal helper: invalidate display cache to force full redraw ---->

static void invalidate_display_cache(player_id_t player) {
    display_cache[player].needs_full_redraw = TRUE;
}

// <---- internal helper: transition to armed state ---->

// resets game to armed with current config, loads starting times
static void transition_to_armed(void) {
    game.phase = GAME_PHASE_ARMED;

    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        load_starting_times(&game.players[i]);
        game.players[i].in_menu = FALSE;
        game.players[i].is_loser = FALSE;
        invalidate_display_cache((player_id_t)i);
    }
}

// <---- internal helper: transition to running state ---->

// starts the game clock, sets active player
static void transition_to_running(player_id_t first_active) {
    game.phase = GAME_PHASE_RUNNING;
    game.active_player = first_active;

    // start hardware timer for 100ms ticks
    hardware_start_clock_timer();

    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        invalidate_display_cache((player_id_t)i);
    }
}

// <---- internal helper: transition to paused state ---->

// freezes clocks, stops hardware timer
static void transition_to_paused(void) {
    game.phase = GAME_PHASE_PAUSED;

    // stop hardware timer so ticks stop
    hardware_stop_clock_timer();

    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        invalidate_display_cache((player_id_t)i);
    }
}

// <---- internal helper: transition to finished state ---->

// marks loser, stops timer, starts buzzer pattern
static void transition_to_finished(player_id_t loser) {
    game.phase = GAME_PHASE_FINISHED;
    game.players[loser].is_loser = TRUE;

    // stop hardware timer
    hardware_stop_clock_timer();

    // start buzzer beep pattern
    buzzer_state = BUZZER_BEEP_1;
    buzzer_timestamp = HAL_GetTick();
    hardware_buzzer_on();

    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        invalidate_display_cache((player_id_t)i);
    }
}

// <---- internal helper: apply time control on tap (player just moved) ---->

// called when the active player taps to pass the turn
// applies bonus/increment rules to the player who just moved
static void apply_time_control_on_tap(player_state_t* player) {
    switch (player->config.time_control_mode) {
        case TIME_CONTROL_NONE:
            // no bonus time to apply
            break;

        case TIME_CONTROL_INCREMENT:
            // fischer: add bonus time, can accumulate beyond starting time
            player->current_time_ms += player->config.bonus_time_ms;
            break;

        case TIME_CONTROL_DELAY:
            // bronstein: refresh the delay buffer to full
            player->current_bonus_ms = player->config.bonus_time_ms;
            break;

        case TIME_CONTROL_PARTIAL:
            // add bonus time but cap at starting time
            player->current_time_ms += player->config.bonus_time_ms;
            if (player->current_time_ms > player->config.starting_time_ms) {
                player->current_time_ms = player->config.starting_time_ms;
            }
            break;

        case TIME_CONTROL_LIMITED:
            // refresh the limited time buffer to full
            player->current_bonus_ms = player->config.bonus_time_ms;
            break;

        case TIME_CONTROL_BYOYOMI:
            // refresh byo-yomi period if main time already expired
            if (player->current_time_ms == 0) {
                player->current_bonus_ms = player->config.bonus_time_ms;
            }
            break;

        default:
            break;
    }
}

// <---- internal helper: enter menu for a player ---->

// handles menu entry from any phase
static void enter_menu(player_id_t player) {
    // if already in menu, nothing to do
    if (game.players[player].in_menu) {
        return;
    }

    switch (game.phase) {
        case GAME_PHASE_ARMED:
            // just set menu flag, no phase change
            game.players[player].in_menu = TRUE;
            break;

        case GAME_PHASE_RUNNING:
            // save current times (already in current_time_ms/current_bonus_ms)
            // transition to paused, then set menu flag
            transition_to_paused();
            game.players[player].in_menu = TRUE;
            break;

        case GAME_PHASE_PAUSED:
            // already paused, just set menu flag
            game.players[player].in_menu = TRUE;
            break;

        case GAME_PHASE_FINISHED:
            // transition to armed with last config, then set menu flag
            transition_to_armed();
            game.players[player].in_menu = TRUE;
            break;

        default:
            break;
    }

    invalidate_display_cache(player);
}

// <---- internal helper: handle input for armed phase ---->

static void handle_input_armed(void) {
    // taps only work when both players are out of menu
    if (game.players[PLAYER_1].in_menu || game.players[PLAYER_2].in_menu) {
        return;
    }

    // player 1 taps: player 2 becomes active
    if (button_was_pressed(BUTTON_PLAYER1_TAP)) {
        // load starting times for a fresh game
        for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
            load_starting_times(&game.players[i]);
        }
        transition_to_running(PLAYER_2);
    }
    // player 2 taps: player 1 becomes active
    else if (button_was_pressed(BUTTON_PLAYER2_TAP)) {
        for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
            load_starting_times(&game.players[i]);
        }
        transition_to_running(PLAYER_1);
    }
}

// <---- internal helper: handle input for running phase ---->

static void handle_input_running(void) {
    // only the active player can tap to switch turns
    player_id_t active = game.active_player;
    player_id_t inactive = (active == PLAYER_1) ? PLAYER_2 : PLAYER_1;

    if (button_was_pressed(tap_buttons[active])) {
        // apply time control bonus to the player who just moved
        apply_time_control_on_tap(&game.players[active]);

        // switch active player
        game.active_player = inactive;

        // invalidate both displays since active/inactive swapped
        invalidate_display_cache(PLAYER_1);
        invalidate_display_cache(PLAYER_2);
    }
}

// <---- internal helper: handle input for paused phase ---->

static void handle_input_paused(void) {
    // taps only work when both players are out of menu
    if (game.players[PLAYER_1].in_menu || game.players[PLAYER_2].in_menu) {
        return;
    }

    // player 1 taps: player 2 becomes active, resume with saved times
    if (button_was_pressed(BUTTON_PLAYER1_TAP)) {
        transition_to_running(PLAYER_2);
    }
    // player 2 taps: player 1 becomes active, resume with saved times
    else if (button_was_pressed(BUTTON_PLAYER2_TAP)) {
        transition_to_running(PLAYER_1);
    }
}

// <---- internal helper: handle input for finished phase ---->

static void handle_input_finished(void) {
    // any tap from either player goes to armed with last config
    if (button_was_pressed(BUTTON_PLAYER1_TAP) || button_was_pressed(BUTTON_PLAYER2_TAP)) {
        transition_to_armed();
    }
}

// <---- internal helper: handle menu hold for both players ---->

// checks menu button hold for both players, triggers menu entry once per hold
static void handle_menu_hold(void) {
    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        if (button_is_held(menu_buttons[i], MENU_BUTTON_HOLD_TIME_MS)) {
            // hold threshold reached, trigger once
            if (!menu_hold_triggered[i]) {
                menu_hold_triggered[i] = TRUE;
                enter_menu((player_id_t)i);
            }
        } else {
            // button released or not held long enough, reset trigger
            menu_hold_triggered[i] = FALSE;
        }
    }
}

// <---- internal helper: update buzzer pattern ---->

// advances the buzzer beep pattern state machine
static void update_buzzer_pattern(void) {
    if (buzzer_state == BUZZER_IDLE || buzzer_state == BUZZER_DONE) {
        return;
    }

    uint32_t elapsed = HAL_GetTick() - buzzer_timestamp;

    switch (buzzer_state) {
        case BUZZER_BEEP_1:
            if (elapsed >= BUZZER_BEEP_DURATION_MS) {
                hardware_buzzer_off();
                buzzer_state = BUZZER_PAUSE_1;
                buzzer_timestamp = HAL_GetTick();
            }
            break;

        case BUZZER_PAUSE_1:
            if (elapsed >= BUZZER_PAUSE_DURATION_MS) {
                hardware_buzzer_on();
                buzzer_state = BUZZER_BEEP_2;
                buzzer_timestamp = HAL_GetTick();
            }
            break;

        case BUZZER_BEEP_2:
            if (elapsed >= BUZZER_BEEP_DURATION_MS) {
                hardware_buzzer_off();
                buzzer_state = BUZZER_PAUSE_2;
                buzzer_timestamp = HAL_GetTick();
            }
            break;

        case BUZZER_PAUSE_2:
            if (elapsed >= BUZZER_PAUSE_DURATION_MS) {
                hardware_buzzer_on();
                buzzer_state = BUZZER_BEEP_3;
                buzzer_timestamp = HAL_GetTick();
            }
            break;

        case BUZZER_BEEP_3:
            if (elapsed >= BUZZER_BEEP_DURATION_MS) {
                hardware_buzzer_off();
                buzzer_state = BUZZER_DONE;
            }
            break;

        default:
            break;
    }
}

// <---- internal helper: update display for one player ---->

// compares current state against cache, redraws only changed elements
// TODO: add per-digit granularity for clock digits
static void update_player_display(player_id_t player) {
    player_state_t* ps = &game.players[player];
    display_cache_t* cache = &display_cache[player];
    I2C_HandleTypeDef* i2c = get_player_i2c(player);

    // if player is in menu, the menu module handles drawing
    // just track the state change for when they exit
    if (ps->in_menu) {
        if (!cache->in_menu) {
            // just entered menu, mark cache so we know to full-redraw on exit
            cache->in_menu = TRUE;
        }
        return;
    }

    // if player just exited menu, force full redraw
    if (cache->in_menu) {
        cache->in_menu = FALSE;
        cache->needs_full_redraw = TRUE;
    }

    // full redraw: clear screen and draw everything
    if (cache->needs_full_redraw) {
        display_clear(i2c);
        cache->needs_full_redraw = FALSE;

        // reset all cached values to force individual element draws below
        cache->time_ms = UINT32_MAX;
        cache->bonus_ms = UINT32_MAX;
        cache->mode = (time_control_mode_t)0xFF;
        cache->is_ready = 0xFF;
        cache->is_loser = 0xFF;
        cache->phase = (game_phase_t)0xFF;
        cache->is_blink_visible = 0xFF;
    }

    // determine what to display based on phase
    uint32_t display_time_ms = ps->current_time_ms;
    uint32_t display_bonus_ms = ps->current_bonus_ms;
    uint8_t display_ready = FALSE;
    uint8_t is_blink_visible = TRUE;    // default: visible (no blink)

    // in byo-yomi, show bonus time on big clock when main time is 0
    if (game.phase == GAME_PHASE_RUNNING &&
        ps->config.time_control_mode == TIME_CONTROL_BYOYOMI &&
        ps->current_time_ms == 0) {
        display_time_ms = ps->current_bonus_ms;
    }

    // ready footer shows in armed and paused (when not in menu)
    if (game.phase == GAME_PHASE_ARMED || game.phase == GAME_PHASE_PAUSED) {
        display_ready = TRUE;
    }

    // in finished state, loser's display blinks
    if (game.phase == GAME_PHASE_FINISHED && ps->is_loser) {
        uint32_t blink_phase = (HAL_GetTick() / FINISHED_BLINK_INTERVAL_MS) % 2;
        is_blink_visible = (blink_phase == 0) ? TRUE : FALSE;
    }

    // update header if mode or bonus time changed
    if (cache->mode != ps->config.time_control_mode ||
        cache->bonus_ms != display_bonus_ms) {

        // clear header area (page 0 and page 1 for medium font)
        display_set_position(i2c, 0, 0);
        uint8_t clear_data[129];
        clear_data[0] = 0x40;
        memset(&clear_data[1], 0x00, 128);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);
        display_set_position(i2c, 0, 1);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);

        display_draw_header(i2c, ps->config.time_control_mode, display_bonus_ms);

        cache->mode = ps->config.time_control_mode;
        cache->bonus_ms = display_bonus_ms;
    }

    // update clock if time changed or blink state changed
    if (cache->time_ms != display_time_ms ||
        cache->is_blink_visible != is_blink_visible) {

        if (is_blink_visible) {
            display_draw_clock(i2c, display_time_ms);
        } else {
            // blink off: clear the clock area (pages 3-5)
            uint8_t clear_data[129];
            clear_data[0] = 0x40;
            memset(&clear_data[1], 0x00, 128);
            for (uint8_t page = 3; page <= 5; page++) {
                display_set_position(i2c, 0, page);
                HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);
            }
        }

        cache->time_ms = display_time_ms;
        cache->is_blink_visible = is_blink_visible;
    }

    // update footer if ready state changed
    // footer shows "Ready!" in armed/paused, "Byo-yomi" during byo-yomi overtime
    uint8_t show_byoyomi_footer = (game.phase == GAME_PHASE_RUNNING &&
                                   ps->config.time_control_mode == TIME_CONTROL_BYOYOMI &&
                                   ps->current_time_ms == 0);

    // combine ready and byo-yomi into a single footer state for cache comparison
    // use display_ready for normal states, override with byoyomi indicator
    uint8_t effective_ready = display_ready;
    if (show_byoyomi_footer) {
        // use a special value (2) to distinguish from normal ready
        effective_ready = 2;
    }

    if (cache->is_ready != effective_ready) {
        // clear footer area (page 7)
        uint8_t clear_data[129];
        clear_data[0] = 0x40;
        memset(&clear_data[1], 0x00, 128);
        display_set_position(i2c, 0, 7);
        HAL_I2C_Master_Transmit(i2c, DISPLAY_I2C_ADDRESS, clear_data, 129, 1000);

        if (show_byoyomi_footer) {
            // "Byo-yomi" centered: 8 chars * 6 pixels = 48 pixels, (128-48)/2 = 40
            display_draw_string(i2c, 40, 7, "Byo-yomi");
        } else {
            display_draw_footer(i2c, display_ready);
        }

        cache->is_ready = effective_ready;
    }
}

// <---- initialization ---->

void game_init(void) {
    // set default config for both players
    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        set_default_config(&game.players[i].config);
        load_starting_times(&game.players[i]);
        game.players[i].in_menu = FALSE;
        game.players[i].is_loser = FALSE;
    }

    // start in armed phase
    game.phase = GAME_PHASE_ARMED;
    game.active_player = PLAYER_1;

    // initialize display caches to force full redraw on first update
    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        display_cache[i].needs_full_redraw = TRUE;
        display_cache[i].in_menu = FALSE;
    }

    // initialize menu hold edge detection
    menu_hold_triggered[PLAYER_1] = FALSE;
    menu_hold_triggered[PLAYER_2] = FALSE;

    // initialize buzzer state
    buzzer_state = BUZZER_IDLE;
}

// <---- main update ---->

void game_update(void) {
    // check menu hold for both players (works in any phase)
    handle_menu_hold();

    // phase-specific input handling
    switch (game.phase) {
        case GAME_PHASE_ARMED:
            handle_input_armed();
            break;

        case GAME_PHASE_RUNNING:
            handle_input_running();
            break;

        case GAME_PHASE_PAUSED:
            handle_input_paused();
            break;

        case GAME_PHASE_FINISHED:
            handle_input_finished();
            break;

        default:
            break;
    }

    // advance buzzer pattern if active
    update_buzzer_pattern();

    // update displays for both players
    update_player_display(PLAYER_1);
    update_player_display(PLAYER_2);
}

// <---- clock tick (called from TIM2 ISR every 100ms) ---->

void game_tick(void) {
    // only tick when game is running
    if (game.phase != GAME_PHASE_RUNNING) {
        return;
    }

    player_state_t* active = &game.players[game.active_player];

    switch (active->config.time_control_mode) {
        case TIME_CONTROL_NONE:
        case TIME_CONTROL_INCREMENT:
        case TIME_CONTROL_PARTIAL:
            // simple countdown: decrement main time
            if (active->current_time_ms >= CLOCK_TICK_INTERVAL_MS) {
                active->current_time_ms -= CLOCK_TICK_INTERVAL_MS;
            } else {
                active->current_time_ms = 0;
            }
            break;

        case TIME_CONTROL_DELAY:
            // bonus time counts down first, then main time
            if (active->current_bonus_ms >= CLOCK_TICK_INTERVAL_MS) {
                active->current_bonus_ms -= CLOCK_TICK_INTERVAL_MS;
            } else {
                // bonus exhausted, decrement main time
                active->current_bonus_ms = 0;
                if (active->current_time_ms >= CLOCK_TICK_INTERVAL_MS) {
                    active->current_time_ms -= CLOCK_TICK_INTERVAL_MS;
                } else {
                    active->current_time_ms = 0;
                }
            }
            break;

        case TIME_CONTROL_LIMITED:
            // both count down simultaneously
            if (active->current_bonus_ms >= CLOCK_TICK_INTERVAL_MS) {
                active->current_bonus_ms -= CLOCK_TICK_INTERVAL_MS;
            } else {
                active->current_bonus_ms = 0;
            }
            if (active->current_time_ms >= CLOCK_TICK_INTERVAL_MS) {
                active->current_time_ms -= CLOCK_TICK_INTERVAL_MS;
            } else {
                active->current_time_ms = 0;
            }

            // bonus reaching 0 in limited mode means immediate loss
            if (active->current_bonus_ms == 0) {
                transition_to_finished(game.active_player);
                return;
            }
            break;

        case TIME_CONTROL_BYOYOMI:
            // main time counts down first, then bonus time
            if (active->current_time_ms >= CLOCK_TICK_INTERVAL_MS) {
                active->current_time_ms -= CLOCK_TICK_INTERVAL_MS;
            } else {
                active->current_time_ms = 0;
                // main time exhausted, decrement bonus time
                if (active->current_bonus_ms >= CLOCK_TICK_INTERVAL_MS) {
                    active->current_bonus_ms -= CLOCK_TICK_INTERVAL_MS;
                } else {
                    active->current_bonus_ms = 0;
                }
            }
            break;

        default:
            break;
    }

    // check for timeout (main time and bonus both at 0 for byoyomi, just main for others)
    uint8_t timed_out = FALSE;

    if (active->config.time_control_mode == TIME_CONTROL_BYOYOMI) {
        // byoyomi: lose only when both main and bonus are 0
        timed_out = (active->current_time_ms == 0 && active->current_bonus_ms == 0);
    } else {
        // all other modes: lose when main time reaches 0
        // (limited mode already handled above with bonus check)
        timed_out = (active->current_time_ms == 0);
    }

    if (timed_out) {
        transition_to_finished(game.active_player);
    }
}

// <---- state accessors ---->

game_t* game_get_state(void) {
    return &game;
}

player_state_t* game_get_player_state(player_id_t player) {
    if (player >= PLAYER_COUNT) {
        return &game.players[PLAYER_1];
    }
    return &game.players[player];
}

game_phase_t game_get_phase(void) {
    return game.phase;
}

// <---- callbacks for menu module ---->

/**
 * called by menu module when player selects "Ready!"
 * clears in_menu flag and invalidates display cache
 */
void game_player_ready(player_id_t player) {
    if (player >= PLAYER_COUNT) {
        return;
    }

    game.players[player].in_menu = FALSE;
    invalidate_display_cache(player);
}

/**
 * called by menu module when player selects "Reset" from paused menu
 * transitions to armed with current config for both players
 */
void game_request_reset(void) {
    transition_to_armed();
}