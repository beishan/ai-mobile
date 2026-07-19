#include "platform/esp_battery.h"
#include "platform/esp_board_config.h"

#include <stddef.h>

#if ESP_BATTERY_ADC_GPIO >= 0
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"

static const char *TAG = "battery";
static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;
static adc_channel_t adc_channel;
static int initialized;

static int battery_init(void) {
    adc_unit_t unit;
    adc_oneshot_unit_init_cfg_t unit_config = {0};
    adc_oneshot_chan_cfg_t channel_config = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    adc_cali_curve_fitting_config_t calibration = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    if (initialized) return adc_handle != NULL && cali_handle != NULL ? 0 : -1;
    initialized = 1;
    if (adc_oneshot_io_to_channel(ESP_BATTERY_ADC_GPIO, &unit, &adc_channel) != ESP_OK) return -1;
    unit_config.unit_id = unit;
    calibration.unit_id = unit;
    if (adc_oneshot_new_unit(&unit_config, &adc_handle) != ESP_OK ||
        adc_oneshot_config_channel(adc_handle, adc_channel, &channel_config) != ESP_OK ||
        adc_cali_create_scheme_curve_fitting(&calibration, &cali_handle) != ESP_OK) {
        ESP_LOGW(TAG, "battery ADC initialization failed");
        return -1;
    }
    return 0;
}
#endif

int esp_battery_read_percent(int *percent, int *millivolts) {
    if (percent == NULL || millivolts == NULL) return -1;
#if ESP_BATTERY_ADC_GPIO >= 0
    int raw;
    int pin_mv;
    int total_mv = 0;
    if (battery_init() != 0) return -1;
    for (int i = 0; i < 16; i++) {
        if (adc_oneshot_read(adc_handle, adc_channel, &raw) != ESP_OK ||
            adc_cali_raw_to_voltage(cali_handle, raw, &pin_mv) != ESP_OK) return -1;
        total_mv += pin_mv;
    }
    pin_mv = total_mv / 16;
    *millivolts = pin_mv * ESP_BATTERY_DIVIDER_NUMERATOR /
                  ESP_BATTERY_DIVIDER_DENOMINATOR;
    *percent = (*millivolts - ESP_BATTERY_EMPTY_MV) * 100 /
               (ESP_BATTERY_FULL_MV - ESP_BATTERY_EMPTY_MV);
    if (*percent < 0) *percent = 0;
    if (*percent > 100) *percent = 100;
    return 0;
#else
    *percent = 0;
    *millivolts = 0;
    return -1;
#endif
}
