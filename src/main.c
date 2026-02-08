/*
 * test program for game state machine
 * tests phase transitions, clock countdown, time controls, and buzzer
 *
 * temporary workaround: back button simulates menu "Ready!" selection
 * since menu module is not yet implemented
 *
 * test flow:
 *   1. power on -> armed (both displays show 05:00:00 + "Ready!")
 *   2. tap either player's button -> running (other player's clock starts)
 *   3. active player taps -> switch turns (time control applied)
 *   4. clock reaches 0 -> finished (buzzer beeps, loser blinks)
 *   5. tap any button -> armed (reset to defaults)
 *   6. hold menu button -> that player enters menu (display cleared with "IN MENU")
 *   7. press back button -> simulates "Ready!" (exits menu)
 */

#include "stm32f1xx_hal.h"
#include "config.h"
#include "hardware.h"
#include "display.h"
#include "button.h"
#include "encoder.h"
#include "game.h"

// <---- temporary menu simulation ---->

// back button ids for each player, used to simulate menu "Ready!"
static const button_id_t back_buttons[PLAYER_COUNT] = {
    BUTTON_PLAYER1_BACK,
    BUTTON_PLAYER2_BACK
};

// temporary: draw a simple "in menu" placeholder on a player's display
static void draw_menu_placeholder(player_id_t player) {
    I2C_HandleTypeDef* i2c;
    if (player == PLAYER_1) {
        i2c = hardware_get_i2c1();
    } else {
        i2c = hardware_get_i2c2();
    }

    display_clear(i2c);
    display_draw_string(i2c, 30, 3, "IN MENU");
    display_draw_string(i2c, 12, 5, "BACK = Ready!");
}

// tracks whether we already drew the placeholder to avoid redrawing every loop
static uint8_t menu_placeholder_drawn[PLAYER_COUNT] = {FALSE, FALSE};

// temporary: check back buttons to simulate menu "Ready!" selection
// also draws placeholder when player first enters menu
static void simulate_menu(void) {
    for (uint8_t i = 0; i < PLAYER_COUNT; i++) {
        player_state_t* ps = game_get_player_state((player_id_t)i);

        if (ps->in_menu) {
            // draw placeholder once when entering menu
            if (!menu_placeholder_drawn[i]) {
                draw_menu_placeholder((player_id_t)i);
                menu_placeholder_drawn[i] = TRUE;
            }

            // back button simulates selecting "Ready!"
            if (button_was_pressed(back_buttons[i])) {
                game_player_ready((player_id_t)i);
                menu_placeholder_drawn[i] = FALSE;
            }
        } else {
            menu_placeholder_drawn[i] = FALSE;
        }
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

    // initialize game module (starts in armed with defaults)
    game_init();

    // main loop
    while (1) {
        // poll button states
        button_update();

        // temporary: handle menu simulation with back buttons
        simulate_menu();

        // run game state machine (input handling, transitions, display)
        game_update();

        // small delay to prevent cpu spinning too fast
        HAL_Delay(1);
    }
}