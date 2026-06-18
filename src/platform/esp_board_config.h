#ifndef ESP_BOARD_CONFIG_H
#define ESP_BOARD_CONFIG_H

/*
 * Central wiring map for the current ESP32 + E-Ink panel prototype.
 * Update this file first when the hardware wiring changes.
 */
#define ESP_EPD_PIN_BUSY 4
#define ESP_EPD_PIN_RST 16
#define ESP_EPD_PIN_DC 17
#define ESP_EPD_PIN_CS 5
#define ESP_EPD_PIN_SCK 18
#define ESP_EPD_PIN_SDA 23

#define ESP_BUTTON_PIN_POWER 0
#define ESP_BUTTON_PIN_UP 35
#define ESP_BUTTON_PIN_HOME 34
#define ESP_BUTTON_PIN_DOWN 39
#define ESP_BUTTON_ACTIVE_LEVEL 0
#define ESP_BUTTON_POLL_MS 20

#define ESP_EPD_SPI_HOST SPI2_HOST
#define ESP_EPD_SPI_INIT_HZ 2000000
#define ESP_EPD_SPI_REFRESH_HZ 4000000
#define ESP_EPD_RESET_LOW_MS 10
#define ESP_EPD_RESET_HIGH_MS 10
#define ESP_EPD_BUSY_TIMEOUT_MS 5000

#define ESP_EPD_POWER_GND "GND"
#define ESP_EPD_POWER_VCC "3V"

#endif
