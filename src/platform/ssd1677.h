#ifndef SSD1677_H
#define SSD1677_H

#include <stddef.h>
#include <stdint.h>

#define SSD1677_PANEL_WIDTH 800
#define SSD1677_PANEL_HEIGHT 480
#define SSD1677_FRAME_BYTES ((SSD1677_PANEL_WIDTH * SSD1677_PANEL_HEIGHT) / 8)

typedef struct {
    void *context;
    int (*write_command)(void *context, uint8_t command);
    int (*write_data)(void *context, const uint8_t *data, size_t length);
    int (*wait_busy)(void *context, int timeout_ms);
    void (*delay_ms)(void *context, int delay_ms);
} ssd1677_io_t;

typedef struct {
    ssd1677_io_t io;
    int initialized;
    int sleeping;
} ssd1677_t;

int ssd1677_init(ssd1677_t *controller, const ssd1677_io_t *io);
int ssd1677_present(ssd1677_t *controller, const uint8_t *frame, size_t length);
int ssd1677_present_partial(ssd1677_t *controller, const uint8_t *frame, size_t length,
                            int x, int y, int width, int height);
int ssd1677_sleep(ssd1677_t *controller);

#endif
