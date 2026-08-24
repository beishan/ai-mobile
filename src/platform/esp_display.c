#include "platform/esp_display.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/esp_board_config.h"
#include "platform/epd_frame.h"
#include "platform/epd_controller.h"
#include "platform/epd_panel.h"

static const char *TAG = "esp_display";
static epd_frame_t packed_frame;

static void esp_display_remember_area(esp_display_t *display,
                                      int native_x, int native_y,
                                      int native_width, int native_height) {
    const int row_bytes = EPD_NATIVE_WIDTH / 8;
    int byte_x;
    int byte_width;

    if (display == NULL || display->previous_frame == NULL ||
        native_x < 0 || native_y < 0 ||
        native_width <= 0 || native_height <= 0) {
        return;
    }
    byte_x = native_x / 8;
    byte_width = native_width / 8;
    for (int y = native_y; y < native_y + native_height; y++) {
        int offset = y * row_bytes + byte_x;
        memcpy(display->previous_frame + offset,
               packed_frame.bw + offset,
               (size_t)byte_width);
    }
}

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
    size_t shared_bus_max_transfer = 0;
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
    esp_err_t err;

    if (spi_bus_get_max_transaction_len(ESP_EPD_SPI_HOST,
                                        &shared_bus_max_transfer) == ESP_OK) {
        ESP_LOGI(TAG, "reusing shared SPI bus (max transfer=%u bytes)",
                 (unsigned int)shared_bus_max_transfer);
    } else {
        err = spi_bus_initialize(ESP_EPD_SPI_HOST, &bus, SPI_DMA_CH_AUTO);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to initialize EPD SPI bus: %s", esp_err_to_name(err));
            return -1;
        }
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
    display->previous_frame = heap_caps_malloc(
        EPD_FRAME_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (display->previous_frame == NULL) {
        display->previous_frame = heap_caps_malloc(
            EPD_FRAME_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    display->previous_frame_valid = 0;
    display->hardware_ready = 0;
    display->energy_saving_level = 0;
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
        /*
         * Do not wait for BUSY immediately after the hardware reset.  A panel
         * waking from deep sleep may keep BUSY active until it receives the
         * SSD1677 software-reset command (0x12).  ssd1677_init() sends that
         * command first and then performs the required BUSY wait.
         */
        if (esp_display_reset(display) != 0) {
            display->hardware_ready = 0;
            ESP_LOGW(TAG, "EPD hardware reset failed");
        } else {
            epd_controller_io_t io = {
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
            if (epd_controller_init(&display->controller, &io) != 0) {
                display->hardware_ready = 0;
                ESP_LOGE(TAG, "%s controller initialization failed", EPD_DRIVER_NAME);
            } else {
                ESP_LOGI(TAG, "%s controller initialized", EPD_DRIVER_NAME);
            }
        }
    }
    if (display->previous_frame == NULL) {
        ESP_LOGW(TAG, "automatic dirty-region tracking unavailable: no 48KB frame cache");
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
    int initial_level;
    if (display == NULL || !display->hardware_ready) {
        return -1;
    }
    initial_level = gpio_get_level(ESP_EPD_PIN_BUSY);
    ESP_LOGI(TAG, "EPD BUSY wait: level=%d active=%d timeout=%dms",
             initial_level, ESP_EPD_BUSY_ACTIVE_LEVEL, timeout_ms);
    while (gpio_get_level(ESP_EPD_PIN_BUSY) == ESP_EPD_BUSY_ACTIVE_LEVEL) {
        if (elapsed_ms >= timeout_ms) {
            ESP_LOGE(TAG, "EPD BUSY timeout after %dms (level=%d)",
                     timeout_ms, gpio_get_level(ESP_EPD_PIN_BUSY));
            return -1;
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        elapsed_ms += 10;
    }
    ESP_LOGI(TAG, "EPD BUSY ready after %dms (level=%d)",
             elapsed_ms, gpio_get_level(ESP_EPD_PIN_BUSY));
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
        epd_controller_io_t io = display->controller.io;
        if (esp_display_reset(display) != 0 ||
            epd_controller_init(&display->controller, &io) != 0) {
            ESP_LOGE(TAG, "failed to wake and reinitialize %s", EPD_DRIVER_NAME);
            return -1;
        }
    }
    if (epd_controller_present(&display->controller, packed_frame.bw, sizeof(packed_frame.bw)) != 0) {
        ESP_LOGE(TAG, "%s frame transfer or refresh failed", EPD_DRIVER_NAME);
        return -1;
    }

    display->refresh_count++;
    display->partial_since_full = 0;
    display->partial_area_since_full = 0;
    if (display->previous_frame != NULL) {
        memcpy(display->previous_frame, packed_frame.bw, EPD_FRAME_BYTES);
        display->previous_frame_valid = 1;
    }
    ESP_LOGI(TAG, "present %s BW frame %d: bytes=%d black=%d bw_sum=%08x",
             EPD_DRIVER_NAME,
             display->refresh_count,
             EPD_FRAME_BYTES,
             black,
             black_checksum);
    return 0;
}

int esp_display_present_auto(esp_display_t *display, const gfx_framebuffer_t *fb) {
    int native_x;
    int native_y;
    int native_width;
    int native_height;
    int ui_x;
    int ui_y;
    int ui_width;
    int ui_height;
    int diff_result;
    uint32_t dirty_area;
    uint32_t full_area;
    size_t changed_bytes = 0;
    epd_frame_t *previous;

    if (display == NULL || fb == NULL) {
        return -1;
    }
    if (display->previous_frame == NULL || !display->previous_frame_valid) {
        return esp_display_present(display, fb);
    }
    if (epd_frame_pack(fb, &packed_frame) != 0) {
        return -1;
    }

    previous = (epd_frame_t *)display->previous_frame;
    diff_result = epd_frame_diff_bounds(previous, &packed_frame,
                                        &native_x, &native_y,
                                        &native_width, &native_height,
                                        &changed_bytes);
    if (diff_result < 0) {
        return -1;
    }
    if (diff_result == 0) {
        ESP_LOGD(TAG, "automatic refresh skipped: framebuffer unchanged");
        return 0;
    }

    dirty_area = (uint32_t)native_width * (uint32_t)native_height;
    full_area = (uint32_t)EPD_NATIVE_WIDTH * (uint32_t)EPD_NATIVE_HEIGHT;
    if (dirty_area * 100u >= full_area * ESP_EPD_AUTO_FULL_AREA_PERCENT) {
        ESP_LOGI(TAG,
                 "automatic refresh selected full update: area=%u/%u changed_bytes=%u",
                 (unsigned int)dirty_area, (unsigned int)full_area,
                 (unsigned int)changed_bytes);
        return esp_display_present(display, fb);
    }

#if EPD_NATIVE_PORTRAIT
    ui_x = native_x * GFX_WIDTH / EPD_NATIVE_WIDTH;
    ui_y = native_y * GFX_HEIGHT / EPD_NATIVE_HEIGHT;
    ui_width = (native_width * GFX_WIDTH + EPD_NATIVE_WIDTH - 1) / EPD_NATIVE_WIDTH;
    ui_height = (native_height * GFX_HEIGHT + EPD_NATIVE_HEIGHT - 1) / EPD_NATIVE_HEIGHT;
#else
    /* Convert the native 800x480 diff box back to portrait UI coordinates. */
    ui_x = EPD_NATIVE_HEIGHT - (native_y + native_height);
    ui_y = native_x;
    ui_width = native_height;
    ui_height = native_width;
#endif
    ESP_LOGD(TAG,
             "automatic refresh selected partial update: ui=(%d,%d %dx%d) changed_bytes=%u",
             ui_x, ui_y, ui_width, ui_height, (unsigned int)changed_bytes);
    return esp_display_present_partial(display, fb, ui_x, ui_y, ui_width, ui_height);
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
        int refresh_limit = ESP_EPD_PARTIAL_REFRESH_LIMIT;
        int area_screens = ESP_EPD_PARTIAL_AREA_SCREENS;
        if (display->energy_saving_level >= 2) {
            refresh_limit *= 2;
            area_screens *= 2;
        }
        uint32_t area_limit = (uint32_t)GFX_WIDTH * (uint32_t)GFX_HEIGHT *
                              (uint32_t)area_screens;
        if (display->partial_since_full >= refresh_limit ||
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

#if EPD_NATIVE_PORTRAIT
    native_x = x * EPD_NATIVE_WIDTH / GFX_WIDTH;
    native_y = y * EPD_NATIVE_HEIGHT / GFX_HEIGHT;
    x_end = (x_end * EPD_NATIVE_WIDTH + GFX_WIDTH - 1) / GFX_WIDTH;
    y_end = (y_end * EPD_NATIVE_HEIGHT + GFX_HEIGHT - 1) / GFX_HEIGHT;
    native_width = x_end - native_x;
    native_height = y_end - native_y;
#else
    /* Portrait UI is rotated 180 degrees into SSD1677 native RAM (800x480). */
    native_x = y;
    native_y = EPD_NATIVE_HEIGHT - x_end;
    native_width = y_end - y;
    native_height = x_end - x;
#endif

    /* SSD1677 source windows are byte-addressed: expand to whole bytes. */
    x_end = native_x + native_width;
    native_x &= ~7;
    x_end = (x_end + 7) & ~7;
    if (x_end > EPD_NATIVE_WIDTH) x_end = EPD_NATIVE_WIDTH;
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
        epd_controller_io_t io = display->controller.io;
        if (esp_display_reset(display) != 0 ||
            epd_controller_init(&display->controller, &io) != 0) {
            ESP_LOGE(TAG, "failed to wake %s for partial refresh", EPD_DRIVER_NAME);
            return -1;
        }
    }
    if (epd_controller_present_partial(&display->controller, packed_frame.bw,
                                sizeof(packed_frame.bw), native_x, native_y,
                                native_width, native_height) != 0) {
        ESP_LOGE(TAG, "%s partial transfer or refresh failed", EPD_DRIVER_NAME);
        return -1;
    }

    display->refresh_count++;
    display->partial_refresh_count++;
    display->partial_since_full++;
    display->partial_area_since_full += (uint32_t)ui_width * (uint32_t)ui_height;
    if (display->previous_frame != NULL && display->previous_frame_valid) {
        esp_display_remember_area(display, native_x, native_y,
                                  native_width, native_height);
    }
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
    if (epd_controller_sleep(&display->controller) != 0) {
        ESP_LOGE(TAG, "%s deep sleep command failed", EPD_DRIVER_NAME);
        return;
    }
    display->previous_frame_valid = 0;
    ESP_LOGI(TAG, "%s entered deep sleep after %d frame(s)", EPD_DRIVER_NAME, display->refresh_count);
}

void esp_display_set_energy_saving(esp_display_t *display, int level) {
    if (display == NULL) return;
    if (level < 0) level = 0;
    if (level > 2) level = 2;
    display->energy_saving_level = level;
}
