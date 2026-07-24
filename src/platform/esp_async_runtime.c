#include "platform/esp_async_runtime.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

typedef struct {
    esp_async_library_snapshot_t library;
    esp_async_pagination_snapshot_t pagination;
    esp_async_phase_t wifi_phase;
    app_state_t wifi_request;
    app_state_t wifi_result;
} esp_async_runtime_state_t;

static StaticSemaphore_t runtime_mutex_storage;
static SemaphoreHandle_t runtime_mutex;
static esp_async_runtime_state_t runtime_state;

static void runtime_lock(void) {
    if (runtime_mutex != NULL) {
        xSemaphoreTake(runtime_mutex, portMAX_DELAY);
    }
}

static void runtime_unlock(void) {
    if (runtime_mutex != NULL) {
        xSemaphoreGive(runtime_mutex);
    }
}

void esp_async_runtime_init(void) {
    memset(&runtime_state, 0, sizeof(runtime_state));
    runtime_state.pagination.book_index = -1;
    runtime_mutex = xSemaphoreCreateMutexStatic(&runtime_mutex_storage);
}

void esp_async_library_start(void) {
    runtime_lock();
    runtime_state.library.phase = ESP_ASYNC_RUNNING;
    runtime_state.library.progress = 0;
    runtime_state.library.total_count = 0;
    runtime_state.library.loaded_count = 0;
    runtime_unlock();
}

void esp_async_library_update_total(int total_count) {
    runtime_lock();
    runtime_state.library.total_count = total_count;
    runtime_unlock();
}

void esp_async_library_update_progress(int progress) {
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;
    runtime_lock();
    runtime_state.library.progress = progress;
    runtime_unlock();
}

void esp_async_library_complete(int loaded_count) {
    runtime_lock();
    runtime_state.library.loaded_count = loaded_count;
    runtime_state.library.progress = 100;
    runtime_state.library.phase = ESP_ASYNC_COMPLETE;
    runtime_unlock();
}

void esp_async_library_fail(void) {
    runtime_lock();
    runtime_state.library.phase = ESP_ASYNC_FAILED;
    runtime_unlock();
}

void esp_async_library_mark_ready(void) {
    runtime_lock();
    if (runtime_state.library.phase == ESP_ASYNC_COMPLETE) {
        runtime_state.library.phase = ESP_ASYNC_READY;
    }
    runtime_unlock();
}

esp_async_library_snapshot_t esp_async_library_snapshot(void) {
    esp_async_library_snapshot_t snapshot;
    runtime_lock();
    snapshot = runtime_state.library;
    runtime_unlock();
    return snapshot;
}

int esp_async_pagination_try_start(int book_index) {
    int started = 0;
    runtime_lock();
    if (runtime_state.pagination.phase == ESP_ASYNC_IDLE) {
        runtime_state.pagination.phase = ESP_ASYNC_RUNNING;
        runtime_state.pagination.book_index = book_index;
        runtime_state.pagination.result = 0;
        started = 1;
    }
    runtime_unlock();
    return started;
}

void esp_async_pagination_complete(int result) {
    runtime_lock();
    runtime_state.pagination.result = result;
    runtime_state.pagination.phase = ESP_ASYNC_COMPLETE;
    runtime_unlock();
}

void esp_async_pagination_fail_start(void) {
    runtime_lock();
    runtime_state.pagination.phase = ESP_ASYNC_IDLE;
    runtime_state.pagination.book_index = -1;
    runtime_state.pagination.result = 0;
    runtime_unlock();
}

void esp_async_pagination_mark_idle(void) {
    esp_async_pagination_fail_start();
}

esp_async_pagination_snapshot_t esp_async_pagination_snapshot(void) {
    esp_async_pagination_snapshot_t snapshot;
    runtime_lock();
    snapshot = runtime_state.pagination;
    runtime_unlock();
    return snapshot;
}

int esp_async_wifi_try_start(const app_state_t *request) {
    int started = 0;
    if (request == NULL) return 0;
    runtime_lock();
    if (runtime_state.wifi_phase != ESP_ASYNC_RUNNING) {
        runtime_state.wifi_request = *request;
        runtime_state.wifi_phase = ESP_ASYNC_RUNNING;
        started = 1;
    }
    runtime_unlock();
    return started;
}

int esp_async_wifi_copy_request(app_state_t *request) {
    int available;
    if (request == NULL) return 0;
    runtime_lock();
    available = runtime_state.wifi_phase == ESP_ASYNC_RUNNING;
    if (available) *request = runtime_state.wifi_request;
    runtime_unlock();
    return available;
}

void esp_async_wifi_complete(const app_state_t *result) {
    if (result == NULL) return;
    runtime_lock();
    runtime_state.wifi_result = *result;
    runtime_state.wifi_phase = ESP_ASYNC_COMPLETE;
    runtime_unlock();
}

void esp_async_wifi_fail(void) {
    runtime_lock();
    runtime_state.wifi_phase = ESP_ASYNC_FAILED;
    runtime_unlock();
}

esp_async_phase_t esp_async_wifi_phase(void) {
    esp_async_phase_t phase;
    runtime_lock();
    phase = runtime_state.wifi_phase;
    runtime_unlock();
    return phase;
}

int esp_async_wifi_take_result(app_state_t *result, int *succeeded) {
    int ready;
    runtime_lock();
    ready = runtime_state.wifi_phase == ESP_ASYNC_COMPLETE ||
            runtime_state.wifi_phase == ESP_ASYNC_FAILED;
    if (ready) {
        int ok = runtime_state.wifi_phase == ESP_ASYNC_COMPLETE;
        if (ok && result != NULL) {
            memcpy(result->wifi_ip, runtime_state.wifi_result.wifi_ip,
                   sizeof(result->wifi_ip));
            memcpy(result->wifi_saved_ssids, runtime_state.wifi_result.wifi_saved_ssids,
                   sizeof(result->wifi_saved_ssids));
            result->wifi_saved_count = runtime_state.wifi_result.wifi_saved_count;
            memcpy(result->wifi_network_ssids, runtime_state.wifi_result.wifi_network_ssids,
                   sizeof(result->wifi_network_ssids));
            memcpy(result->wifi_network_rssi, runtime_state.wifi_result.wifi_network_rssi,
                   sizeof(result->wifi_network_rssi));
            memcpy(result->wifi_network_secure, runtime_state.wifi_result.wifi_network_secure,
                   sizeof(result->wifi_network_secure));
            result->wifi_network_count = runtime_state.wifi_result.wifi_network_count;
            result->wifi_network_selection =
                runtime_state.wifi_result.wifi_network_selection;
            result->wifi_scan_in_progress = 0;
            result->wifi_scan_requested = 0;
        }
        if (succeeded != NULL) *succeeded = ok;
        runtime_state.wifi_phase = ESP_ASYNC_IDLE;
    }
    runtime_unlock();
    return ready;
}
