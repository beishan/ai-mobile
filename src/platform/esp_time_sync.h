#ifndef ESP_TIME_SYNC_H
#define ESP_TIME_SYNC_H

#include <stddef.h>

void esp_time_sync_start(void);
int esp_time_sync_wait_for_time(int timeout_ms);
int esp_time_sync_is_ready(void);
int esp_time_sync_load_credentials(char *ssid, size_t ssid_size,
                                   char *password, size_t password_size);
int esp_time_sync_save_credentials(const char *ssid, const char *password);

#endif
