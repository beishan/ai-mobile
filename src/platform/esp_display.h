#ifndef ESP_DISPLAY_H
#define ESP_DISPLAY_H

#include "driver/spi_master.h"
#include "gfx/gfx.h"

typedef struct {
    int refresh_count;
    int hardware_ready;
    spi_device_handle_t spi;
} esp_display_t;

void esp_display_init(esp_display_t *display);
int esp_display_reset(esp_display_t *display);
int esp_display_wait_busy(esp_display_t *display, int timeout_ms);
int esp_display_send_command(esp_display_t *display, unsigned char command);
int esp_display_send_data(esp_display_t *display, const unsigned char *data, int length);
int esp_display_present(esp_display_t *display, const gfx_framebuffer_t *fb);
void esp_display_sleep(esp_display_t *display);

#endif
