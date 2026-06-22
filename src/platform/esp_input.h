#ifndef ESP_INPUT_H
#define ESP_INPUT_H

#include "app/app_state.h"

#define ESP_INPUT_BUTTON_COUNT 4

typedef struct {
    int armed[ESP_INPUT_BUTTON_COUNT];
} esp_input_t;

void esp_input_init(esp_input_t *input);
int esp_input_poll_button(esp_input_t *input, app_button_t *button);

#endif
