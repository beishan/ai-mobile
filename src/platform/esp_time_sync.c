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
#define WIFI_NVS_LIST_KEY "wifi_list"
#define WIFI_LIST_MAGIC 0x57494649U

typedef struct {
    char ssid[33];
    char password[65];
} saved_wifi_entry_t;

typedef struct {
    uint32_t magic;
    uint32_t version;
    int count;
    saved_wifi_entry_t entries[ESP_WIFI_SAVED_MAX];
} saved_wifi_list_t;

static const char *TAG = "time_sync";
static EventGroupHandle_t sync_events;
static int started;
static int sntp_started;
static int scan_driver_started;
static int wifi_initialized;
static volatile int station_connected;

static int load_saved_list(nvs_handle_t handle, saved_wifi_list_t *list) {
    size_t size = sizeof(*list);
    memset(list, 0, sizeof(*list));
    if (nvs_get_blob(handle, WIFI_NVS_LIST_KEY, list, &size) != ESP_OK ||
        size != sizeof(*list) || list->magic != WIFI_LIST_MAGIC ||
        list->version != 1 || list->count < 0 || list->count > ESP_WIFI_SAVED_MAX) {
        memset(list, 0, sizeof(*list));
        list->magic = WIFI_LIST_MAGIC;
        list->version = 1;
        return -1;
    }
    for (int i = 0; i < list->count; i++) {
        list->entries[i].ssid[sizeof(list->entries[i].ssid) - 1] = '\0';
        list->entries[i].password[sizeof(list->entries[i].password) - 1] = '\0';
    }
    return 0;
}

int esp_time_sync_has_credentials(void) {
    char ssid[33];
    char password[65];
    return esp_time_sync_load_credentials(ssid, sizeof(ssid),
                                          password, sizeof(password)) == 0 &&
           ssid[0] != '\0';
}

int esp_time_sync_start_provisioning_ap(const char *ap_ssid) {
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t ap = {0};
    esp_netif_t *ap_netif = NULL;
    esp_err_t err;
    if (ap_ssid == NULL || ap_ssid[0] == '\0') return -1;
    if (started) return -1;
    if (!scan_driver_started) {
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return -1;
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return -1;
        ap_netif = esp_netif_create_default_wifi_ap();
        esp_netif_t *station_netif = esp_netif_create_default_wifi_sta();
        if (station_netif != NULL) esp_netif_set_hostname(station_netif, "ai-reader");
        if (esp_wifi_init(&wifi_init) != ESP_OK) return -1;
        wifi_initialized = 1;
    } else {
        ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    }
    snprintf((char *)ap.ap.ssid, sizeof(ap.ap.ssid), "%s", ap_ssid);
    ap.ap.ssid_len = (uint8_t)strlen((char *)ap.ap.ssid);
    ap.ap.channel = 1;
    ap.ap.authmode = WIFI_AUTH_OPEN;
    ap.ap.max_connection = 4;
    if (esp_wifi_set_mode(WIFI_MODE_APSTA) != ESP_OK ||
        esp_wifi_set_config(WIFI_IF_AP, &ap) != ESP_OK ||
        esp_wifi_start() != ESP_OK) return -1;
    if (ap_netif != NULL) {
        esp_netif_ip_info_t ip_info;
        esp_netif_dns_info_t dns = {0};
        uint8_t offer_dns = 0x02;
        if (esp_netif_get_ip_info(ap_netif, &ip_info) == ESP_OK) {
            dns.ip.u_addr.ip4.addr = ip_info.ip.addr;
            dns.ip.type = IPADDR_TYPE_V4;
            esp_netif_dhcps_stop(ap_netif);
            esp_netif_dhcps_option(ap_netif, ESP_NETIF_OP_SET,
                                   ESP_NETIF_DOMAIN_NAME_SERVER,
                                   &offer_dns, sizeof(offer_dns));
            esp_netif_set_dns_info(ap_netif, ESP_NETIF_DNS_MAIN, &dns);
            esp_netif_dhcps_start(ap_netif);
        }
    }
    scan_driver_started = 1;
    started = 1;
    ESP_LOGI(TAG, "provisioning access point started: %s", ap_ssid);
    return 0;
}

