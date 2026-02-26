# STM32ChessClock
Two-player chess clock built on an STM32F103C8 (Blue Pill) with dual SH1106 OLED displays, rotary encoders, and 6 time control modes.

Each player has their own display, rotary encoder, and dedicated buttons. The menu system allows fully independent configuration of starting time, time control mode, and bonus time per player -- making it easy to play odds games (e.g. 10 min vs 5 min, or different time control modes for each player).

## Table of Contents
- [Hardware Components](#hardware-components)
- [Assembly](#assembly)
- [Pinout Configuration](#pinout-configuration)
- [Key Features](#key-features)
  - [Game State Machine](#game-state-machine)
  - [6 Time Control Modes](#6-time-control-modes)
  - [Independent Player Menus](#independent-player-menus)
  - [Time Editor](#time-editor)
  - [Input System](#input-system)
  - [Buzzer](#buzzer)
  - [Boot Animation](#boot-animation)
  - [Display](#display)
- [Technical Specifications](#technical-specifications)
  - [Software Architecture](#software-architecture)
  - [Coordinator Pattern](#coordinator-pattern)
- [Build Environment](#build-environment)
- [Configuration](#configuration)
- [Usage](#usage)
- [License](#license)

## Hardware Components
- STM32F103C8 (Blue Pill) running at 8 MHz HSI
- 2x SH1106 128x64 OLED displays (I2C)
- 2x Rotary encoders with push buttons
- 6x Tactile buttons (menu, back, tap per player)
- Piezo buzzer (8500 Hz)
  
![chessclock components](https://github.com/user-attachments/assets/9850f6b1-77ef-4770-982f-09b8461e3403)

## Assembly

### Solder the components together

![chesscolockl soldering](https://github.com/user-attachments/assets/2a62bd80-1831-4c23-b995-200da9eb0a03)

### Design and print the casing
<!-- TODO: Add casing photo -->

The `chessclock-CAD/` folder includes all the STL files needed to print the casing and a gift box, as well as the Blender source files if you want to modify the designs.

### Finished product

![finished chessclokc](https://github.com/user-attachments/assets/34ca8def-3943-44aa-aa19-6c580db328a5)


https://github.com/user-attachments/assets/36cff737-d65f-408a-ad59-bc486b68077b



https://github.com/user-attachments/assets/38348769-685e-4487-b6a5-1dc7a97dfc77


## Pinout Configuration

### Player 1
```cpp
// Display (I2C1)
SCL = PB6
SDA = PB7

// Rotary Encoder
CH_A = PA0
CH_B = PA1

// Buttons
Encoder Push = PA2
Menu         = PA3
Back         = PA4
Tap          = PA15
```

### Player 2
```cpp
// Display (I2C2)
SCL = PB10
SDA = PB11

// Rotary Encoder
CH_A = PB12
CH_B = PB13

// Buttons
Encoder Push = PB14
Menu         = PB15
Back         = PA8
Tap          = PA9
```

### Buzzer
```cpp
Buzzer = PA6  // PWM via TIM3
```

## Key Features

### Game State Machine

The game operates in 4 phases:

```
                      ┌──── tap ───┐
                      │  (switch   │
                      │   turns)   │
                      v            │
 ARMED ──────────> RUNNING ───────┘
   │    first tap   │    │
   │                │    │ time expires
   │   menu hold    │    v
   │     (2s)       │  FINISHED
   v                v    │
  MENU            MENU   │ any tap
   │ (configure)    │    │
   │          ready │    │
   └──> ARMED <─────┘    │
          ^    or reset   │
          └───────────────┘
```

The menu can be entered from both **Armed** and **Running** by holding the menu button for 2 seconds. From Armed, it offers starting time and mode configuration. From Running, the game pauses and the menu additionally offers current time editing and reset.

- **Armed:** both players ready, waiting for first tap to start
- **Running:** active player's clock counting down, tap to switch turns
- **Paused:** clocks frozen, access menu to edit times or reset
- **Finished:** game over, loser's display blinks, buzzer beeps (if enabled)

### 6 Time Control Modes

| Mode | Behavior | Default Bonus |
|------|----------|---------------|
| **None** | Pure countdown | N/A |
| **Increment (Fischer)** | Add time per move, can accumulate | 3s |
| **Delay (Bronstein)** | Buffer before countdown starts | 2s |
| **Partial** | Add time per move, capped at starting time | 5s |
| **Limited** | Must move within X seconds or lose instantly | 30s |
| **Byo-yomi** | Extra period after main time expires | 30s |

### Independent Player Menus

Each player has their own menu accessible by holding their menu button for 2 seconds. All settings are fully independent per player -- Player 1 can play with 10 minutes + Fischer increment while Player 2 plays with 5 minutes + no bonus, for example.

**Armed Menu:**
1. Starting Time
2. Time Control (mode + bonus)
3. Buzzer (on/off)
4. Ready!

**Paused Menu:**
1. Current Time (live editing)
2. Time Control (mode + bonus)
3. Buzzer (on/off)
4. Reset (with confirmation)
5. Ready!

### Time Editor
- Large HH:MM:SS display with blinking field selection
- Rotate encoder to adjust values, push encoder to move between fields
- Hold encoder for 1 second to save
- Values loop within limits (0-23 hours, 0-59 minutes/seconds)

### Input System
- **Rotary Encoders:** quadrature decoding via EXTI interrupts, 20 detents per rotation
- **Button Debouncing:** 50ms settling time
- **Menu Hold Detection:** 2-second hold to enter menu
- **Stale Press Clearing:** button events flushed on state transitions

### Buzzer
- 3 beeps on game finish (500ms on, 250ms off pattern)
- Toggleable from either player's menu
- 8500 Hz PWM at 90% duty cycle (piezo resonant frequency)

### Boot Animation
4-frame splash screen displayed on both OLEDs at startup.

<!-- TODO: Add demo video -->

### Display
- Smart partial redraws to minimize flicker
- Display cache prevents redundant I2C writes
- Header shows mode name and bonus time
- Large clock digits (16x24 font)

## Technical Specifications

| Component | Specification |
|-----------|---------------|
| **MCU** | STM32F103C8 (ARM Cortex-M3) @ 8 MHz HSI |
| **Displays** | 2x SH1106 OLED 128x64, I2C @ 400 kHz |
| **Encoders** | Quadrature, 20 detents, 4 pulses/detent |
| **Clock Precision** | 100ms tick via TIM2 interrupt |
| **Time Range** | 1 second to 24 hours |
| **Bonus Range** | 0 to 60 seconds |
| **Buzzer** | Piezo, 8500 Hz PWM via TIM3 |
| **Upload** | Serial (UART bootloader) |

### Software Architecture

| Module | Purpose |
|--------|---------|
| `main.c` | Coordinator between game and menu modules |
| `game.c` | Game state machine, clock logic, input handling |
| `menu.c` | Menu navigation, time editing, mode selection |
| `display.c` | SH1106 OLED driver (I2C) |
| `hardware.c` | GPIO, I2C, timer initialization |
| `button.c` | Debouncing and edge detection |
| `encoder.c` | Quadrature decoding via interrupts |
| `config.h` | All pin definitions, constants, and types |
| `fonts.h` | Font data (5x7, 8x16, 16x24) |
| `splash.h` | Boot animation frame data |

### Coordinator Pattern
The game and menu modules are fully decoupled. `main.c` bridges them:
- Detects when a player enters the menu (game sets `in_menu` flag)
- Calls `menu_open()` with pointers to editable config/time values
- Relays menu actions (`READY`, `RESET`) back to the game module
- Syncs mode changes to update bonus times

## Build Environment

### PlatformIO Configuration
```ini
[env:genericSTM32F103C8]
platform = ststm32
board = genericSTM32F103C8
framework = stm32cube

build_flags =
    -DUSE_HAL_DRIVER
    -DSTM32F103xB

upload_protocol = serial
```

Upload via UART bootloader with default PlatformIO settings.

## Configuration

All parameters can be adjusted in `config.h`:

### Timing
```cpp
CLOCK_TICK_INTERVAL_MS        = 100      // decisecond precision
MENU_BUTTON_HOLD_TIME_MS      = 2000     // hold to enter menu
BUTTON_DEBOUNCE_TIME_MS       = 50       // button settling time
ENCODER_LONG_PRESS_TIME_MS    = 1000     // hold encoder to save
```

### Defaults
```cpp
DEFAULT_STARTING_TIME_MS      = 300000   // 5 minutes
DEFAULT_BONUS_INCREMENT_MS    = 3000     // 3 seconds
DEFAULT_BONUS_DELAY_MS        = 2000     // 2 seconds
DEFAULT_BONUS_PARTIAL_MS      = 5000     // 5 seconds
DEFAULT_BONUS_LIMITED_MS      = 30000    // 30 seconds
DEFAULT_BONUS_BYOYOMI_MS      = 30000    // 30 seconds
```

### Buzzer
```cpp
BUZZER_PWM_FREQUENCY_HZ       = 8500     // piezo resonant frequency
BUZZER_PWM_DUTY_CYCLE_PERCENT  = 90       // duty cycle
BUZZER_BEEP_DURATION_MS        = 500      // beep length
BUZZER_PAUSE_DURATION_MS       = 250      // pause between beeps
```

## Usage

### First Boot
1. Both displays show the splash animation
2. Game starts in Armed phase with default settings (5 min, no time control)
3. Both displays show "Ready!" with the clock at 05:00

### Starting a Game
1. Player 1 or Player 2 taps their button to start
2. The **other** player's clock begins counting down
3. Tap to switch turns -- the active player's clock stops and the opponent's starts

### During a Game
- **Tap:** switch turns (applies time control bonus to current player)
- **Hold Menu (2s):** pause the game and enter menu
- **Time expires:** game ends, loser's display blinks, buzzer beeps 3x

### Menu Navigation
- **Encoder rotation:** move cursor up/down or adjust values
- **Encoder push:** select item or advance to next field
- **Encoder long press (1s):** save in time editor
- **Back button:** return to previous screen



https://github.com/user-attachments/assets/ae7eb4f5-54fc-41e0-8311-3a073d8e431d



## License
MIT License - feel free to use, modify, and distribute.
