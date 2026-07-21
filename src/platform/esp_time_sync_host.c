#include "platform/esp_time_sync.h"

#ifndef ESP_PLATFORM
int esp_time_sync_load_saved_password(const char *ssid, char *password,
                                      size_t password_size) {
    (void)ssid;
    if (password != NULL && password_size > 0) password[0] = '\0';
    return -1;
}
#endif
