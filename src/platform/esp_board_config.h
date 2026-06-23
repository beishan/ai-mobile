#ifndef ESP_BOARD_CONFIG_H
#define ESP_BOARD_CONFIG_H

/*
 * Central wiring map for the ESP32 + 4.26 inch 480x800 black/white E-Ink panel.
 * Target controller: SSD677 over SPI.
 * Update this file first when the hardware wiring changes.
 */
#define ESP_EPD_PANEL_NAME "4.26in 480x800 BW E-Ink"
#define ESP_EPD_DRIVER_IC "SSD677"
#define ESP_EPD_WIDTH 480
#define ESP_EPD_HEIGHT 800

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
#define ESP_BUTTON_DEBOUNCE_MS 60
#define ESP_BUTTON_LONG_PRESS_MS 1200

#define ESP_EPD_SPI_HOST SPI2_HOST
#define ESP_EPD_SPI_INIT_HZ 4000000
#define ESP_EPD_SPI_REFRESH_HZ 12000000
#define ESP_EPD_RESET_LOW_MS 10
#define ESP_EPD_RESET_HIGH_MS 10
#define ESP_EPD_BUSY_TIMEOUT_MS 5000

#define ESP_EPD_POWER_GND "GND"
#define ESP_EPD_POWER_VCC "3V"

#endif
