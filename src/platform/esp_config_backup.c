#include "platform/esp_config_backup.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "nvs.h"
#include "platform/esp_board_config.h"
#include "platform/esp_time_sync.h"

#define CONFIG_DIRECTORY ESP_SD_MOUNT_POINT "/.ai_mobile"
#define PLATFORM_CONFIG_PATH CONFIG_DIRECTORY "/platform.cfg"
#define PLATFORM_CONFIG_MAGIC "AICFG3"
#define PLATFORM_CONFIG_VERSION 1U
#define WEATHER_NVS_NAMESPACE "qweather"

typedef struct {
    char magic[8];
    uint32_t version;
    char wifi_ssid[33];
    char wifi_password[65];
    char weather_host[128];
    char weather_api_key[128];
    char weather_location[64];
    uint32_t checksum;
} platform_config_t;

static const char *TAG = "config_backup";

static uint32_t config_checksum(const platform_config_t *config) {
    const unsigned char *data = (const unsigned char *)config;
    size_t length = offsetof(platform_config_t, checksum);
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) {
        hash ^= data[i];
        hash *= 16777619u;
    }
    return hash;
}

static int weather_load(char *host, size_t host_size,
                        char *key, size_t key_size,
                        char *location, size_t location_size) {
    nvs_handle_t nvs;
    size_t size;
    int loaded = 0;
    host[0] = key[0] = location[0] = '\0';
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return -1;
    size = host_size;
    if (nvs_get_str(nvs, "api_host", host, &size) == ESP_OK) loaded = 1;
    size = key_size;
    if (nvs_get_str(nvs, "api_key", key, &size) == ESP_OK) loaded = 1;
    size = location_size;
    if (nvs_get_str(nvs, "location", location, &size) == ESP_OK) loaded = 1;
    nvs_close(nvs);
    return loaded ? 0 : -1;
}

static int weather_save(const platform_config_t *config) {
    nvs_handle_t nvs;
    esp_err_t result;
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) return -1;
    result = nvs_set_str(nvs, "api_host", config->weather_host);
    if (result == ESP_OK) result = nvs_set_str(nvs, "api_key", config->weather_api_key);
    if (result == ESP_OK) result = nvs_set_str(nvs, "location", config->weather_location);
    if (result == ESP_OK) result = nvs_commit(nvs);
    nvs_close(nvs);
    return result == ESP_OK ? 0 : -1;
}

static int read_platform_config(platform_config_t *config) {
    FILE *file = fopen(PLATFORM_CONFIG_PATH, "rb");
    if (file == NULL) return -1;
    int valid = fread(config, sizeof(*config), 1, file) == 1 &&
                fgetc(file) == EOF && !ferror(file);
    fclose(file);
    if (!valid || memcmp(config->magic, PLATFORM_CONFIG_MAGIC,
                         sizeof(PLATFORM_CONFIG_MAGIC)) != 0 ||
        config->version != PLATFORM_CONFIG_VERSION ||
        config->checksum != config_checksum(config)) return -1;
    config->wifi_ssid[sizeof(config->wifi_ssid) - 1] = '\0';
    config->wifi_password[sizeof(config->wifi_password) - 1] = '\0';
    config->weather_host[sizeof(config->weather_host) - 1] = '\0';
    config->weather_api_key[sizeof(config->weather_api_key) - 1] = '\0';
    config->weather_location[sizeof(config->weather_location) - 1] = '\0';
    return 0;
}

int esp_config_backup_save_platform(void) {
    platform_config_t config = {0};
    FILE *file;
    snprintf(config.magic, sizeof(config.magic), "%s", PLATFORM_CONFIG_MAGIC);
    config.version = PLATFORM_CONFIG_VERSION;
    esp_time_sync_load_credentials(config.wifi_ssid, sizeof(config.wifi_ssid),
                                   config.wifi_password, sizeof(config.wifi_password));
    weather_load(config.weather_host, sizeof(config.weather_host),
                 config.weather_api_key, sizeof(config.weather_api_key),
                 config.weather_location, sizeof(config.weather_location));
    config.checksum = config_checksum(&config);
    if (mkdir(CONFIG_DIRECTORY, 0775) != 0 && errno != EEXIST) return -1;
    file = fopen(PLATFORM_CONFIG_PATH ".tmp", "wb");
    if (file == NULL) return -1;
    int write_ok = fwrite(&config, sizeof(config), 1, file) == 1;
    if (fclose(file) != 0) write_ok = 0;
    if (!write_ok) {
        remove(PLATFORM_CONFIG_PATH ".tmp");
        return -1;
    }
    remove(PLATFORM_CONFIG_PATH ".bak");
    rename(PLATFORM_CONFIG_PATH, PLATFORM_CONFIG_PATH ".bak");
    if (rename(PLATFORM_CONFIG_PATH ".tmp", PLATFORM_CONFIG_PATH) != 0) {
        rename(PLATFORM_CONFIG_PATH ".bak", PLATFORM_CONFIG_PATH);
        return -1;
    }
    remove(PLATFORM_CONFIG_PATH ".bak");
    ESP_LOGI(TAG, "saved platform configuration to %s", PLATFORM_CONFIG_PATH);
    return 0;
}

int esp_config_backup_restore_platform(void) {
    platform_config_t config;
    char host[128];
    char key[128];
    char location[64];
    int restored = 0;
    if (read_platform_config(&config) != 0) return -1;
    if (!esp_time_sync_has_credentials() && config.wifi_ssid[0] != '\0' &&
        esp_time_sync_save_credentials(config.wifi_ssid, config.wifi_password) == 0) {
        restored = 1;
        ESP_LOGI(TAG, "restored Wi-Fi configuration from SD");
    }
    if (weather_load(host, sizeof(host), key, sizeof(key),
                     location, sizeof(location)) != 0 &&
        (config.weather_host[0] != '\0' || config.weather_api_key[0] != '\0' ||
         config.weather_location[0] != '\0') && weather_save(&config) == 0) {
        restored = 1;
        ESP_LOGI(TAG, "restored QWeather configuration from SD");
    }
    return restored ? 0 : 1;
}
