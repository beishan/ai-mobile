#ifndef ESP_TIME_SYNC_H
#define ESP_TIME_SYNC_H

#include <stddef.h>

#define ESP_WIFI_SCAN_MAX 10
#define ESP_WIFI_SAVED_MAX 5

typedef struct {
    char ssid[33];
    int rssi;
    int secure;
} esp_wifi_scan_result_t;

void esp_time_sync_start(void);
int esp_time_sync_wait_for_time(int timeout_ms);
int esp_time_sync_is_ready(void);
int esp_time_sync_wifi_connected(void);
int esp_time_sync_get_ip(char *buffer, size_t buffer_size);
int esp_time_sync_load_credentials(char *ssid, size_t ssid_size,
                                   char *password, size_t password_size);
int esp_time_sync_save_credentials(const char *ssid, const char *password);
int esp_time_sync_scan_networks(esp_wifi_scan_result_t *results, int max_results);
int esp_time_sync_load_saved_networks(char ssids[][33], int max_networks);
int esp_time_sync_load_saved_password(const char *ssid, char *password, size_t password_size);
int esp_time_sync_has_credentials(void);
int esp_time_sync_start_provisioning_ap(const char *ap_ssid);

#endif