int esp_time_sync_scan_networks(esp_wifi_scan_result_t *results, int max_results) {
    wifi_ap_record_t records[ESP_WIFI_SCAN_MAX];
    wifi_scan_config_t scan_config = {0};
    uint16_t count;
    esp_err_t err;
    if (results == NULL || max_results <= 0) return -1;
    if (max_results > ESP_WIFI_SCAN_MAX) max_results = ESP_WIFI_SCAN_MAX;
    if (!started && esp_time_sync_has_credentials()) {
        esp_time_sync_start();
    }
    if (!started && !scan_driver_started) {
        wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return -1;
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return -1;
        esp_netif_create_default_wifi_sta();
        if (esp_wifi_init(&wifi_init) != ESP_OK ||
            esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK ||
            esp_wifi_start() != ESP_OK) return -1;
        wifi_initialized = 1;
        scan_driver_started = 1;
        started = 1;
    }
    scan_config.show_hidden = false;
    if (esp_wifi_scan_start(&scan_config, true) != ESP_OK) return -1;
    count = (uint16_t)max_results;
    memset(records, 0, sizeof(records));
    if (esp_wifi_scan_get_ap_records(&count, records) != ESP_OK) return -1;
    for (int i = 0; i < count; i++) {
        snprintf(results[i].ssid, sizeof(results[i].ssid), "%s", (char *)records[i].ssid);
        results[i].rssi = records[i].rssi;
        results[i].secure = records[i].authmode != WIFI_AUTH_OPEN;
    }
    ESP_LOGI(TAG, "Wi-Fi scan found %u network(s)", (unsigned int)count);
    return (int)count;
}

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
    saved_wifi_list_t list;
    if (ssid == NULL || password == NULL || ssid[0] == '\0') {
        return -1;
    }
    err = nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to open Wi-Fi credential storage: %s", esp_err_to_name(err));
        return -1;
    }
    load_saved_list(handle, &list);
    int existing = -1;
    for (int i = 0; i < list.count; i++) {
        if (strcmp(list.entries[i].ssid, ssid) == 0) {
            existing = i;
            break;
        }
    }
    if (existing < 0) {
        existing = list.count < ESP_WIFI_SAVED_MAX ? list.count : ESP_WIFI_SAVED_MAX - 1;
        if (list.count < ESP_WIFI_SAVED_MAX) list.count++;
    }
    for (int i = existing; i > 0; i--) list.entries[i] = list.entries[i - 1];
    snprintf(list.entries[0].ssid, sizeof(list.entries[0].ssid), "%s", ssid);
    snprintf(list.entries[0].password, sizeof(list.entries[0].password), "%s", password);
    err = nvs_set_str(handle, WIFI_NVS_SSID_KEY, ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(handle, WIFI_NVS_PASSWORD_KEY, password);
    }
    if (err == ESP_OK) {
        err = nvs_set_blob(handle, WIFI_NVS_LIST_KEY, &list, sizeof(list));
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
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        station_connected = 0;
        if (sync_events != NULL) xEventGroupClearBits(sync_events, WIFI_CONNECTED_BIT);
        if (started) {
            ESP_LOGW(TAG, "Wi-Fi disconnected; retrying");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        station_connected = 1;
        if (sync_events != NULL) xEventGroupSetBits(sync_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi connected: " IPSTR " (web: http://ai-reader/)",
                 IP2STR(&event->ip_info.ip));
        start_sntp();
    }
}

void esp_time_sync_start(void) {
    wifi_init_config_t wifi_init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t station = {0};
    char ssid[sizeof(station.sta.ssid)];
    char password[sizeof(station.sta.password)];
    esp_err_t err;

    if (started) return;
    setenv("TZ", ESP_TIMEZONE, 1);
    tzset();
    if (esp_time_sync_load_credentials(ssid, sizeof(ssid), password, sizeof(password)) != 0) {
        ESP_LOGW(TAG, "Wi-Fi credentials are empty; open Settings > Wi-Fi to configure NTP");
        return;
    }

    if (sync_events == NULL) {
        sync_events = xEventGroupCreate();
        if (sync_events == NULL) {
            ESP_LOGE(TAG, "failed to create time-sync event group");
            return;
        }
    }
    if (!wifi_initialized) {
        err = esp_netif_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "failed to initialize TCP/IP stack");
            return;
        }
        err = esp_event_loop_create_default();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGE(TAG, "failed to create default event loop: %s", esp_err_to_name(err));
            return;
        }
        esp_netif_t *station_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (station_netif == NULL) station_netif = esp_netif_create_default_wifi_sta();
        if (station_netif != NULL) esp_netif_set_hostname(station_netif, "ai-reader");
        if (esp_wifi_init(&wifi_init) != ESP_OK ||
            esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                       wifi_event_handler, NULL) != ESP_OK ||
            esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                       wifi_event_handler, NULL) != ESP_OK) {
            ESP_LOGE(TAG, "failed to initialize Wi-Fi event handlers");
            return;
        }
        wifi_initialized = 1;
        scan_driver_started = 1;
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

