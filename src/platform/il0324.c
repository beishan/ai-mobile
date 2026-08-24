#include "platform/il0324.h"

#include <string.h>

#define IL0324_BUSY_TIMEOUT_MS 60000

static int command(il0324_t *c, uint8_t value) {
    return c->io.write_command(c->io.context, value);
}

static int data(il0324_t *c, const uint8_t *values, size_t length) {
    return c->io.write_data(c->io.context, values, length);
}

static int command_data(il0324_t *c, uint8_t cmd,
                        const uint8_t *values, size_t length) {
    return command(c, cmd) != 0 || (length != 0 && data(c, values, length) != 0) ? -1 : 0;
}

static int wait_busy(il0324_t *c) {
    return c->io.wait_busy(c->io.context, IL0324_BUSY_TIMEOUT_MS);
}

static int set_window(il0324_t *c, int x, int y, int width, int height) {
    uint8_t window[7];
    int x_end;
    int y_end;
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > IL0324_PANEL_WIDTH || y + height > IL0324_PANEL_HEIGHT ||
        (x & 7) != 0 || (width & 7) != 0) {
        return -1;
    }
    x_end = x + width - 1;
    y_end = y + height - 1;
    window[0] = (uint8_t)x;
    window[1] = (uint8_t)x_end;
    window[2] = (uint8_t)(y >> 8);
    window[3] = (uint8_t)y;
    window[4] = (uint8_t)(y_end >> 8);
    window[5] = (uint8_t)y_end;
    window[6] = 0x01;
    return command_data(c, 0x90, window, sizeof(window));
}

static int power_on(il0324_t *c) {
    return command(c, 0x04) != 0 || wait_busy(c) != 0 ? -1 : 0;
}

static int power_off(il0324_t *c) {
    static const uint8_t sequence[] = {0x30};
    return command_data(c, 0x03, sequence, sizeof(sequence)) != 0 ||
           command(c, 0x02) != 0 || wait_busy(c) != 0 ? -1 : 0;
}

static int write_area(il0324_t *c, uint8_t ram_command, const uint8_t *frame,
                      int x, int y, int width, int height) {
    const int panel_row_bytes = IL0324_PANEL_WIDTH / 8;
    const int row_bytes = width / 8;
    if (command(c, 0x91) != 0 || set_window(c, x, y, width, height) != 0 ||
        command(c, ram_command) != 0) {
        return -1;
    }
    for (int row = 0; row < height; row++) {
        const uint8_t *source = frame + (y + row) * panel_row_bytes + x / 8;
        if (data(c, source, (size_t)row_bytes) != 0) return -1;
    }
    return command(c, 0x92);
}

int il0324_init(il0324_t *c, const il0324_io_t *io) {
    static const uint8_t power[] = {0x07, 0x07, 0x3f, 0x3f};
    static const uint8_t boost[] = {0x17, 0x17, 0x1d};
    static const uint8_t panel[] = {0x1f};
    static const uint8_t resolution[] = {IL0324_PANEL_WIDTH, 0x01, 0xa0};
    static const uint8_t vcom[] = {0x1c};
    static const uint8_t interval[] = {0x29, 0x07};
    if (c == NULL || io == NULL || io->write_command == NULL ||
        io->write_data == NULL || io->wait_busy == NULL || io->delay_ms == NULL) {
        return -1;
    }
    memset(c, 0, sizeof(*c));
    c->io = *io;
    c->io.delay_ms(c->io.context, 10);
    if (command_data(c, 0x01, power, sizeof(power)) != 0 ||
        command_data(c, 0x06, boost, sizeof(boost)) != 0 ||
        power_on(c) != 0 ||
        command_data(c, 0x00, panel, sizeof(panel)) != 0 ||
        command_data(c, 0x61, resolution, sizeof(resolution)) != 0 ||
        command_data(c, 0x82, vcom, sizeof(vcom)) != 0 ||
        command_data(c, 0x50, interval, sizeof(interval)) != 0) {
        return -1;
    }
    c->initialized = 1;
    return 0;
}

int il0324_present(il0324_t *c, const uint8_t *frame, size_t length) {
    if (c == NULL || !c->initialized || c->sleeping || frame == NULL ||
        length != IL0324_FRAME_BYTES) return -1;
    if (command(c, 0x10) != 0 || data(c, frame, length) != 0 ||
        command(c, 0x13) != 0 || data(c, frame, length) != 0 ||
        command(c, 0x12) != 0 || wait_busy(c) != 0) return -1;
    return 0;
}

int il0324_present_partial(il0324_t *c, const uint8_t *frame, size_t length,
                           int x, int y, int width, int height) {
    /* The panel copies new RAM to old RAM (N2OCP), so only current RAM needs
     * to be written for a differential update. OTP full-refresh waveforms are
     * retained for reliability on uncharacterized HINK production lots. */
    if (c == NULL || !c->initialized || c->sleeping || frame == NULL ||
        length != IL0324_FRAME_BYTES) return -1;
    if (write_area(c, 0x13, frame, x, y, width, height) != 0 ||
        command(c, 0x12) != 0 || wait_busy(c) != 0) return -1;
    return 0;
}

int il0324_sleep(il0324_t *c) {
    static const uint8_t check_code[] = {0xa5};
    if (c == NULL || !c->initialized) return -1;
    if (power_off(c) != 0 || command_data(c, 0x07, check_code, sizeof(check_code)) != 0) {
        return -1;
    }
    c->sleeping = 1;
    c->initialized = 0;
    return 0;
}
