/*
 * minimal test program for display clock function
 * displays a countdown timer using the large font
 */

#include "config.h"
#include "display.h"
#include "stm32f1xx_hal.h"

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

  // test different modes with varying times
  typedef struct {
    uint8_t readycheck;
    uint32_t main_time;
    time_control_mode_t mode;
    uint32_t bonus_time;
    const char* description;
  } test_case_t;

  test_case_t test_cases[] = {
      {TRUE, 3661000, TIME_CONTROL_NONE, 0, "None mode"},
      {FALSE, 305000, TIME_CONTROL_INCREMENT, 3000, "Increment 3s"},
      {TRUE, 59000, TIME_CONTROL_DELAY, 2000, "Delay 2s"},
      {FALSE, 120000, TIME_CONTROL_PARTIAL, 5000, "Partial 5s"},
      {TRUE, 600000, TIME_CONTROL_LIMITED, 30000, "Limited 30s"},
      {FALSE, 180000, TIME_CONTROL_BYOYOMI, 30000, "Byo-yomi 30s"},
  };

  uint8_t test_index = 0;
  uint32_t last_update = 0;

  while (1) {
    // update display every 2 seconds
    if ((HAL_GetTick() - last_update) >= 3000) {
      last_update = HAL_GetTick();

      // clear and draw new time
      display_clear(&hi2c1);
      display_clear(&hi2c2);

      test_case_t current = test_cases[test_index];

      display_draw_clock_screen(&hi2c1, current.main_time, current.mode, current.bonus_time, current.readycheck);
      display_draw_clock_screen(&hi2c2, current.main_time, current.mode, current.bonus_time, current.readycheck);

      // cycle through test cases
      test_index++;
      if (test_index >= 6) {
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
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  // 64mhz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;   // 32mhz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   // 64mhz
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