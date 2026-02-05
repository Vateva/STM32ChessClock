/*
 * chess clock hardware test program with sh1106 driver
 * tests all components sequentially
 * 
 * player 1 module on left, player 2 module on right
 * uses stm32f103c8t6 with hal
 */

#include "stm32f1xx_hal.h"
#include <string.h>
#include <stdio.h>

// pin definitions for player 1
#define P1_I2C_SCL_PIN    GPIO_PIN_6
#define P1_I2C_SDA_PIN    GPIO_PIN_7
#define P1_I2C_PORT       GPIOB
#define P1_ENC_A_PIN      GPIO_PIN_0
#define P1_ENC_B_PIN      GPIO_PIN_1
#define P1_ENC_PORT       GPIOA
#define P1_ENC_PUSH_PIN   GPIO_PIN_2
#define P1_CONFIRM_PIN    GPIO_PIN_3
#define P1_BACK_PIN       GPIO_PIN_4
#define P1_TAP_PIN        GPIO_PIN_5
#define P1_BTN_PORT       GPIOA

// pin definitions for player 2
#define P2_I2C_SCL_PIN    GPIO_PIN_10
#define P2_I2C_SDA_PIN    GPIO_PIN_11
#define P2_I2C_PORT       GPIOB
#define P2_ENC_A_PIN      GPIO_PIN_12
#define P2_ENC_B_PIN      GPIO_PIN_13
#define P2_ENC_PORT       GPIOB
#define P2_ENC_PUSH_PIN   GPIO_PIN_14
#define P2_CONFIRM_PIN    GPIO_PIN_15
#define P2_BTN_PORT       GPIOB
#define P2_BACK_PIN       GPIO_PIN_8 
#define P2_TAP_PIN        GPIO_PIN_9
#define P2_BTN2_PORT      GPIOA

// buzzer pin
#define BUZZER_PIN        GPIO_PIN_6
#define BUZZER_PORT       GPIOA

// sh1106 i2c address
#define SH1106_ADDR      0x3C << 1

// sh1106 commands
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

// display dimensions
#define SH1106_WIDTH  128
#define SH1106_HEIGHT 64

