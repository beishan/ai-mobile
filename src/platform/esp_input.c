#include "platform/esp_input.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "platform/esp_board_config.h"

static const char *TAG = "esp_input";

typedef struct {
    int pin;
    app_button_t button;
    int internal_pullup;
    const char *name;
} esp_button_map_t;

static const esp_button_map_t BUTTONS[ESP_INPUT_BUTTON_COUNT] = {
    {ESP_BUTTON_PIN_BACK, APP_BUTTON_BACK, 1, "BACK"},
    {ESP_BUTTON_PIN_POWER, APP_BUTTON_POWER, 1, "POWER"},
    {ESP_BUTTON_PIN_UP, APP_BUTTON_UP, 1, "UP"},
    {ESP_BUTTON_PIN_HOME, APP_BUTTON_HOME, 1, "HOME"},
    {ESP_BUTTON_PIN_DOWN, APP_BUTTON_DOWN, 1, "DOWN"}
};

static int debounce_samples(void) {
    int samples = ESP_BUTTON_DEBOUNCE_MS / ESP_BUTTON_POLL_MS;
    if (ESP_BUTTON_DEBOUNCE_MS % ESP_BUTTON_POLL_MS != 0) {
        samples++;
    }
    return samples > 1 ? samples : 1;
}

static int long_press_samples(void) {
    int samples = ESP_BUTTON_LONG_PRESS_MS / ESP_BUTTON_POLL_MS;
    if (ESP_BUTTON_LONG_PRESS_MS % ESP_BUTTON_POLL_MS != 0) {
        samples++;
    }
    return samples > debounce_samples() ? samples : debounce_samples();
}

static int esp_input_scan_button(esp_input_t *input, app_button_t *button) {
    for (int i = 0; i < ESP_INPUT_BUTTON_COUNT; i++) {
        int pressed = gpio_get_level(BUTTONS[i].pin) == ESP_BUTTON_ACTIVE_LEVEL;
        int was_pressed = input->debounce[i].debounced_pressed;

        if (BUTTONS[i].button == APP_BUTTON_POWER) {
            input_debounce_event_t event = input_debounce_update_hold(
                &input->debounce[i], pressed, long_press_samples());
            if (input->debounce[i].debounced_pressed != was_pressed) {
                ESP_LOGI(TAG, "button %s GPIO%d %s (debounced)",
                         BUTTONS[i].name, BUTTONS[i].pin,
                         input->debounce[i].debounced_pressed ? "PRESSED" : "RELEASED");
            }
            if (event == INPUT_DEBOUNCE_LONG_PRESS) {
                *button = APP_BUTTON_POWER_LONG;
                ESP_LOGI(TAG, "button %s GPIO%d event=LONG_PRESS held_ms=%d",
                         BUTTONS[i].name, BUTTONS[i].pin, ESP_BUTTON_LONG_PRESS_MS);
                return 1;
            }
            if (event == INPUT_DEBOUNCE_SHORT_PRESS) {
                *button = APP_BUTTON_POWER;
                ESP_LOGI(TAG, "button %s GPIO%d event=SHORT_PRESS",
                         BUTTONS[i].name, BUTTONS[i].pin);
                return 1;
            }
        } else {
            int event = input_debounce_update(&input->debounce[i], pressed);
            if (input->debounce[i].debounced_pressed != was_pressed) {
                ESP_LOGI(TAG, "button %s GPIO%d %s (debounced)",
                         BUTTONS[i].name, BUTTONS[i].pin,
                         input->debounce[i].debounced_pressed ? "PRESSED" : "RELEASED");
            }
            if (event) {
                *button = BUTTONS[i].button;
                ESP_LOGI(TAG, "button %s GPIO%d event=PRESS app_button=%d",
                         BUTTONS[i].name, BUTTONS[i].pin, (int)*button);
                return 1;
            }
        }
    }
    return 0;
}

static void esp_input_scan_task(void *context) {
    esp_input_t *input = (esp_input_t *)context;
    while (1) {
        app_button_t button;
        if (esp_input_scan_button(input, &button) &&
            xQueueSend(input->event_queue, &button, 0) != pdTRUE) {
            ESP_LOGW(TAG, "button event queue full; dropping app_button=%d", (int)button);
        }
        vTaskDelay(pdMS_TO_TICKS(ESP_BUTTON_POLL_MS));
    }
}

void esp_input_init(esp_input_t *input) {
    if (input == NULL) {
        return;
    }

    input->event_queue = NULL;
    input->scan_task = NULL;
    input->scan_task_started = 0;

    for (int i = 0; i < ESP_INPUT_BUTTON_COUNT; i++) {
        gpio_config_t config = {
            .pin_bit_mask = 1ULL << BUTTONS[i].pin,
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = BUTTONS[i].internal_pullup ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
        };
        esp_err_t err = gpio_config(&config);
        input_debounce_init(&input->debounce[i], debounce_samples());
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "failed to configure %s button GPIO%d: %s",
                     BUTTONS[i].name, BUTTONS[i].pin, esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG,
                     "button %s mapped to GPIO%d active_level=%d debounce_ms=%d long_press_ms=%d",
                     BUTTONS[i].name, BUTTONS[i].pin, ESP_BUTTON_ACTIVE_LEVEL,
                     ESP_BUTTON_DEBOUNCE_MS, ESP_BUTTON_LONG_PRESS_MS);
        }
    }

    input->event_queue = xQueueCreate(ESP_INPUT_EVENT_QUEUE_LENGTH, sizeof(app_button_t));
    if (input->event_queue == NULL) {
        ESP_LOGE(TAG, "failed to create button event queue; using main-loop scan fallback");
        return;
    }
    if (xTaskCreate(esp_input_scan_task, "button_scan", 3072, input, 5,
                    &input->scan_task) != pdPASS) {
        ESP_LOGE(TAG, "failed to start button scan task; using main-loop scan fallback");
        vQueueDelete(input->event_queue);
        input->event_queue = NULL;
        return;
    }
    input->scan_task_started = 1;
    ESP_LOGI(TAG, "button scan task started: poll_ms=%d debounce_ms=%d queue=%d",
             ESP_BUTTON_POLL_MS, ESP_BUTTON_DEBOUNCE_MS, ESP_INPUT_EVENT_QUEUE_LENGTH);
}

int esp_input_poll_button(esp_input_t *input, app_button_t *button) {
    if (input == NULL || button == NULL) {
        return 0;
    }
    if (input->scan_task_started && input->event_queue != NULL) {
        return xQueueReceive(input->event_queue, button, 0) == pdTRUE;
    }
    return esp_input_scan_button(input, button);
}
