/*
 * chess clock main program
 * wires together game and menu modules
 * acts as coordinator — neither module knows about the other
 */

#include "stm32f1xx_hal.h"
#include "config.h"
#include "hardware.h"
#include "display.h"
#include "button.h"
#include "encoder.h"
#include "game.h"
#include "menu.h"

// <---- internal helper: open menu for a player ---->

// reads game state and passes the right pointers to the menu module
static void open_menu_for_player(player_id_t player) {
    player_state_t* ps = game_get_player_state(player);
    game_phase_t phase = game_get_phase();

    // determine whether we're editing a paused game or configuring a new one
    // note: RUNNING transitions to PAUSED before menu opens (handled by game module)
    // FINISHED transitions to ARMED before menu opens (handled by game module)
    // so by this point phase is either ARMED or PAUSED
    uint8_t is_paused = (phase == GAME_PHASE_PAUSED) ? TRUE : FALSE;

    if (is_paused) {
        // determine which bonus pointer to pass based on mode type
        uint32_t* bonus_ptr;
        time_control_mode_t mode = ps->config.time_control_mode;

        if (mode == TIME_CONTROL_DELAY ||
            mode == TIME_CONTROL_LIMITED ||
            mode == TIME_CONTROL_BYOYOMI) {
            // countdown modes: edit the live bonus timer
            bonus_ptr = &ps->current_bonus_ms;
        } else if (mode == TIME_CONTROL_INCREMENT ||
                   mode == TIME_CONTROL_PARTIAL) {
            // increment modes: edit the configured increment amount
            bonus_ptr = &ps->config.bonus_time_ms[mode];
        } else {
            // none: no bonus to edit
            bonus_ptr = NULL;
        }

        menu_open((uint8_t)player,
                  &ps->config,
                  &ps->current_time_ms,
                  bonus_ptr,
                  TRUE);
    } else {
        // configuring new game: no live values to edit
        menu_open((uint8_t)player,
                  &ps->config,
                  NULL,
                  NULL,
                  FALSE);
    }
}

// <---- internal helper: handle menu results for a player ---->

// checks menu actions and relays them to game module
static void process_menu_result(player_id_t player) {
    if (!menu_is_open((uint8_t)player)) {
        return;
    }

    menu_action_t action = menu_update((uint8_t)player);

    switch (action) {
        case MENU_ACTION_READY:
            // player selected "Ready!" — tell game module
            game_player_ready(player);
            break;

        case MENU_ACTION_RESET:
            // player confirmed reset — tell game module
            game_request_reset();
            break;

        case MENU_ACTION_NONE:
            // menu still active, nothing to relay
            break;

        default:
            break;
    }
}

// <---- internal helper: detect menu entry and call menu_open ---->

// game module sets in_menu flag but doesn't open the menu module
// main.c detects the flag transition and calls menu_open
static uint8_t prev_in_menu[PLAYER_COUNT] = {FALSE, FALSE};

static void check_menu_open(void) {
    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        player_state_t* ps = game_get_player_state((player_id_t)i);

        // detect transition: was not in menu, now is in menu
        if (ps->in_menu && !prev_in_menu[i]) {
            open_menu_for_player((player_id_t)i);
        }

        prev_in_menu[i] = ps->in_menu;
    }
}

// <---- interrupt handlers ---->

// systick handler: required for hal timing (HAL_GetTick, HAL_Delay)
void SysTick_Handler(void) {
    HAL_IncTick();
}

// tim2 handler: fires every 100ms, decrements active player's clock
void TIM2_IRQHandler(void) {
    HAL_TIM_IRQHandler(hardware_get_tim2());
    game_tick();
}

// <---- main ---->

int main(void) {
    // initialize hal
    HAL_Init();

    // initialize all hardware subsystems
    hardware_init();

    // initialize input modules
    button_init();
    encoder_init();

    // initialize displays
    I2C_HandleTypeDef* i2c1 = hardware_get_i2c1();
    I2C_HandleTypeDef* i2c2 = hardware_get_i2c2();
    HAL_Delay(100);
    display_init(i2c1);
    display_init(i2c2);
    display_clear(i2c1);
    display_clear(i2c2);

    // initialize game and menu modules
    game_init();
    menu_init();

    // main loop
    while (1) {
        // poll button states
        button_update();

        // detect menu open transitions and wire to menu module
        check_menu_open();

        // update menu for players who are in menu
        // relay any actions back to game module
        process_menu_result(PLAYER_1);
        process_menu_result(PLAYER_2);

        // run game state machine (input handling, transitions, display)
        game_update();

        // small delay to prevent cpu spinning too fast
        HAL_Delay(1);
    }
}