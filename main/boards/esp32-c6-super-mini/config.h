#ifndef _BOARD_CONFIG_H_
#define _BOARD_CONFIG_H_

#include <driver/gpio.h>

#define AUDIO_INPUT_SAMPLE_RATE  16000
#define AUDIO_OUTPUT_SAMPLE_RATE 16000

// ESP32-C6 has one I2S peripheral, so INMP441 and MAX98357 share BCLK/LRCK.
#define AUDIO_I2S_GPIO_BCLK GPIO_NUM_6
#define AUDIO_I2S_GPIO_WS   GPIO_NUM_7
#define AUDIO_I2S_GPIO_DOUT GPIO_NUM_21
#define AUDIO_I2S_GPIO_DIN  GPIO_NUM_20

#define BUILTIN_LED_GPIO GPIO_NUM_15
#define RGB_LED_GPIO GPIO_NUM_8
#define BOOT_BUTTON_GPIO GPIO_NUM_9
#define CHAT_BUTTON_GPIO GPIO_NUM_4 // Tombol tambahan untuk listening / chat
// BAT+ / BAT- only powers and charges the board. The charger status LED is
// handled by the charger IC and is not readable by firmware unless its CHG/STAT
// signal is wired to an ESP32 GPIO.
#define BUILTIN_LED_ACTIVE_LEVEL 1
// Default wiring target: solder charger CHG/STAT (active-low) to GPIO2.
// If you have not wired CHG/STAT to a GPIO yet, this will read as not charging.
#define BATTERY_CHARGING_GPIO GPIO_NUM_NC
#define BATTERY_CHARGING_ACTIVE_LEVEL 0

// Soft power-off uses deep sleep. ESP32-C6 can only wake from deep sleep through
// RTC-capable GPIOs on this board, so wire a separate wake button to one of
// GPIO0..GPIO7 and set this to that pin. BOOT on GPIO9 is not a deep-sleep wake pin.
#define DEEP_SLEEP_WAKE_GPIO GPIO_NUM_NC
#define DEEP_SLEEP_WAKE_ACTIVE_LEVEL 0

#define DISPLAY_SDA_PIN GPIO_NUM_2 // SPI MOSI
#define DISPLAY_SCL_PIN GPIO_NUM_3 // SPI SCLK
#define DISPLAY_CS_PIN  GPIO_NUM_0 // SPI CS
#define DISPLAY_DC_PIN  GPIO_NUM_1 // SPI DC
#define DISPLAY_RES_PIN GPIO_NUM_22 // SPI RES
#define DISPLAY_BLK_PIN GPIO_NUM_23 // SPI BLK

#define DISPLAY_WIDTH   240
#define DISPLAY_HEIGHT  280
#define DISPLAY_OFFSET_X 0
#define DISPLAY_OFFSET_Y 20
#define DISPLAY_MIRROR_X false
#define DISPLAY_MIRROR_Y false
#define DISPLAY_SWAP_XY false

// Relay GPIOs
#define RELAY_CH1_GPIO GPIO_NUM_18
#define RELAY_CH2_GPIO GPIO_NUM_19

#endif // _BOARD_CONFIG_H_
