#include "app/app_state.h"
#include "font/font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/gfx.h"
#include "platform/esp_display.h"
#include "platform/esp_input.h"
#include "platform/esp_board_config.h"
#include "ui/pages.h"

#include "esp_log.h"

static const char *TAG = "ai_mobile";

void app_main(void) {
    app_state_t app;
    gfx_framebuffer_t fb;
    esp_display_t display;
    esp_input_t input;
    font_t font;

    ESP_LOGI(TAG, "booting ESP32 E-Ink reader firmware skeleton");

    app_init(&app);
    gfx_init(&fb);
    esp_display_init(&display);
    esp_input_init(&input);

    if (!font_load_default(&font)) {
        ESP_LOGE(TAG, "failed to load built-in bitmap font");
        return;
    }

    ui_render_page(&fb, &app, &font);
    if (esp_display_present(&display, &fb) != 0) {
        ESP_LOGE(TAG, "failed to present first frame");
    }

    while (1) {
        app_button_t button;
        if (esp_input_poll_button(&input, &button)) {
            ESP_LOGI(TAG, "button event %d on page %s", button, app_page_name(app.page));
            app_handle_button(&app, button);
            gfx_clear(&fb, GFX_WHITE);
            ui_render_page(&fb, &app, &font);
            if (esp_display_present(&display, &fb) != 0) {
                ESP_LOGE(TAG, "failed to present button frame");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ESP_BUTTON_POLL_MS));
    }
}
