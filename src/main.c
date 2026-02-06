/*
 * minimal test program for display clock function
 * displays a countdown timer using the large font
 */

#include "stm32f1xx_hal.h"
#include "config.h"
#include "display.h"

// global i2c handles
I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

// function prototypes
void system_clock_config(void);
void i2c_init(void);

int main(void) {
    // initialize hal
    HAL_Init();
    
    // configure system clock to 64mhz (using hsi)
    system_clock_config();
    
    // initialize i2c
    i2c_init();
    
    // initialize displays
    HAL_Delay(100);
    display_init(&hi2c1);
    display_init(&hi2c2);
    
    // clear displays
    display_clear(&hi2c1);
    display_clear(&hi2c2);
    
    // test different time values (all show HH:MM:SS format)
    uint32_t test_times[] = {
        3661000,  // 01:01:01
        3599000,  // 00:59:59
        305000,   // 00:05:05
        59000,    // 00:00:59
        10000,    // 00:00:10
        1000,     // 00:00:01
        0         // 00:00:00
    };
    
    uint8_t test_index = 0;
    uint32_t last_update = 0;
    
    while (1) {
        // update display every 2 seconds
        if ((HAL_GetTick() - last_update) >= 2000) {
            last_update = HAL_GetTick();
            
            // clear and draw new time
            display_clear(&hi2c1);
            display_clear(&hi2c2);
            
            display_draw_clock(&hi2c1, test_times[test_index]);
            display_draw_clock(&hi2c2, test_times[test_index]);
            
            // show which test we're on (small text at top)
            char label[20];
            sprintf(label, "TEST %d/7", test_index + 1);
            display_draw_string(&hi2c1, 40, 0, label);
            display_draw_string(&hi2c2, 40, 0, label);
            
            // cycle through test times
            test_index++;
            if (test_index >= 7) {
                test_index = 0;
            }
        }
    }
}

// configure system clock to 64mhz using internal oscillator
void system_clock_config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    // configure pll using hsi
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI_DIV2;
    RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL16;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    // configure clocks (hsi: 8mhz -> /2 = 4mhz -> *16 = 64mhz)
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                                | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;   // 64mhz
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;    // 32mhz
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;    // 64mhz
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

// initialize both i2c peripherals
void i2c_init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    // enable clocks
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_I2C1_CLK_ENABLE();
    __HAL_RCC_I2C2_CLK_ENABLE();
    
    // configure i2c1 pins (pb6=scl, pb7=sda)
    GPIO_InitStruct.Pin = PLAYER1_I2C_SCL_PIN | PLAYER1_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(PLAYER1_I2C_PORT, &GPIO_InitStruct);
    
    // configure i2c1
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
    
    // configure i2c2 pins (pb10=scl, pb11=sda)
    GPIO_InitStruct.Pin = PLAYER2_I2C_SCL_PIN | PLAYER2_I2C_SDA_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(PLAYER2_I2C_PORT, &GPIO_InitStruct);
    
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

// required for hal
void SysTick_Handler(void) {
    HAL_IncTick();
}