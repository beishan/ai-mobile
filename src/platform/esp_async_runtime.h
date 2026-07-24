#ifndef ESP_ASYNC_RUNTIME_H
#define ESP_ASYNC_RUNTIME_H

#include "app/app_state.h"

typedef enum {
    ESP_ASYNC_IDLE = 0,
    ESP_ASYNC_RUNNING,
    ESP_ASYNC_COMPLETE,
    ESP_ASYNC_FAILED,
    ESP_ASYNC_READY
} esp_async_phase_t;

typedef struct {
    esp_async_phase_t phase;
    int progress;
    int total_count;
    int loaded_count;
} esp_async_library_snapshot_t;

typedef struct {
    esp_async_phase_t phase;
    int book_index;
    int result;
} esp_async_pagination_snapshot_t;

void esp_async_runtime_init(void);

void esp_async_library_start(void);
void esp_async_library_update_total(int total_count);
void esp_async_library_update_progress(int progress);
void esp_async_library_complete(int loaded_count);
void esp_async_library_fail(void);
void esp_async_library_mark_ready(void);
esp_async_library_snapshot_t esp_async_library_snapshot(void);

int esp_async_pagination_try_start(int book_index);
void esp_async_pagination_complete(int result);
void esp_async_pagination_fail_start(void);
void esp_async_pagination_mark_idle(void);
esp_async_pagination_snapshot_t esp_async_pagination_snapshot(void);

int esp_async_wifi_try_start(const app_state_t *request);
int esp_async_wifi_copy_request(app_state_t *request);
void esp_async_wifi_complete(const app_state_t *result);
void esp_async_wifi_fail(void);
esp_async_phase_t esp_async_wifi_phase(void);
int esp_async_wifi_take_result(app_state_t *result, int *succeeded);

#endif
