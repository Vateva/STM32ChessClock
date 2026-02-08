/*
 * game module
 * manages game state machine, player clocks, and time control logic
 */

#ifndef GAME_H
#define GAME_H

#include <stdint.h>
#include "config.h"

// <---- player identification ---->

typedef enum {
    PLAYER_1 = 0,
    PLAYER_2 = 1,
    PLAYER_COUNT
} player_id_t;

// <---- game phase (global state machine) ---->

typedef enum {
    GAME_PHASE_ARMED,       // both players ready, waiting for first tap to start
    GAME_PHASE_RUNNING,     // game active, one player's clock counting down
    GAME_PHASE_PAUSED,      // game interrupted via menu, clocks frozen with saved times
    GAME_PHASE_FINISHED     // game over, one player ran out of time
} game_phase_t;

// <---- per-player runtime state ---->

typedef struct {
    player_config_t config;             // menu-configured settings (the "template", defined in config.h)
    uint32_t current_time_ms;           // live main clock (counts down during game)
    uint32_t current_bonus_ms;          // live bonus clock (counts down for delay/limited)
    uint8_t in_menu;                    // flag: player is currently in the menu
    uint8_t is_loser;                   // flag: this player lost (for finished display)
} player_state_t;

// <---- top-level game state ---->

typedef struct {
    game_phase_t phase;                 // current global game phase
    player_id_t active_player;          // which player's clock is counting down
    player_state_t players[PLAYER_COUNT];  // state for both players
} game_t;

// <---- initialization ---->

/**
 * initialize game module
 * sets phase to armed with default config for both players
 * call once at startup after hardware and display are initialized
 */
void game_init(void);

// <---- main update (call every main loop iteration) ---->

/**
 * update game logic
 * handles button input, state transitions, and display updates
 * call this every main loop iteration
 */
void game_update(void);

// <---- clock tick (call from timer interrupt) ---->

/**
 * decrement active player's clock by one tick (100ms)
 * applies time control rules (delay, limited, etc.)
 * call this from TIM2 interrupt handler every 100ms
 * only decrements if phase is RUNNING
 */
void game_tick(void);

// <---- state accessors ---->

/**
 * get pointer to the game state
 * useful for menu module to read phase and player data
 *
 * @return pointer to the global game state
 */
game_t* game_get_state(void);

/**
 * get pointer to a player's state
 * useful for menu module to edit config or current time
 *
 * @param player which player
 * @return pointer to that player's state
 */
player_state_t* game_get_player_state(player_id_t player);

/**
 * get the current game phase
 *
 * @return current game phase
 */
game_phase_t game_get_phase(void);

// <---- callbacks for menu module ---->

/**
 * called by menu module when player selects "Ready!"
 * clears in_menu flag and triggers display redraw
 *
 * @param player which player is now ready
 */
void game_player_ready(player_id_t player);

/**
 * called by menu module when player selects "Reset" from paused menu
 * transitions to armed state with current config for both players
 */
void game_request_reset(void);

#endif  // GAME_H