// 5x7 font - standard ascii font (space through Z)
static const uint8_t font_5x7[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // space (32)
    {0x00, 0x00, 0x5F, 0x00, 0x00}, // ! (33)
    {0x00, 0x07, 0x00, 0x07, 0x00}, // " (34)
    {0x14, 0x7F, 0x14, 0x7F, 0x14}, // # (35)
    {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // $ (36)
    {0x23, 0x13, 0x08, 0x64, 0x62}, // % (37)
    {0x36, 0x49, 0x55, 0x22, 0x50}, // & (38)
    {0x00, 0x05, 0x03, 0x00, 0x00}, // ' (39)
    {0x00, 0x1C, 0x22, 0x41, 0x00}, // ( (40)
    {0x00, 0x41, 0x22, 0x1C, 0x00}, // ) (41)
    {0x14, 0x08, 0x3E, 0x08, 0x14}, // * (42)
    {0x08, 0x08, 0x3E, 0x08, 0x08}, // + (43)
    {0x00, 0x50, 0x30, 0x00, 0x00}, // , (44)
    {0x08, 0x08, 0x08, 0x08, 0x08}, // - (45)
    {0x00, 0x60, 0x60, 0x00, 0x00}, // . (46)
    {0x20, 0x10, 0x08, 0x04, 0x02}, // / (47)
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 (48)
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1 (49)
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2 (50)
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3 (51)
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4 (52)
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5 (53)
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6 (54)
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7 (55)
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8 (56)
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9 (57)
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : (58)
    {0x00, 0x56, 0x36, 0x00, 0x00}, // ; (59)
    {0x08, 0x14, 0x22, 0x41, 0x00}, // < (60)
    {0x14, 0x14, 0x14, 0x14, 0x14}, // = (61)
    {0x00, 0x41, 0x22, 0x14, 0x08}, // > (62)
    {0x02, 0x01, 0x51, 0x09, 0x06}, // ? (63)
    {0x32, 0x49, 0x79, 0x41, 0x3E}, // @ (64)
    {0x7E, 0x11, 0x11, 0x11, 0x7E}, // A (65)
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B (66)
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C (67)
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D (68)
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E (69)
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F (70)
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G (71)
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H (72)
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I (73)
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J (74)
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K (75)
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L (76)
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M (77)
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N (78)
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O (79)
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P (80)
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q (81)
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R (82)
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S (83)
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T (84)
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U (85)
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V (86)
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W (87)
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X (88)
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y (89)
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z (90)
};

// global handles
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

// encoder states
static volatile int8_t encoder_p1_delta = 0;
static volatile int8_t encoder_p2_delta = 0;
static volatile uint8_t encoder_p1_last_state = 0;
static volatile uint8_t encoder_p2_last_state = 0;

// function prototypes
void system_clock_config(void);
void gpio_init(void);
void i2c1_init(void);
void i2c2_init(void);
void sh1106_init(I2C_HandleTypeDef *hi2c);
void sh1106_send_command(I2C_HandleTypeDef *hi2c, uint8_t cmd);
void sh1106_set_position(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page);
void sh1106_draw_char(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page, char c);
void sh1106_draw_string(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page, const char *str);
void sh1106_clear(I2C_HandleTypeDef *hi2c);
void delay_ms(uint32_t ms);
void test_displays(void);
void test_encoders(void);
void test_buttons(void);
void test_buzzer(void);

int main(void) {
    // initialize hal library
    HAL_Init();
    
    // configure system clock to 72mhz
    system_clock_config();
    
    // initialize peripherals
    gpio_init();
    i2c1_init();
    i2c2_init();
    
    // initialize displays
    delay_ms(100); // give displays time to power up
    sh1106_init(&hi2c1);
    sh1106_init(&hi2c2);
    
    // welcome message
    sh1106_clear(&hi2c1);
    sh1106_draw_string(&hi2c1, 10, 1, "PLAYER 1");
    sh1106_draw_string(&hi2c1, 10, 3, "TEST MODE");
    
    sh1106_clear(&hi2c2);
    sh1106_draw_string(&hi2c2, 10, 1, "PLAYER 2");
    sh1106_draw_string(&hi2c2, 3, 3, "TEST MODE");
    
    delay_ms(2000);
    
    // run tests
    while (1) {
        //test_displays();
        //delay_ms(5000);
        
        //test_encoders();
       // delay_ms(5000);
        
        test_buttons();
        delay_ms(30000);
        
        //test_buzzer();
        //delay_ms(5000);
    }
}

// configure system clock to 72mhz using internal oscillator
void system_clock_config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // configure the main pll using hsi
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // configure clocks
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;   // 64mhz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;    // 32mhz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;    // 64mhz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

// initialize all gpio pins
void gpio_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // enable gpio clocks
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    // player 1 encoder inputs (with interrupt)
    GPIO_InitStruct.Pin = P1_ENC_A_PIN | P1_ENC_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // module has external pull-ups
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(P1_ENC_PORT, &GPIO_InitStruct);
    
    // player 1 buttons (input with pull-up)
    GPIO_InitStruct.Pin = P1_ENC_PUSH_PIN | P1_CONFIRM_PIN | P1_BACK_PIN | P1_TAP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // module has external pull-ups
    HAL_GPIO_Init(P1_BTN_PORT, &GPIO_InitStruct);
    
    // player 2 encoder inputs (with interrupt)
    GPIO_InitStruct.Pin = P2_ENC_A_PIN | P2_ENC_B_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(P2_ENC_PORT, &GPIO_InitStruct);
    
    // player 2 buttons on port b
    GPIO_InitStruct.Pin = P2_ENC_PUSH_PIN | P2_CONFIRM_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(P2_BTN_PORT, &GPIO_InitStruct);
    
    // player 2 buttons on port a
    GPIO_InitStruct.Pin = P2_BACK_PIN | P2_TAP_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP; // external buttons need pull-ups
    HAL_GPIO_Init(P2_BTN2_PORT, &GPIO_InitStruct);
    
    // buzzer output
    GPIO_InitStruct.Pin = BUZZER_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
    HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
    
    // enable and set exti line interrupts
    HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI0_IRQn);
    
    HAL_NVIC_SetPriority(EXTI1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI1_IRQn);
    
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

// initialize i2c1 for player 1 display
void i2c1_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // enable i2c1 clock
    __HAL_RCC_I2C1_CLK_ENABLE();
    
    // configure i2c1 pins (pb6=scl, pb7=sda)
    GPIO_InitStruct.Pin = P1_I2C_SCL_PIN | P1_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL; // module has external pull-ups
    HAL_GPIO_Init(P1_I2C_PORT, &GPIO_InitStruct);
    
    // configure i2c1
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000; // 400khz fast mode
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

