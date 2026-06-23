#include "app/app_persistence.h"
#include "app/app_state.h"
#include "font/font.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gfx/gfx.h"
#include "platform/esp_display.h"
#include "platform/esp_input.h"
#include "platform/esp_board_config.h"
#include "ui/pages.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "ai_mobile";

#define APP_NVS_NAMESPACE "reader"
#define APP_NVS_KEY "app_state"

static void init_nvs_storage(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase before initialization: %s", esp_err_to_name(err));
        if (nvs_flash_erase() == ESP_OK) {
            err = nvs_flash_init();
        }
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS persistence unavailable: %s", esp_err_to_name(err));
    }
}

void app_main(void) {
    app_state_t app;
    gfx_framebuffer_t fb;
    esp_display_t display;
    esp_input_t input;
    font_t font;

    ESP_LOGI(TAG, "booting ESP32 E-Ink reader firmware skeleton");

    init_nvs_storage();
    app_init(&app);
    if (app_persistence_load_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) == 0) {
        ESP_LOGI(TAG, "restored app state from NVS");
    }
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
            if (button == APP_BUTTON_POWER_LONG) {
                if (app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) != 0) {
                    ESP_LOGW(TAG, "failed to save app state before display sleep");
                }
                esp_display_sleep(&display);
                continue;
            }
            app_handle_button(&app, button);
            if (app_persistence_save_nvs(APP_NVS_NAMESPACE, APP_NVS_KEY, &app) != 0) {
                ESP_LOGW(TAG, "failed to save app state to NVS");
            }
            gfx_clear(&fb, GFX_WHITE);
            ui_render_page(&fb, &app, &font);
            if (esp_display_present(&display, &fb) != 0) {
                ESP_LOGE(TAG, "failed to present button frame");
            }
        }
        vTaskDelay(pdMS_TO_TICKS(ESP_BUTTON_POLL_MS));
    }
}
