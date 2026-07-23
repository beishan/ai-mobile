#include "platform/storage_io.h"

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static StaticSemaphore_t storage_mutex_buffer;
static SemaphoreHandle_t storage_mutex;
static portMUX_TYPE storage_state_lock = portMUX_INITIALIZER_UNLOCKED;
static int foreground_waiters;

void storage_io_init(void) {
    if (storage_mutex == NULL) {
        storage_mutex = xSemaphoreCreateRecursiveMutexStatic(&storage_mutex_buffer);
    }
}

void storage_io_lock(storage_io_priority_t priority) {
    if (storage_mutex == NULL) storage_io_init();
    if (storage_mutex == NULL) return;
    if (priority == STORAGE_IO_FOREGROUND) {
        taskENTER_CRITICAL(&storage_state_lock);
        foreground_waiters++;
        taskEXIT_CRITICAL(&storage_state_lock);
        xSemaphoreTakeRecursive(storage_mutex, portMAX_DELAY);
        taskENTER_CRITICAL(&storage_state_lock);
        foreground_waiters--;
        taskEXIT_CRITICAL(&storage_state_lock);
        return;
    }
    if (xSemaphoreGetMutexHolder(storage_mutex) == xTaskGetCurrentTaskHandle()) {
        xSemaphoreTakeRecursive(storage_mutex, portMAX_DELAY);
        return;
    }
    while (1) {
        int waiting;
        taskENTER_CRITICAL(&storage_state_lock);
        waiting = foreground_waiters;
        taskEXIT_CRITICAL(&storage_state_lock);
        if (waiting > 0) {
            taskYIELD();
            continue;
        }
        xSemaphoreTakeRecursive(storage_mutex, portMAX_DELAY);
        taskENTER_CRITICAL(&storage_state_lock);
        waiting = foreground_waiters;
        taskEXIT_CRITICAL(&storage_state_lock);
        if (waiting == 0) return;
        xSemaphoreGiveRecursive(storage_mutex);
        taskYIELD();
    }
}

void storage_io_unlock(void) {
    if (storage_mutex != NULL) {
        xSemaphoreGiveRecursive(storage_mutex);
    }
}

#else

void storage_io_init(void) {}
void storage_io_lock(storage_io_priority_t priority) { (void)priority; }
void storage_io_unlock(void) {}

#endif
