#include "platform/esp_display.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/esp_board_config.h"
#include "platform/epd_frame.h"
#include "platform/ssd1677.h"

static const char *TAG = "esp_display";
static epd_frame_t packed_frame;

static int esp_display_controller_write_command(void *context, uint8_t command);
static int esp_display_controller_write_data(void *context, const uint8_t *data, size_t length);
static int esp_display_controller_wait_busy(void *context, int timeout_ms);
static void esp_display_controller_delay_ms(void *context, int delay_ms);

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
        .miso_io_num = ESP_SD_PIN_MISO,
        .sclk_io_num = ESP_EPD_PIN_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = GFX_WIDTH * GFX_HEIGHT / 8
    };
    spi_device_interface_config_t device = {
        .clock_speed_hz = ESP_EPD_SPI_HZ,
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
    display->partial_refresh_count = 0;
    display->partial_since_full = 0;
    display->partial_area_since_full = 0;
    display->hardware_ready = 0;
    display->spi = NULL;
    ESP_LOGI(TAG, "display adapter initialized for %s controller=%s framebuffer=%dx%d",
             ESP_EPD_PANEL_NAME,
             ESP_EPD_DRIVER_IC,
             GFX_WIDTH,
             GFX_HEIGHT);
    ESP_LOGI(TAG, "EPD pins: BUSY=%d RST=%d DC=%d CS=%d SCK=%d SDA=%d VCC=%s GND=%s",
             ESP_EPD_PIN_BUSY,
             ESP_EPD_PIN_RST,
             ESP_EPD_PIN_DC,
             ESP_EPD_PIN_CS,
             ESP_EPD_PIN_SCK,
             ESP_EPD_PIN_SDA,
             ESP_EPD_POWER_VCC,
             ESP_EPD_POWER_GND);
    ESP_LOGI(TAG, "EPD SPI host=%d hz=%d",
             ESP_EPD_SPI_HOST,
             ESP_EPD_SPI_HZ);

    if (esp_display_configure_gpio() == 0 && esp_display_configure_spi(display) == 0) {
        display->hardware_ready = 1;
        ESP_LOGI(TAG, "EPD GPIO and SPI bus initialized");
        if (esp_display_reset(display) != 0 || esp_display_wait_busy(display, ESP_EPD_BUSY_TIMEOUT_MS) != 0) {
            display->hardware_ready = 0;
            ESP_LOGW(TAG, "EPD basic reset/busy handshake failed");
        } else {
            ssd1677_io_t io = {
                .context = display,
                .write_command = NULL,
                .write_data = NULL,
                .wait_busy = NULL,
                .delay_ms = NULL
            };
            io.write_command = esp_display_controller_write_command;
            io.write_data = esp_display_controller_write_data;
            io.wait_busy = esp_display_controller_wait_busy;
            io.delay_ms = esp_display_controller_delay_ms;
            if (ssd1677_init(&display->controller, &io) != 0) {
                display->hardware_ready = 0;
                ESP_LOGE(TAG, "SSD1677 controller initialization failed");
            } else {
                ESP_LOGI(TAG, "SSD1677 controller initialized");
            }
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
    while (gpio_get_level(ESP_EPD_PIN_BUSY) == ESP_EPD_BUSY_ACTIVE_LEVEL) {
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
    const int chunk_size = 4096;
    int offset = 0;
    if (data == NULL || length <= 0) {
        return -1;
    }
    while (offset < length) {
        int remaining = length - offset;
        int chunk = remaining < chunk_size ? remaining : chunk_size;
        if (esp_display_spi_write(display, 1, data + offset, chunk) != 0) {
            return -1;
        }
        offset += chunk;
    }
    return 0;
}

static int esp_display_controller_write_command(void *context, uint8_t command) {
    return esp_display_send_command((esp_display_t *)context, command);
}

static int esp_display_controller_write_data(void *context, const uint8_t *data, size_t length) {
    if (length > INT_MAX) {
        return -1;
    }
    return esp_display_send_data((esp_display_t *)context, data, (int)length);
}

static int esp_display_controller_wait_busy(void *context, int timeout_ms) {
    return esp_display_wait_busy((esp_display_t *)context, timeout_ms);
}

static void esp_display_controller_delay_ms(void *context, int delay_ms) {
    (void)context;
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

int esp_display_present(esp_display_t *display, const gfx_framebuffer_t *fb) {
    int black = 0;
    unsigned int black_checksum = 0;
    if (display == NULL || fb == NULL) {
        return -1;
    }

    if (epd_frame_pack(fb, &packed_frame) != 0) {
        return -1;
    }

    for (int i = 0; i < EPD_FRAME_BYTES; i++) {
        unsigned char bw_byte = packed_frame.bw[i];
        black_checksum = (black_checksum * 33u) ^ bw_byte;
        for (int bit = 0; bit < 8; bit++) {
            unsigned char mask = (unsigned char)(0x80u >> bit);
            if ((bw_byte & mask) == 0) {
                black++;
            }
        }
    }

    if (!display->hardware_ready) {
        ESP_LOGE(TAG, "cannot present frame: display hardware is not ready");
        return -1;
    }
    if (display->controller.sleeping || !display->controller.initialized) {
        ssd1677_io_t io = display->controller.io;
        if (esp_display_reset(display) != 0 ||
            ssd1677_init(&display->controller, &io) != 0) {
            ESP_LOGE(TAG, "failed to wake and reinitialize SSD1677");
            return -1;
        }
    }
    if (ssd1677_present(&display->controller, packed_frame.bw, sizeof(packed_frame.bw)) != 0) {
        ESP_LOGE(TAG, "SSD1677 frame transfer or refresh failed");
        return -1;
    }

    display->refresh_count++;
    display->partial_since_full = 0;
    display->partial_area_since_full = 0;
    ESP_LOGI(TAG, "present SSD1677 BW frame %d: bytes=%d black=%d bw_sum=%08x",
             display->refresh_count,
             EPD_FRAME_BYTES,
             black,
             black_checksum);
    return 0;
}

int esp_display_present_partial(esp_display_t *display, const gfx_framebuffer_t *fb,
                                int x, int y, int width, int height) {
    int x_end;
    int y_end;
    int native_x;
    int native_y;
    int native_width;
    int native_height;
    int ui_width;
    int ui_height;

    if (display == NULL || fb == NULL || width <= 0 || height <= 0) {
        return -1;
    }

    /* Clip the dirty rectangle in portrait UI coordinates. */
    x_end = x + width;
    y_end = y + height;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x_end > GFX_WIDTH) x_end = GFX_WIDTH;
    if (y_end > GFX_HEIGHT) y_end = GFX_HEIGHT;
    if (x >= x_end || y >= y_end) {
        return -1;
    }
    ui_width = x_end - x;
    ui_height = y_end - y;

#if ESP_EPD_PARTIAL_REFRESH_LIMIT > 0
    {
        uint32_t update_area = (uint32_t)ui_width * (uint32_t)ui_height;
        uint32_t area_limit = (uint32_t)GFX_WIDTH * (uint32_t)GFX_HEIGHT *
                              (uint32_t)ESP_EPD_PARTIAL_AREA_SCREENS;
        if (display->partial_since_full >= ESP_EPD_PARTIAL_REFRESH_LIMIT ||
            display->partial_area_since_full + update_area >= area_limit) {
            ESP_LOGI(TAG,
                     "adaptive partial limit reached: count=%d area=%u/%u; full refresh",
                     display->partial_since_full,
                     (unsigned int)display->partial_area_since_full,
                     (unsigned int)area_limit);
            return esp_display_present(display, fb);
        }
    }
#endif

    /* Portrait UI is rotated 180 degrees into SSD1677 native RAM (800x480). */
    native_x = y;
    native_y = SSD1677_PANEL_HEIGHT - x_end;
    native_width = y_end - y;
    native_height = x_end - x;

    /* SSD1677 source windows are byte-addressed: expand to whole bytes. */
    x_end = native_x + native_width;
    native_x &= ~7;
    x_end = (x_end + 7) & ~7;
    if (x_end > SSD1677_PANEL_WIDTH) x_end = SSD1677_PANEL_WIDTH;
    native_width = x_end - native_x;

    if (epd_frame_pack_partial(fb, &packed_frame,
                               native_x, native_y,
                               native_width, native_height) != 0) {
        return -1;
    }
    if (!display->hardware_ready) {
        ESP_LOGE(TAG, "cannot present partial frame: display hardware is not ready");
        return -1;
    }
    if (display->controller.sleeping || !display->controller.initialized) {
        ssd1677_io_t io = display->controller.io;
        if (esp_display_reset(display) != 0 ||
            ssd1677_init(&display->controller, &io) != 0) {
            ESP_LOGE(TAG, "failed to wake SSD1677 for partial refresh");
            return -1;
        }
    }
    if (ssd1677_present_partial(&display->controller, packed_frame.bw,
                                sizeof(packed_frame.bw), native_x, native_y,
                                native_width, native_height) != 0) {
        ESP_LOGE(TAG, "SSD1677 partial transfer or refresh failed");
        return -1;
    }

    display->refresh_count++;
    display->partial_refresh_count++;
    display->partial_since_full++;
    display->partial_area_since_full += (uint32_t)ui_width * (uint32_t)ui_height;
    ESP_LOGD(TAG,
             "partial frame %d (partial total=%d, since full=%d/%d): ui=(%d,%d %dx%d) native=(%d,%d %dx%d) bytes=%d",
             display->refresh_count, display->partial_refresh_count,
             display->partial_since_full, ESP_EPD_PARTIAL_REFRESH_LIMIT,
             x, y, ui_width, ui_height,
             native_x, native_y, native_width, native_height,
             (native_width / 8) * native_height);
    return 0;
}

void esp_display_sleep(esp_display_t *display) {
    if (display == NULL) {
        return;
    }
    if (!display->hardware_ready) {
        ESP_LOGW(TAG, "display sleep ignored: hardware is not ready");
        return;
    }
    if (ssd1677_sleep(&display->controller) != 0) {
        ESP_LOGE(TAG, "SSD1677 deep sleep command failed");
        return;
    }
    ESP_LOGI(TAG, "SSD1677 entered deep sleep after %d frame(s)", display->refresh_count);
}
