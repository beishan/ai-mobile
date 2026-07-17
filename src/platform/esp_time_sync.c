#include "platform/esp_time_sync.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs.h"
#include "platform/esp_board_config.h"

#define WIFI_CONNECTED_BIT BIT0
#define TIME_VALID_EPOCH 1700000000
#define WIFI_NVS_NAMESPACE "network"
#define WIFI_NVS_SSID_KEY "wifi_ssid"
#define WIFI_NVS_PASSWORD_KEY "wifi_password"

static const char *TAG = "time_sync";
static EventGroupHandle_t sync_events;
static int started;
static int sntp_started;

static int time_is_valid(void) {
    return time(NULL) >= (time_t)TIME_VALID_EPOCH;
}

int esp_time_sync_load_credentials(char *ssid, size_t ssid_size,
                                   char *password, size_t password_size) {
    nvs_handle_t handle;
    size_t stored_ssid_size = ssid_size;
    size_t stored_password_size = password_size;
    int loaded = 0;

    if (ssid == NULL || password == NULL || ssid_size == 0 || password_size == 0) {
        return -1;
    }
    ssid[0] = '\0';
    password[0] = '\0';
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        if (nvs_get_str(handle, WIFI_NVS_SSID_KEY, ssid, &stored_ssid_size) == ESP_OK &&
            nvs_get_str(handle, WIFI_NVS_PASSWORD_KEY, password, &stored_password_size) == ESP_OK) {
            loaded = 1;
        }
        nvs_close(handle);
    }
    if (!loaded) {
        snprintf(ssid, ssid_size, "%s", ESP_WIFI_SSID);
        snprintf(password, password_size, "%s", ESP_WIFI_PASSWORD);
    }
    return loaded ? 0 : (ssid[0] != '\0' ? 0 : -1);
}

int esp_time_sync_save_credentials(const char *ssid, const char *password) {
    nvs_handle_t handle;
    esp_err_t err;
    if (ssid == NULL || password == NULL || ssid[0] == '\0') {
        return -1;
    }
    err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to open Wi-Fi credential storage: %s", esp_err_to_name(err));
        return -1;
    }
    err = nvs_set_str(handle, WIFI_NVS_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_NVS_PASSWORD_KEY, password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to save Wi-Fi credentials: %s", esp_err_to_name(err));
        return -1;
    }
    ESP_LOGI(TAG, "saved Wi-Fi credentials for SSID=%s", ssid);
    return 0;
}

static void start_sntp(void) {
    if (sntp_started) {
        return;
    }
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ESP_NTP_SERVER);
    esp_sntp_init();
    sntp_started = 1;
    ESP_LOGI(TAG, "NTP started: server=%s timezone=%s", ESP_NTP_SERVER, ESP_TIMEZONE);
}

static void wifi_event_handler(void *argument, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    (void)argument;
    (void)event_data;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupClearBits(sync_events, WIFI_CONNECTED_BIT);
        ESP_LOGW(TAG, "Wi-Fi disconnected; retrying");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(sync_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected; waiting for NTP time");
        start_sntp();
    }
}

void esp_time_sync_start(void) {
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t station = {0};
    char ssid[sizeof(station.sta.ssid)];
    char password[sizeof(station.sta.password)];
    esp_err_t err;

    if (started) {
        return;
    }
    setenv("TZ", ESP_TIMEZONE, 1);
    tzset();
    if (esp_time_sync_load_credentials(ssid, sizeof(ssid), password, sizeof(password)) != 0) {
        ESP_LOGW(TAG, "Wi-Fi credentials are empty; open Settings > Wi-Fi to configure NTP");
        return;
    }

    sync_events = xEventGroupCreate();
    if (sync_events == NULL) {
        ESP_LOGE(TAG, "failed to create time-sync event group");
        return;
    }
    if (esp_netif_init() != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize TCP/IP stack");
        return;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "failed to create default event loop: %s", esp_err_to_name(err));
        return;
    }
    esp_netif_create_default_wifi_sta();
    if (esp_wifi_init(&wifi_init) != ESP_OK ||
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL) != ESP_OK ||
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize Wi-Fi event handlers");
        return;
    }
    snprintf((char *)station.sta.ssid, sizeof(station.sta.ssid), "%s", ssid);
    snprintf((char *)station.sta.password, sizeof(station.sta.password), "%s", password);
    station.sta.threshold.authmode = WIFI_AUTH_OPEN;
    station.sta.sae_pwe_h2e = WPA3_SAE_PWE_BOTH;
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_STA, &station) != ESP_OK ||
        esp_wifi_start() != ESP_OK) {
        ESP_LOGE(TAG, "failed to start Wi-Fi station");
        return;
    }
    started = 1;
    ESP_LOGI(TAG, "Wi-Fi station started for SSID=%s", ssid);
}

int esp_time_sync_wait_for_time(int timeout_ms) {
    int elapsed_ms = 0;
    if (!started) {
        return time_is_valid();
    }
    while (elapsed_ms < timeout_ms) {
        if (time_is_valid()) {
            ESP_LOGI(TAG, "NTP time synchronized: epoch=%lld", (long long)time(NULL));
            return 1;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
        elapsed_ms += 200;
    }
    ESP_LOGW(TAG, "NTP time not ready after %d ms; continuing boot", timeout_ms);
    return time_is_valid();
}

int esp_time_sync_is_ready(void) {
    return time_is_valid();
}
