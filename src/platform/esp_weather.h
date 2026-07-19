#ifndef ESP_WEATHER_H
#define ESP_WEATHER_H

#include <stddef.h>

typedef struct {
    int valid;
    int temperature;
    int humidity;
    int type;
    char text[24];
    char error[64];
    char wind[32];
    unsigned int generation;
} esp_weather_result_t;

int esp_weather_is_configured(void);
int esp_weather_request_update(void);
int esp_weather_get_result(esp_weather_result_t *result);

#endif
