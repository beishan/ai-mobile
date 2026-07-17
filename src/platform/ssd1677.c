#include "platform/ssd1677.h"

#include <string.h>

#define SSD1677_BUSY_TIMEOUT_MS 60000

static int write_command(ssd1677_t *controller, uint8_t command) {
    return controller->io.write_command(controller->io.context, command);
}

static int write_data(ssd1677_t *controller, const uint8_t *data, size_t length) {
    return controller->io.write_data(controller->io.context, data, length);
}

static int write_command_data(ssd1677_t *controller, uint8_t command,
                              const uint8_t *data, size_t length) {
    if (write_command(controller, command) != 0) {
        return -1;
    }
    if (length > 0 && write_data(controller, data, length) != 0) {
        return -1;
    }
    return 0;
}

static int wait_busy(ssd1677_t *controller) {
    return controller->io.wait_busy(controller->io.context, SSD1677_BUSY_TIMEOUT_MS);
}

static void delay_ms(ssd1677_t *controller, int milliseconds) {
    controller->io.delay_ms(controller->io.context, milliseconds);
}

static int set_full_ram_area(ssd1677_t *controller) {
    /* GDEQ0426T82 gates are reversed. Start at gate 479 and scan down. */
    static const uint8_t entry_mode[] = {0x01};
    static const uint8_t source_range[] = {0x00, 0x00, 0x1f, 0x03};
    static const uint8_t gate_range[] = {0xdf, 0x01, 0x00, 0x00};
    static const uint8_t source_counter[] = {0x00, 0x00};
    static const uint8_t gate_counter[] = {0xdf, 0x01};

    if (write_command_data(controller, 0x11, entry_mode, sizeof(entry_mode)) != 0 ||
        write_command_data(controller, 0x44, source_range, sizeof(source_range)) != 0 ||
        write_command_data(controller, 0x45, gate_range, sizeof(gate_range)) != 0 ||
        write_command_data(controller, 0x4e, source_counter, sizeof(source_counter)) != 0 ||
        write_command_data(controller, 0x4f, gate_counter, sizeof(gate_counter)) != 0) {
        return -1;
    }
    return 0;
}

static int set_partial_ram_area(ssd1677_t *controller, int x, int y, int width, int height) {
    uint8_t source_range[4];
    uint8_t gate_range[4];
    uint8_t source_counter[2];
    uint8_t gate_counter[2];
    static const uint8_t entry_mode[] = {0x01};
    int reversed_y;
    int x_end;
    int y_end;

    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > SSD1677_PANEL_WIDTH || y + height > SSD1677_PANEL_HEIGHT ||
        (x & 7) != 0 || (width & 7) != 0) {
        return -1;
    }

    x_end = x + width - 1;
    reversed_y = SSD1677_PANEL_HEIGHT - y - height;
    y_end = reversed_y + height - 1;
    source_range[0] = (uint8_t)(x & 0xff);
    source_range[1] = (uint8_t)((x >> 8) & 0xff);
    source_range[2] = (uint8_t)(x_end & 0xff);
    source_range[3] = (uint8_t)((x_end >> 8) & 0xff);
    gate_range[0] = (uint8_t)(y_end & 0xff);
    gate_range[1] = (uint8_t)((y_end >> 8) & 0xff);
    gate_range[2] = (uint8_t)(reversed_y & 0xff);
    gate_range[3] = (uint8_t)((reversed_y >> 8) & 0xff);
    source_counter[0] = source_range[0];
    source_counter[1] = source_range[1];
    gate_counter[0] = gate_range[0];
    gate_counter[1] = gate_range[1];

    if (write_command_data(controller, 0x11, entry_mode, sizeof(entry_mode)) != 0 ||
        write_command_data(controller, 0x44, source_range, sizeof(source_range)) != 0 ||
        write_command_data(controller, 0x45, gate_range, sizeof(gate_range)) != 0 ||
        write_command_data(controller, 0x4e, source_counter, sizeof(source_counter)) != 0 ||
        write_command_data(controller, 0x4f, gate_counter, sizeof(gate_counter)) != 0) {
        return -1;
    }
    return 0;
}

static int write_frame_area(ssd1677_t *controller, uint8_t command,
                            const uint8_t *frame, int x, int y, int width, int height) {
    const int panel_row_bytes = SSD1677_PANEL_WIDTH / 8;
    const int area_row_bytes = width / 8;
    const int byte_x = x / 8;

    if (set_partial_ram_area(controller, x, y, width, height) != 0 ||
        write_command(controller, command) != 0) {
        return -1;
    }
    for (int row = 0; row < height; row++) {
        const uint8_t *row_data = frame + (y + row) * panel_row_bytes + byte_x;
        if (write_data(controller, row_data, (size_t)area_row_bytes) != 0) {
            return -1;
        }
    }
    return 0;
}

