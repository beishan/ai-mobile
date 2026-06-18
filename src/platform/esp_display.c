#include "platform/esp_display.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/esp_board_config.h"

static const char *TAG = "esp_display";

static int esp_display_configure_gpio(void) {
    gpio_config_t output = {
        .pin_bit_mask = (1ULL << ESP_EPD_PIN_CS) | (1ULL << ESP_EPD_PIN_DC) | (1ULL << ESP_EPD_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config_t input = {
        .pin_bit_mask = (1ULL << ESP_EPD_PIN_BUSY),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    esp_err_t err = gpio_config(&output);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure EPD output pins: %s", esp_err_to_name(err));
        return -1;
    }
    err = gpio_config(&input);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure EPD busy pin: %s", esp_err_to_name(err));
        return -1;
    }

    gpio_set_level(ESP_EPD_PIN_CS, 1);
    gpio_set_level(ESP_EPD_PIN_DC, 0);
    gpio_set_level(ESP_EPD_PIN_RST, 1);
    return 0;
}

static int esp_display_configure_spi(esp_display_t *display) {
    spi_bus_config_t bus = {
        .mosi_io_num = ESP_EPD_PIN_SDA,
        .miso_io_num = -1,
        .sclk_io_num = ESP_EPD_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GFX_WIDTH * GFX_HEIGHT / 8
    };
    spi_device_interface_config_t device = {
        .clock_speed_hz = ESP_EPD_SPI_INIT_HZ,
        .mode = 0,
        .spics_io_num = ESP_EPD_PIN_CS,
        .queue_size = 1
    };
    esp_err_t err = spi_bus_initialize(ESP_EPD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to initialize EPD SPI bus: %s", esp_err_to_name(err));
        return -1;
    }

    err = spi_bus_add_device(ESP_EPD_SPI_HOST, &device, &display->spi);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to add EPD SPI device: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

void esp_display_init(esp_display_t *display) {
    if (display == NULL) {
        return;
    }
    display->refresh_count = 0;
    display->hardware_ready = 0;
    display->spi = NULL;
    ESP_LOGI(TAG, "display adapter initialized for 400x300 tri-color framebuffer");
    ESP_LOGI(TAG, "EPD pins: BUSY=%d RST=%d DC=%d CS=%d SCK=%d SDA=%d VCC=%s GND=%s",
             ESP_EPD_PIN_BUSY,
             ESP_EPD_PIN_RST,
             ESP_EPD_PIN_DC,
             ESP_EPD_PIN_CS,
             ESP_EPD_PIN_SCK,
             ESP_EPD_PIN_SDA,
             ESP_EPD_POWER_VCC,
             ESP_EPD_POWER_GND);
    ESP_LOGI(TAG, "EPD SPI host=%d init_hz=%d refresh_hz=%d",
             ESP_EPD_SPI_HOST,
             ESP_EPD_SPI_INIT_HZ,
             ESP_EPD_SPI_REFRESH_HZ);

    if (esp_display_configure_gpio() == 0 && esp_display_configure_spi(display) == 0) {
        display->hardware_ready = 1;
        ESP_LOGI(TAG, "EPD GPIO and SPI bus initialized");
        if (esp_display_reset(display) != 0 || esp_display_wait_busy(display, ESP_EPD_BUSY_TIMEOUT_MS) != 0) {
            display->hardware_ready = 0;
            ESP_LOGW(TAG, "EPD basic reset/busy handshake failed");
        }
    }
}

int esp_display_reset(esp_display_t *display) {
    if (display == NULL || !display->hardware_ready) {
        return -1;
    }
    gpio_set_level(ESP_EPD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(ESP_EPD_RESET_LOW_MS));
    gpio_set_level(ESP_EPD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(ESP_EPD_RESET_HIGH_MS));
    ESP_LOGI(TAG, "EPD reset pulse complete");
    return 0;
}

int esp_display_wait_busy(esp_display_t *display, int timeout_ms) {
    int elapsed_ms = 0;
    if (display == NULL || !display->hardware_ready) {
        return -1;
    }
    while (gpio_get_level(ESP_EPD_PIN_BUSY) != 0) {
        if (elapsed_ms >= timeout_ms) {
            ESP_LOGE(TAG, "EPD busy timeout after %d ms", timeout_ms);
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed_ms += 10;
    }
    return 0;
}

static int esp_display_spi_write(esp_display_t *display, int dc, const unsigned char *data, int length) {
    spi_transaction_t transaction = {0};
    esp_err_t err;
    if (display == NULL || !display->hardware_ready || display->spi == NULL || data == NULL || length <= 0) {
        return -1;
    }

    gpio_set_level(ESP_EPD_PIN_DC, dc);
    transaction.length = length * 8;
    transaction.tx_buffer = data;
    err = spi_device_transmit(display->spi, &transaction);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "EPD SPI write failed: %s", esp_err_to_name(err));
        return -1;
    }
    return 0;
}

int esp_display_send_command(esp_display_t *display, unsigned char command) {
    return esp_display_spi_write(display, 0, &command, 1);
}

int esp_display_send_data(esp_display_t *display, const unsigned char *data, int length) {
    return esp_display_spi_write(display, 1, data, length);
}

int esp_display_present(esp_display_t *display, const gfx_framebuffer_t *fb) {
    int black = 0;
    int red = 0;
    if (display == NULL || fb == NULL) {
        return -1;
    }

    for (int y = 0; y < gfx_height(fb); y++) {
        for (int x = 0; x < gfx_width(fb); x++) {
            gfx_color_t pixel = gfx_get_pixel(fb, x, y);
            if (pixel == GFX_BLACK) {
                black++;
            } else if (pixel == GFX_RED) {
                red++;
            }
        }
    }

    display->refresh_count++;
    ESP_LOGI(TAG, "present frame %d: black=%d red=%d hardware_ready=%d",
             display->refresh_count,
             black,
             red,
             display->hardware_ready);
    return 0;
}

void esp_display_sleep(esp_display_t *display) {
    if (display == NULL) {
        return;
    }
    ESP_LOGI(TAG, "display sleep requested after %d frame(s)", display->refresh_count);
}
