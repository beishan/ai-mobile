#include "platform/esp_input.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "platform/esp_board_config.h"

static const char *TAG = "esp_input";

typedef struct {
    int pin;
    app_button_t button;
    int internal_pullup;
    const char *name;
} esp_button_map_t;

static const esp_button_map_t BUTTONS[ESP_INPUT_BUTTON_COUNT] = {
    {ESP_BUTTON_PIN_POWER, APP_BUTTON_POWER, 1, "POWER"},
    {ESP_BUTTON_PIN_UP, APP_BUTTON_UP, 0, "UP"},
    {ESP_BUTTON_PIN_HOME, APP_BUTTON_HOME, 0, "HOME"},
    {ESP_BUTTON_PIN_DOWN, APP_BUTTON_DOWN, 0, "DOWN"}
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

void esp_input_init(esp_input_t *input) {
    if (input == NULL) {
        return;
    }

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
                     BUTTONS[i].name,
                     BUTTONS[i].pin,
                     esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "button %s mapped to GPIO%d active_level=%d debounce_ms=%d long_press_ms=%d",
                     BUTTONS[i].name,
                     BUTTONS[i].pin,
                     ESP_BUTTON_ACTIVE_LEVEL,
                     ESP_BUTTON_DEBOUNCE_MS,
                     ESP_BUTTON_LONG_PRESS_MS);
        }
    }
}

int esp_input_poll_button(esp_input_t *input, app_button_t *button) {
    if (input == NULL || button == NULL) {
        return 0;
    }

    for (int i = 0; i < ESP_INPUT_BUTTON_COUNT; i++) {
        int pressed = gpio_get_level(BUTTONS[i].pin) == ESP_BUTTON_ACTIVE_LEVEL;
        if (BUTTONS[i].button == APP_BUTTON_POWER) {
            input_debounce_event_t event = input_debounce_update_hold(&input->debounce[i], pressed, long_press_samples());
            if (event == INPUT_DEBOUNCE_LONG_PRESS) {
                *button = APP_BUTTON_POWER_LONG;
                return 1;
            }
            if (event == INPUT_DEBOUNCE_SHORT_PRESS) {
                *button = APP_BUTTON_POWER;
                return 1;
            }
        } else if (input_debounce_update(&input->debounce[i], pressed)) {
            *button = BUTTONS[i].button;
            return 1;
        }
    }

    return 0;
}
