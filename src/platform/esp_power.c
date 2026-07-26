#include "platform/esp_power.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform/esp_board_config.h"

static const char *TAG = "power";

static void configure_button_wakeup(void) {
    const int pins[] = {
        ESP_BUTTON_PIN_BACK,
        ESP_BUTTON_PIN_POWER,
        ESP_BUTTON_PIN_UP,
        ESP_BUTTON_PIN_HOME,
        ESP_BUTTON_PIN_DOWN
    };
    for (size_t i = 0; i < sizeof(pins) / sizeof(pins[0]); i++) {
        esp_err_t err = gpio_wakeup_enable((gpio_num_t)pins[i],
                                          GPIO_INTR_LOW_LEVEL);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "GPIO%d light-sleep wake unavailable: %s",
                     pins[i], esp_err_to_name(err));
        }
    }
    if (esp_sleep_enable_gpio_wakeup() != ESP_OK) {
        ESP_LOGW(TAG, "could not enable GPIO light-sleep wake source");
    }
}

void esp_power_set_enabled(int enabled) {
    esp_pm_config_t config = {
        .max_freq_mhz = 160,
        .min_freq_mhz = enabled ? 40 : 160,
        .light_sleep_enable = enabled ? true : false
    };
    esp_err_t err = esp_pm_configure(&config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "dynamic power configuration failed: %s",
                 esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "dynamic power management: min=%dMHz auto_light_sleep=%s",
                 config.min_freq_mhz, enabled ? "on" : "off");
    }
}

void esp_power_init(int enabled) {
    configure_button_wakeup();
    esp_power_set_enabled(enabled);
}

int esp_power_light_sleep(int wait_for_power_release) {
    esp_sleep_wakeup_cause_t cause;
    if (wait_for_power_release) {
        int waited_ms = 0;
        while (gpio_get_level(ESP_BUTTON_PIN_POWER) == ESP_BUTTON_ACTIVE_LEVEL &&
               waited_ms < 3000) {
            vTaskDelay(pdMS_TO_TICKS(20));
            waited_ms += 20;
        }
    }
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (!wait_for_power_release) {
        /* Periodically wake to service the low-rate clock/weather schedule. */
        (void)esp_sleep_enable_timer_wakeup(10ULL * 60ULL * 1000000ULL);
    }
    ESP_LOGI(TAG, "entering light sleep");
    if (esp_light_sleep_start() != ESP_OK) {
        ESP_LOGW(TAG, "light sleep entry failed");
        return 0;
    }
    cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "woke from light sleep: cause=%d", (int)cause);
    return cause == ESP_SLEEP_WAKEUP_GPIO;
}