// initialize i2c2 for player 2 display
void i2c2_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // enable i2c2 clock
    __HAL_RCC_I2C2_CLK_ENABLE();
    
    // configure i2c2 pins (pb10=scl, pb11=sda)
    GPIO_InitStruct.Pin = P2_I2C_SCL_PIN | P2_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(P2_I2C_PORT, &GPIO_InitStruct);
    
    // configure i2c2
    hi2c2.Instance = I2C2;
    hi2c2.Init.ClockSpeed = 400000;
    hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c2);
}

// send command to sh1106
void sh1106_send_command(I2C_HandleTypeDef *hi2c, uint8_t cmd) {
    uint8_t data[2] = {0x00, cmd}; // 0x00 = command mode
    HAL_I2C_Master_Transmit(hi2c, SH1106_ADDR, data, 2, 100);
}

// initialize sh1106 display
void sh1106_init(I2C_HandleTypeDef *hi2c) {
    delay_ms(100);
    
    sh1106_send_command(hi2c, SH1106_CMD_DISPLAY_OFF);
    sh1106_send_command(hi2c, SH1106_CMD_SET_CLOCK_DIV);
    sh1106_send_command(hi2c, 0x80);
    sh1106_send_command(hi2c, SH1106_CMD_SET_MULTIPLEX);
    sh1106_send_command(hi2c, 0x3F);
    sh1106_send_command(hi2c, SH1106_CMD_SET_DISPLAY_OFFSET);
    sh1106_send_command(hi2c, 0x00);
    sh1106_send_command(hi2c, 0x40 | 0x00);
    sh1106_send_command(hi2c, SH1106_CMD_CHARGE_PUMP);
    sh1106_send_command(hi2c, 0x14);
    sh1106_send_command(hi2c, SH1106_CMD_SET_SEGMENT_REMAP);
    sh1106_send_command(hi2c, SH1106_CMD_SET_SCAN_DIR);
    sh1106_send_command(hi2c, SH1106_CMD_SET_COM_PINS);
    sh1106_send_command(hi2c, 0x12);
    sh1106_send_command(hi2c, SH1106_CMD_SET_CONTRAST);
    sh1106_send_command(hi2c, 0xCF);
    sh1106_send_command(hi2c, SH1106_CMD_SET_PRECHARGE);
    sh1106_send_command(hi2c, 0xF1);
    sh1106_send_command(hi2c, SH1106_CMD_SET_VCOM_DESELECT);
    sh1106_send_command(hi2c, 0x40);
    sh1106_send_command(hi2c, 0xA4);
    sh1106_send_command(hi2c, 0xA6);
    sh1106_send_command(hi2c, SH1106_CMD_DISPLAY_ON);
    
    delay_ms(100);
}

// set cursor position (x in pixels, page 0-7)
void sh1106_set_position(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page) {
    // sh1106 has 2 pixel offset
    x += 2;
    
    sh1106_send_command(hi2c, SH1106_CMD_SET_PAGE_ADDR | (page & 0x07));
    sh1106_send_command(hi2c, SH1106_CMD_SET_COLUMN_LOW | (x & 0x0F));
    sh1106_send_command(hi2c, SH1106_CMD_SET_COLUMN_HIGH | ((x >> 4) & 0x0F));
}

// draw a character at position (x in pixels, page 0-7)
void sh1106_draw_char(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page, char c) {
    uint8_t font_idx;
    
    // font array starts at ascii 32 (space) and goes to 90 (Z)
    if (c >= 32 && c <= 90) {
        font_idx = c - 32;
    } else {
        font_idx = 0; // default to space
    }
    
    // set position and send character data
    sh1106_set_position(hi2c, x, page);
    
    uint8_t data[6];
    data[0] = 0x40; // data mode
    for (uint8_t i = 0; i < 5; i++) {
        data[i + 1] = font_5x7[font_idx][i];
    }
    
    HAL_I2C_Master_Transmit(hi2c, SH1106_ADDR, data, 6, 100);
}

// draw string at position (x in pixels, page 0-7)
void sh1106_draw_string(I2C_HandleTypeDef *hi2c, uint8_t x, uint8_t page, const char *str) {
    uint8_t cur_x = x;
    while (*str) {
        sh1106_draw_char(hi2c, cur_x, page, *str);
        cur_x += 6; // 5 pixels + 1 pixel spacing
        str++;
    }
}

