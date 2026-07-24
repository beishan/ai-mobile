#ifndef ESP_DISPLAY_H
#define ESP_DISPLAY_H

#include "driver/spi_master.h"
#include "gfx/gfx.h"
#include "platform/ssd1677.h"
#include <stdint.h>

typedef struct {
    int refresh_count;
    int partial_refresh_count;
    int partial_since_full;
    uint32_t partial_area_since_full;
    uint8_t *previous_frame;
    int previous_frame_valid;
    int hardware_ready;
    spi_device_handle_t spi;
    ssd1677_t controller;
} esp_display_t;

void esp_display_init(esp_display_t *display);
int esp_display_reset(esp_display_t *display);
int esp_display_wait_busy(esp_display_t *display, int timeout_ms);
int esp_display_send_command(esp_display_t *display, unsigned char command);
int esp_display_send_data(esp_display_t *display, const unsigned char *data, int length);
int esp_display_present(esp_display_t *display, const gfx_framebuffer_t *fb);
/* Compare against the last successful frame and automatically skip, partially
 * refresh, or fully refresh the panel. */
int esp_display_present_auto(esp_display_t *display, const gfx_framebuffer_t *fb);
int esp_display_present_partial(esp_display_t *display, const gfx_framebuffer_t *fb,
                                int x, int y, int width, int height);
void esp_display_sleep(esp_display_t *display);

#endif