int ssd1677_init(ssd1677_t *controller, const ssd1677_io_t *io) {
    static const uint8_t temperature_sensor[] = {0x80};
    static const uint8_t booster_soft_start[] = {0xae, 0xc7, 0xc3, 0xc0, 0x80};
    static const uint8_t driver_output[] = {0xdf, 0x01, 0x02};
    static const uint8_t border_waveform[] = {0x01};

    if (controller == NULL || io == NULL || io->write_command == NULL ||
        io->write_data == NULL || io->wait_busy == NULL || io->delay_ms == NULL) {
        return -1;
    }

    memset(controller, 0, sizeof(*controller));
    controller->io = *io;

    delay_ms(controller, 10);
    if (write_command(controller, 0x12) != 0) { /* software reset */
        return -1;
    }
    delay_ms(controller, 10);
    if (wait_busy(controller) != 0 ||
        write_command_data(controller, 0x18, temperature_sensor, sizeof(temperature_sensor)) != 0 ||
        write_command_data(controller, 0x0c, booster_soft_start, sizeof(booster_soft_start)) != 0 ||
        write_command_data(controller, 0x01, driver_output, sizeof(driver_output)) != 0 ||
        write_command_data(controller, 0x3c, border_waveform, sizeof(border_waveform)) != 0 ||
        set_full_ram_area(controller) != 0) {
        return -1;
    }

    controller->initialized = 1;
    controller->sleeping = 0;
    return 0;
}

int ssd1677_present(ssd1677_t *controller, const uint8_t *frame, size_t length) {
    static const uint8_t display_update_control[] = {0x40, 0x00};
    static const uint8_t full_update[] = {0xf7};

    if (controller == NULL || !controller->initialized || controller->sleeping ||
        frame == NULL || length != SSD1677_FRAME_BYTES) {
        return -1;
    }

    if (set_full_ram_area(controller) != 0 ||
        write_command(controller, 0x26) != 0 || /* previous image RAM */
        write_data(controller, frame, length) != 0 ||
        set_full_ram_area(controller) != 0 ||
        write_command(controller, 0x24) != 0 || /* current image RAM */
        write_data(controller, frame, length) != 0 ||
        write_command_data(controller, 0x21, display_update_control, sizeof(display_update_control)) != 0 ||
        write_command_data(controller, 0x22, full_update, sizeof(full_update)) != 0 ||
        write_command(controller, 0x20) != 0 ||
        wait_busy(controller) != 0) {
        return -1;
    }
    return 0;
}

int ssd1677_present_partial(ssd1677_t *controller, const uint8_t *frame, size_t length,
                            int x, int y, int width, int height) {
    static const uint8_t display_update_control[] = {0x00, 0x00};
    static const uint8_t partial_update[] = {0xfc};

    if (controller == NULL || !controller->initialized || controller->sleeping ||
        frame == NULL || length != SSD1677_FRAME_BYTES ||
        x < 0 || y < 0 || width <= 0 || height <= 0 ||
        x + width > SSD1677_PANEL_WIDTH || y + height > SSD1677_PANEL_HEIGHT ||
        (x & 7) != 0 || (width & 7) != 0) {
        return -1;
    }

    /* Write the changed window, refresh it, then synchronize both RAM planes.
     * Keeping 0x24 and 0x26 equal is required for the next differential update. */
    if (write_frame_area(controller, 0x24, frame, x, y, width, height) != 0 ||
        write_command_data(controller, 0x21, display_update_control, sizeof(display_update_control)) != 0 ||
        write_command_data(controller, 0x22, partial_update, sizeof(partial_update)) != 0 ||
        write_command(controller, 0x20) != 0 ||
        wait_busy(controller) != 0 ||
        write_frame_area(controller, 0x24, frame, x, y, width, height) != 0 ||
        write_frame_area(controller, 0x26, frame, x, y, width, height) != 0) {
        return -1;
    }
    return 0;
}

int ssd1677_sleep(ssd1677_t *controller) {
    static const uint8_t power_off[] = {0x83};
    static const uint8_t deep_sleep[] = {0x01};

    if (controller == NULL) {
        return -1;
    }
    if (controller->sleeping) {
        return 0;
    }
    if (!controller->initialized) {
        return -1;
    }

    if (write_command_data(controller, 0x22, power_off, sizeof(power_off)) != 0 ||
        write_command(controller, 0x20) != 0 ||
        wait_busy(controller) != 0 ||
        write_command_data(controller, 0x10, deep_sleep, sizeof(deep_sleep)) != 0) {
        return -1;
    }

    controller->sleeping = 1;
    controller->initialized = 0;
    return 0;
}