void esp_time_sync_stop(void) {
    if (sntp_started) {
        esp_sntp_stop();
        sntp_started = 0;
    }
    if (started) {
        started = 0;
        station_connected = 0;
        if (sync_events != NULL) xEventGroupClearBits(sync_events, WIFI_CONNECTED_BIT);
        (void)esp_wifi_disconnect();
        if (esp_wifi_stop() != ESP_OK) {
            ESP_LOGW(TAG, "failed to stop Wi-Fi radio");
        } else {
            ESP_LOGI(TAG, "Wi-Fi radio stopped");
        }
    }
}

int esp_time_sync_is_running(void) {
    return started ? 1 : 0;
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

int esp_time_sync_wifi_connected(void) {
    return station_connected ? 1 : 0;
}

int esp_time_sync_get_ip(char *buffer, size_t buffer_size) {
    esp_netif_t *netif;
    esp_netif_ip_info_t info;
    if (buffer == NULL || buffer_size == 0) return -1;
    buffer[0] = '\0';
    netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!station_connected || netif == NULL || esp_netif_get_ip_info(netif, &info) != ESP_OK ||
        info.ip.addr == 0) return -1;
    snprintf(buffer, buffer_size, IPSTR, IP2STR(&info.ip));
    return 0;
}

int esp_time_sync_load_saved_networks(char ssids[][33], int max_networks) {
    nvs_handle_t handle;
    saved_wifi_list_t list;
    int count = 0;
    if (ssids == NULL || max_networks <= 0) return -1;
    if (max_networks > ESP_WIFI_SAVED_MAX) max_networks = ESP_WIFI_SAVED_MAX;
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        if (load_saved_list(handle, &list) == 0) {
            count = list.count < max_networks ? list.count : max_networks;
            for (int i = 0; i < count; i++) {
                snprintf(ssids[i], 33, "%s", list.entries[i].ssid);
            }
        }
        nvs_close(handle);
    }
    if (count == 0) {
        char password[65];
        if (esp_time_sync_load_credentials(ssids[0], 33, password, sizeof(password)) == 0 &&
            ssids[0][0] != '\0') count = 1;
    }
    return count;
}

int esp_time_sync_load_saved_password(const char *ssid, char *password, size_t password_size) {
    nvs_handle_t handle;
    saved_wifi_list_t list;
    if (ssid == NULL || password == NULL || password_size == 0) return -1;
    password[0] = '\0';
    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        if (load_saved_list(handle, &list) == 0) {
            for (int i = 0; i < list.count; i++) {
                if (strcmp(list.entries[i].ssid, ssid) == 0) {
                    snprintf(password, password_size, "%s", list.entries[i].password);
                    nvs_close(handle);
                    return 0;
                }
            }
        }
        nvs_close(handle);
    }
    {
        char current_ssid[33];
        char current_password[65];
        if (esp_time_sync_load_credentials(current_ssid, sizeof(current_ssid),
                                           current_password, sizeof(current_password)) == 0 &&
            strcmp(current_ssid, ssid) == 0) {
            snprintf(password, password_size, "%s", current_password);
            return 0;
        }
    }
    return -1;
}
