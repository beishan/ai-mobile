#include "platform/esp_sd.h"

#include <stdint.h>
#include <stdio.h>

#include "driver/gpio.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "platform/epd_frame.h"
#include "platform/esp_board_config.h"

static const char *TAG = "esp_sd";

static int keep_spi_devices_deselected(void) {
    gpio_config_t output = {
        .pin_bit_mask = (1ULL << ESP_SD_PIN_CS) | (1ULL << ESP_EPD_PIN_CS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    esp_err_t err = gpio_config(&output);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to configure SPI chip-select pins: %s", esp_err_to_name(err));
        return -1;
    }
    gpio_set_level(ESP_SD_PIN_CS, 1);
    gpio_set_level(ESP_EPD_PIN_CS, 1);
    return 0;
}

int esp_sd_init(esp_sd_t *sd) {
    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    sdspi_device_config_t device = SDSPI_DEVICE_CONFIG_DEFAULT();
    esp_vfs_fat_mount_config_t mount = {
        .format_if_mount_failed = false,
        .max_files = ESP_SD_MAX_OPEN_FILES,
        .allocation_unit_size = 16 * 1024
    };
    spi_bus_config_t bus = {
        .mosi_io_num = ESP_SD_PIN_MOSI,
        .miso_io_num = ESP_SD_PIN_MISO,
        .sclk_io_num = ESP_SD_PIN_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EPD_FRAME_BYTES
    };
    esp_err_t err;

    if (sd == NULL) {
        return -1;
    }
    sd->card = NULL;
    sd->mounted = 0;

    if (keep_spi_devices_deselected() != 0) {
        return -1;
    }

    host.slot = ESP_EPD_SPI_HOST;
    host.max_freq_khz = ESP_SD_SPI_HZ_KHZ;
    err = spi_bus_initialize(host.slot, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to initialize shared SPI bus: %s", esp_err_to_name(err));
        return -1;
    }

    device.gpio_cs = ESP_SD_PIN_CS;
    device.host_id = host.slot;
    err = esp_vfs_fat_sdspi_mount(ESP_SD_MOUNT_POINT, &host, &device, &mount, &sd->card);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "SD card unavailable (%s); continuing without external books",
                 esp_err_to_name(err));
        return -1;
    }

    sd->mounted = 1;
    ESP_LOGI(TAG, "mounted SD card at %s (CS=%d MOSI=%d CLK=%d MISO=%d)",
             ESP_SD_MOUNT_POINT,
             ESP_SD_PIN_CS,
             ESP_SD_PIN_MOSI,
             ESP_SD_PIN_CLK,
             ESP_SD_PIN_MISO);
    sdmmc_card_print_info(stdout, sd->card);
    return 0;
}

int esp_sd_is_mounted(const esp_sd_t *sd) {
    return sd != NULL && sd->mounted;
}
