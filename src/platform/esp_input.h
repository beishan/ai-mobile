#ifndef ESP_INPUT_H
#define ESP_INPUT_H

#include "app/app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "platform/input_debounce.h"

#define ESP_INPUT_BUTTON_COUNT 5
#define ESP_INPUT_EVENT_QUEUE_LENGTH 8

typedef struct {
    input_debounce_t debounce[ESP_INPUT_BUTTON_COUNT];
    QueueHandle_t event_queue;
    TaskHandle_t scan_task;
    int scan_task_started;
} esp_input_t;

void esp_input_init(esp_input_t *input);
int esp_input_poll_button(esp_input_t *input, app_button_t *button);
int esp_input_poll_button_batch(esp_input_t *input, app_button_t *button,
                                int *repeat_count);
int esp_input_pending_count(const esp_input_t *input);

#endif