// clear display
void sh1106_clear(I2C_HandleTypeDef *hi2c) {
    uint8_t data[130];
    data[0] = 0x40; // data mode
    for (int i = 1; i < 130; i++) {
        data[i] = 0x00;
    }
    
    // clear all 8 pages
    for (uint8_t page = 0; page < 8; page++) {
        sh1106_set_position(hi2c, 0, page);
        HAL_I2C_Master_Transmit(hi2c, SH1106_ADDR, data, 130, 1000);
    }
}

// simple delay using hal delay
void delay_ms(uint32_t ms) {
    HAL_Delay(ms);
}

// test 1: display test - show test pattern on both displays
void test_displays(void) {
    sh1106_clear(&hi2c1);
    sh1106_draw_string(&hi2c1, 5, 1, "0123456789");
    sh1106_draw_string(&hi2c1, 5, 3, "TEST");
    
    sh1106_clear(&hi2c2);
    sh1106_draw_string(&hi2c2, 5, 1, "0123456789");
    sh1106_draw_string(&hi2c2, 5, 3, "TEST");
}

// test 2: encoder test - rotate encoders to see counter change
void test_encoders(void) {
    int16_t counter_p1 = 0;
    int16_t counter_p2 = 0;
    char buf[20];
    
    sh1106_clear(&hi2c1);
    sh1106_draw_string(&hi2c1, 5, 0, "ENCODER TEST");
    sh1106_draw_string(&hi2c1, 5, 2, "ROTATE P1:");
    
    sh1106_clear(&hi2c2);
    sh1106_draw_string(&hi2c2, 5, 0, "ENCODER TEST");
    sh1106_draw_string(&hi2c2, 5, 2, "ROTATE P2:");
    
    uint32_t start_time = HAL_GetTick();
    
    // run test for 5 seconds
    while ((HAL_GetTick() - start_time) < 5000) {
        // update counters from encoder deltas
        counter_p1 += encoder_p1_delta / 2;  //divide by 2 to count 1 unit per click
        encoder_p1_delta = 0;

        counter_p2 += encoder_p2_delta / 2;
        encoder_p2_delta = 0;
        // update display
        sprintf(buf, "%d  ", counter_p1);
        sh1106_draw_string(&hi2c1, 5, 4, buf);
        
        sprintf(buf, "%d  ", counter_p2);
        sh1106_draw_string(&hi2c2, 5, 4, buf);
        
        delay_ms(50);
    }
}

// test 3: button test - press buttons to see feedback
void test_buttons(void) {
    sh1106_clear(&hi2c1);
    sh1106_draw_string(&hi2c1, 5, 0, "BUTTON TEST");
    sh1106_draw_string(&hi2c1, 5, 1, "PRESS BTNS");
    
    sh1106_clear(&hi2c2);
    sh1106_draw_string(&hi2c2, 5, 0, "BUTTON TEST");
    sh1106_draw_string(&hi2c2, 5, 1, "PRESS BTNS");
    
    uint32_t start_time = HAL_GetTick();
    
    // run test for 5 seconds
    while ((HAL_GetTick() - start_time) < 30000) {
        // check player 1 buttons (active low)
        uint8_t p1_enc_push = !HAL_GPIO_ReadPin(P1_BTN_PORT, P1_ENC_PUSH_PIN);
        uint8_t p1_confirm = !HAL_GPIO_ReadPin(P1_BTN_PORT, P1_CONFIRM_PIN);
        uint8_t p1_back = !HAL_GPIO_ReadPin(P1_BTN_PORT, P1_BACK_PIN);
        uint8_t p1_tap = !HAL_GPIO_ReadPin(P1_BTN_PORT, P1_TAP_PIN);
        
        // check player 2 buttons (active low)
        uint8_t p2_enc_push = !HAL_GPIO_ReadPin(P2_BTN_PORT, P2_ENC_PUSH_PIN);
        uint8_t p2_confirm = !HAL_GPIO_ReadPin(P2_BTN_PORT, P2_CONFIRM_PIN);
        uint8_t p2_back = !HAL_GPIO_ReadPin(P2_BTN2_PORT, P2_BACK_PIN);
        uint8_t p2_tap = !HAL_GPIO_ReadPin(P2_BTN2_PORT, P2_TAP_PIN);
        
        // display player 1 button states
        if (p1_enc_push) sh1106_draw_string(&hi2c1, 5, 2, "ENC: >>");
        else sh1106_draw_string(&hi2c1, 5, 2, "ENC:   ");
        
        if (p1_confirm) sh1106_draw_string(&hi2c1, 5, 3, "CON: >>");
        else sh1106_draw_string(&hi2c1, 5, 3, "CON:   ");
        
        if (p1_back) sh1106_draw_string(&hi2c1, 5, 4, "BAK: >>");
        else sh1106_draw_string(&hi2c1, 5, 4, "BAK:   ");
        
        if (p1_tap) sh1106_draw_string(&hi2c1, 5, 5, "TAP: >>");
        else sh1106_draw_string(&hi2c1, 5, 5, "TAP:   ");
        
        // display player 2 button states
        if (p2_enc_push) sh1106_draw_string(&hi2c2, 5, 2, "ENC: >>");
        else sh1106_draw_string(&hi2c2, 5, 2, "ENC:   ");
        
        if (p2_confirm) sh1106_draw_string(&hi2c2, 5, 3, "CON: >>");
        else sh1106_draw_string(&hi2c2, 5, 3, "CON:   ");
        
        if (p2_back) sh1106_draw_string(&hi2c2, 5, 4, "BAK: >>");
        else sh1106_draw_string(&hi2c2, 5, 4, "BAK:   ");
        
        if (p2_tap) sh1106_draw_string(&hi2c2, 5, 5, "TAP: >>");
        else sh1106_draw_string(&hi2c2, 5, 5, "TAP:   ");
        
        delay_ms(50);
    }
}

