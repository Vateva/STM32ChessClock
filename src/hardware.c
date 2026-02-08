/*
 * hardware abstraction layer implementation
 */

#include "hardware.h"

#include "config.h"

// <---- private hardware handles ---->

// i2c handles for displays
static I2C_HandleTypeDef hi2c1;
static I2C_HandleTypeDef hi2c2;

// timer handles
static TIM_HandleTypeDef htim2;  // clock tick timer (100ms interrupts)
static TIM_HandleTypeDef htim3;  // buzzer pwm timer

// <---- main initialization function ---->

void hardware_init(void) {
  // initialize subsystems in correct order
  hardware_init_system_clock();
  hardware_init_gpio();
  hardware_init_i2c();
  hardware_init_timers();
}

// <---- system clock configuration ---->

void hardware_init_system_clock(void) {
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  // configure hsi oscillator (8mhz internal)
  // use directly without pll for lower power consumption
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;  // no pll
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

  // configure system clocks
  // sysclk = 8mhz (from hsi)
  // ahb = 8mhz (no division)
  // apb1 = 8mhz (no division)
  // apb2 = 8mhz (no division)
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;  // 8mhz
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;   // 8mhz
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;   // 8mhz
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0);
}

// <---- gpio initialization ---->

void hardware_init_gpio(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // enable gpio and afio clocks (afio needed for exti on stm32f1)
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_AFIO_CLK_ENABLE();

  // <---- player 1 gpio configuration ---->

  // player 1 encoder pins (exti on both edges for interrupt-driven quadrature decoding)
  GPIO_InitStruct.Pin = PLAYER1_ENCODER_A_PIN | PLAYER1_ENCODER_B_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PLAYER1_ENCODER_PORT, &GPIO_InitStruct);

  // enable exti interrupts for player 1 encoder (pa0 -> exti0, pa1 -> exti1)
  HAL_NVIC_SetPriority(EXTI0_IRQn, ENCODER_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(EXTI0_IRQn);
  HAL_NVIC_SetPriority(EXTI1_IRQn, ENCODER_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(EXTI1_IRQn);

  // player 1 button pins (input with pull-up, buttons are active-low)
  GPIO_InitStruct.Pin =
      PLAYER1_ENCODER_PUSH_PIN | PLAYER1_MENU_BUTTON_PIN | PLAYER1_BACK_BUTTON_PIN | PLAYER1_TAP_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PLAYER1_BUTTON_PORT, &GPIO_InitStruct);

  // <---- player 2 gpio configuration ---->

  // player 2 encoder pins (exti on both edges for interrupt-driven quadrature decoding)
  GPIO_InitStruct.Pin = PLAYER2_ENCODER_A_PIN | PLAYER2_ENCODER_B_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PLAYER2_ENCODER_PORT, &GPIO_InitStruct);

  // enable exti interrupts for player 2 encoder (pb12 + pb13 -> shared exti15_10)
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, ENCODER_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  // player 2 button pins on port b (input with pull-up)
  GPIO_InitStruct.Pin = PLAYER2_ENCODER_PUSH_PIN | PLAYER2_MENU_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PLAYER2_BUTTON_PORT_B, &GPIO_InitStruct);

  // player 2 button pins on port a (input with pull-up) (not enough pins left on port b)
  GPIO_InitStruct.Pin = PLAYER2_BACK_BUTTON_PIN | PLAYER2_TAP_BUTTON_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(PLAYER2_BUTTON_PORT_A, &GPIO_InitStruct);

  // <---- buzzer gpio configuration ---->

  // buzzer pin configured as alternate function for tim3 pwm
  GPIO_InitStruct.Pin = BUZZER_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BUZZER_PORT, &GPIO_InitStruct);
}

// <---- i2c initialization ---->

void hardware_init_i2c(void) {
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  // enable i2c peripheral clocks
  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_I2C2_CLK_ENABLE();

  // <---- i2c1 configuration (player 1 display) ---->

  // configure i2c1 pins (pb6=scl, pb7=sda)
  // alternate function open-drain with no pull resistors (external pull-ups on module)
  GPIO_InitStruct.Pin = PLAYER1_I2C_SCL_PIN | PLAYER1_I2C_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PLAYER1_I2C_PORT, &GPIO_InitStruct);

  // configure i2c1 peripheral
  // 400khz fast mode i2c
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 400000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  HAL_I2C_Init(&hi2c1);

  // <---- i2c2 configuration (player 2 display) ---->

  // configure i2c2 pins (pb10=scl, pb11=sda)
  GPIO_InitStruct.Pin = PLAYER2_I2C_SCL_PIN | PLAYER2_I2C_SDA_PIN;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(PLAYER2_I2C_PORT, &GPIO_InitStruct);

  // configure i2c2 peripheral
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

// <---- timer initialization ---->

void hardware_init_timers(void) {
  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  // enable timer clocks
  __HAL_RCC_TIM2_CLK_ENABLE();
  __HAL_RCC_TIM3_CLK_ENABLE();

  // <---- TIM2 configuration (clock tick timer - 100ms interrupts) ---->

  // TIM2 is on APB1 (8mhz)
  // timer clock = apb1 = 8mhz (no multiplier since apb1 prescaler = 1)
  // desired interrupt frequency = 10hz (100ms period)
  // prescaler = 800 - 1 (8mhz / 800 = 10khz timer clock)
  // period = 1000 - 1 (10khz / 1000 = 10hz interrupt rate)

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 800 - 1;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1000 - 1;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim2);

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

  // enable tim2 interrupt in nvic
  HAL_NVIC_SetPriority(TIM2_IRQn, TIMER_INTERRUPT_PRIORITY, 0);
  HAL_NVIC_EnableIRQ(TIM2_IRQn);

  // note: timer is configured but NOT started yet
  // call hardware_start_clock_timer() to start it

  // <---- TIM3 configuration (buzzer pwm - 8500hz) ---->

  // TIM3 is on APB1 (8mhz)
  // timer clock = apb1 = 8mhz
  // desired pwm frequency = 8500hz
  // prescaler = 1 - 1 (no prescaling, timer runs at 8mhz)
  // period = 941 - 1 (8mhz / 941 ≈ 8500hz)
  // duty cycle = 90% → pulse = 847

  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 1 - 1;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 941 - 1;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  HAL_TIM_Base_Init(&htim3);

  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig);

  HAL_TIM_PWM_Init(&htim3);

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig);

  // configure pwm channel 1 (pa6 - buzzer pin)
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 847;  // 90% duty cycle (847 / 941 ≈ 0.9)
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1);

  // note: pwm is configured but NOT started yet
  // call hardware_buzzer_on() to start it
}

// <---- clock timer control ---->

void hardware_start_clock_timer(void) {
  // start tim2 in interrupt mode
  HAL_TIM_Base_Start_IT(&htim2);
}

void hardware_stop_clock_timer(void) {
  // stop tim2 interrupts
  HAL_TIM_Base_Stop_IT(&htim2);
}

// <---- buzzer control ---->

void hardware_buzzer_on(void) {
  // start pwm output on tim3 channel 1
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);
}

void hardware_buzzer_off(void) {
  // stop pwm output on tim3 channel 1
  HAL_TIM_PWM_Stop(&htim3, TIM_CHANNEL_1);
}

// <---- timer handle access ---->

TIM_HandleTypeDef* hardware_get_tim2(void) {
  return &htim2;
}

// <---- i2c handle getters ---->

I2C_HandleTypeDef* hardware_get_i2c1(void) {
  return &hi2c1;
}

I2C_HandleTypeDef* hardware_get_i2c2(void) {
  return &hi2c2;
}