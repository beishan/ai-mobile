#include "app/app_state.h"
#include "font/font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/gfx.h"
#include "platform/esp_display.h"
#include "ui/pages.h"

#include "esp_log.h"

static const char *TAG = "ai_mobile";

void app_main(void) {
    app_state_t app;
    gfx_framebuffer_t fb;
    esp_display_t display;
    font_t font;

    ESP_LOGI(TAG, "booting ESP32 E-Ink reader firmware skeleton");

    app_init(&app);
    gfx_init(&fb);
    esp_display_init(&display);

    if (!font_load_default(&font)) {
        ESP_LOGE(TAG, "failed to load built-in bitmap font");
        return;
    }

    ui_render_page(&fb, &app, &font);
    if (esp_display_present(&display, &fb) != 0) {
        ESP_LOGE(TAG, "failed to present first frame");
    }

    font_free(&font);

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