// test 4: buzzer test - beep pattern
void test_buzzer(void) {
    sh1106_clear(&hi2c1);
    sh1106_draw_string(&hi2c1, 5, 2, "BUZZER TEST");
    
    sh1106_clear(&hi2c2);
    sh1106_draw_string(&hi2c2, 5, 2, "BUZZER TEST");
    
    // beep 3 times
    for (int i = 0; i < 3; i++) {
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_SET);
        delay_ms(100);
        HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
        delay_ms(200);
    }
}

// exti interrupt handler for pa0 (player 1 encoder a)
void EXTI0_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(P1_ENC_A_PIN);
}

// exti interrupt handler for pa1 (player 1 encoder b)
void EXTI1_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(P1_ENC_B_PIN);
}

// exti interrupt handler for pb12-pb13 (player 2 encoder)
void EXTI15_10_IRQHandler(void) {
    HAL_GPIO_EXTI_IRQHandler(P2_ENC_A_PIN);
    HAL_GPIO_EXTI_IRQHandler(P2_ENC_B_PIN);
}

// gpio exti callback - called by hal when interrupt fires
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
    // player 1 encoder
    if (GPIO_Pin == P1_ENC_A_PIN || GPIO_Pin == P1_ENC_B_PIN) {
        uint8_t a = HAL_GPIO_ReadPin(P1_ENC_PORT, P1_ENC_A_PIN);
        uint8_t b = HAL_GPIO_ReadPin(P1_ENC_PORT, P1_ENC_B_PIN);
        uint8_t state = (a << 1) | b;
        
        // decode quadrature
        uint8_t transition = (encoder_p1_last_state << 2) | state;
        
        // lookup table for quadrature decoding
        if (transition == 0b0001 || transition == 0b0111 || 
            transition == 0b1110 || transition == 0b1000) {
            encoder_p1_delta--;
        } else if (transition == 0b0010 || transition == 0b1011 || 
                   transition == 0b1101 || transition == 0b0100) {
            encoder_p1_delta++;
        }
        
        encoder_p1_last_state = state;
    }
    
    // player 2 encoder
    if (GPIO_Pin == P2_ENC_A_PIN || GPIO_Pin == P2_ENC_B_PIN) {
        uint8_t a = HAL_GPIO_ReadPin(P2_ENC_PORT, P2_ENC_A_PIN);
        uint8_t b = HAL_GPIO_ReadPin(P2_ENC_PORT, P2_ENC_B_PIN);
        uint8_t state = (a << 1) | b;
        
        uint8_t transition = (encoder_p2_last_state << 2) | state;
        
        if (transition == 0b0001 || transition == 0b0111 || 
            transition == 0b1110 || transition == 0b1000) {
            encoder_p2_delta++;
        } else if (transition == 0b0010 || transition == 0b1011 || 
                   transition == 0b1101 || transition == 0b0100) {
            encoder_p2_delta--;
        }
        
        encoder_p2_last_state = state;
    }
}

// required for hal
void SysTick_Handler(void) {
    HAL_IncTick();
}