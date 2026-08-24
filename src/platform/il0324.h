#ifndef IL0324_H
#define IL0324_H

#include <stddef.h>
#include <stdint.h>

#define IL0324_PANEL_WIDTH 240
#define IL0324_PANEL_HEIGHT 416
#define IL0324_FRAME_BYTES ((IL0324_PANEL_WIDTH * IL0324_PANEL_HEIGHT) / 8)

typedef struct {
    void *context;
    int (*write_command)(void *context, uint8_t command);
    int (*write_data)(void *context, const uint8_t *data, size_t length);
    int (*wait_busy)(void *context, int timeout_ms);
    void (*delay_ms)(void *context, int delay_ms);
} il0324_io_t;

typedef struct {
    il0324_io_t io;
    int initialized;
    int sleeping;
} il0324_t;

int il0324_init(il0324_t *controller, const il0324_io_t *io);
int il0324_present(il0324_t *controller, const uint8_t *frame, size_t length);
int il0324_present_partial(il0324_t *controller, const uint8_t *frame, size_t length,
                           int x, int y, int width, int height);
int il0324_sleep(il0324_t *controller);

#endif
