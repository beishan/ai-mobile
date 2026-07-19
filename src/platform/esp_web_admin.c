#include "platform/esp_web_admin.h"

#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "platform/esp_board_config.h"
#include "platform/esp_config_backup.h"
#include "platform/esp_time_sync.h"
#include "platform/esp_weather.h"

#define ADMIN_AP_SSID "AI-Reader-Setup"
#define BOOK_DIRECTORY ESP_SD_MOUNT_POINT "/books"
#define FONT_DIRECTORY ESP_SD_MOUNT_POINT "/fonts"
#define BOOK_CACHE BOOK_DIRECTORY "/.ai_mobile_index.bin"
#define BOOK_CACHE_TEMP BOOK_DIRECTORY "/.ai_mobile_index.tmp"
#define BOOK_CACHE_BACKUP BOOK_DIRECTORY "/.ai_mobile_index.bak"
#define WEATHER_NVS_NAMESPACE "qweather"
#define HTTP_BUFFER_SIZE 2048

extern const unsigned char web_admin_html_start[];
extern const unsigned char web_admin_html_end[];

static const char *TAG = "web_admin";
static httpd_handle_t admin_server;
static int admin_sd_mounted;
static int admin_provisioning;

static void set_json(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
}

static void url_decode(char *text) {
    char *source = text;
    char *dest = text;
    while (*source != '\0') {
        if (*source == '+' ) {
            *dest++ = ' ';
            source++;
        } else if (*source == '%' && isxdigit((unsigned char)source[1]) &&
                   isxdigit((unsigned char)source[2])) {
            char hex[3] = {source[1], source[2], '\0'};
            *dest++ = (char)strtol(hex, NULL, 16);
            source += 3;
        } else {
            *dest++ = *source++;
        }
    }
    *dest = '\0';
}

static int read_request_body(httpd_req_t *req, char *buffer, size_t capacity) {
    int received = 0;
    if (req->content_len <= 0 || (size_t)req->content_len >= capacity) return -1;
    while (received < req->content_len) {
        int result = httpd_req_recv(req, buffer + received, req->content_len - received);
        if (result == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (result <= 0) return -1;
        received += result;
    }
    buffer[received] = '\0';
    return received;
}

static int form_value(const char *body, const char *key, char *dest, size_t dest_size) {
    if (httpd_query_key_value(body, key, dest, dest_size) != ESP_OK) {
        if (dest_size > 0) dest[0] = '\0';
        return -1;
    }
    url_decode(dest);
    return 0;
}

static void json_escape(const char *source, char *dest, size_t capacity) {
    size_t used = 0;
    if (capacity == 0) return;
    while (source != NULL && *source != '\0' && used + 2 < capacity) {
        unsigned char c = (unsigned char)*source++;
        if (c == '"' || c == '\\') {
            if (used + 3 >= capacity) break;
            dest[used++] = '\\';
            dest[used++] = (char)c;
        } else if (c < 0x20) {
            dest[used++] = ' ';
        } else {
            dest[used++] = (char)c;
        }
    }
    dest[used] = '\0';
}

static int safe_filename(const char *name, const char *extension) {
    size_t name_length;
    size_t extension_length;
    if (name == NULL || name[0] == '\0' || strstr(name, "..") != NULL ||
        strchr(name, '/') != NULL || strchr(name, '\\') != NULL) return 0;
    name_length = strlen(name);
    extension_length = strlen(extension);
    /* Keep the percent-encoded query below HTTPD's 512-byte URI limit. */
    if (name_length <= extension_length || name_length > 150) return 0;
    for (size_t i = 0; i < name_length; i++) {
        if ((unsigned char)name[i] < 0x20) return 0;
    }
    for (size_t i = 0; i < extension_length; i++) {
        if (tolower((unsigned char)name[name_length - extension_length + i]) !=
            tolower((unsigned char)extension[i])) return 0;
    }
    return 1;
}

static const char *directory_for_type(const char *type, const char **extension) {
    if (strcmp(type, "books") == 0) {
        *extension = ".txt";
        return BOOK_DIRECTORY;
    }
    if (strcmp(type, "fonts") == 0) {
        *extension = ".bin";
        return FONT_DIRECTORY;
    }
    return NULL;
}

static void invalidate_book_cache(void) {
    unlink(BOOK_CACHE);
    unlink(BOOK_CACHE_TEMP);
    unlink(BOOK_CACHE_BACKUP);
}

static esp_err_t root_handler(httpd_req_t *req) {
    size_t length = (size_t)(web_admin_html_end - web_admin_html_start);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, (const char *)web_admin_html_start, length);
}

static esp_err_t status_handler(httpd_req_t *req) {
    char ssid[33] = "";
    char password[65] = "";
    char host[128] = "";
    char location[64] = "";
    char api_key[128] = "";
    char safe_ssid[80];
    char safe_host[180];
    char safe_location[100];
    char response[640];
    nvs_handle_t nvs;
    size_t size;
    esp_time_sync_load_credentials(ssid, sizeof(ssid), password, sizeof(password));
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READONLY, &nvs) == ESP_OK) {
        size = sizeof(host); nvs_get_str(nvs, "api_host", host, &size);
        size = sizeof(location); nvs_get_str(nvs, "location", location, &size);
        size = sizeof(api_key); nvs_get_str(nvs, "api_key", api_key, &size);
        nvs_close(nvs);
    }
    json_escape(ssid, safe_ssid, sizeof(safe_ssid));
    json_escape(host, safe_host, sizeof(safe_host));
    json_escape(location, safe_location, sizeof(safe_location));
    snprintf(response, sizeof(response),
             "{\"mode\":\"%s\",\"ap_ssid\":\"%s\",\"wifi_ssid\":\"%s\","
             "\"sd_mounted\":%s,\"weather_host\":\"%s\",\"weather_location\":\"%s\","
             "\"weather_key_configured\":%s}",
             admin_provisioning ? "provisioning" : "lan", ADMIN_AP_SSID, safe_ssid,
             admin_sd_mounted ? "true" : "false", safe_host, safe_location,
             api_key[0] ? "true" : "false");
    set_json(req);
    return httpd_resp_sendstr(req, response);
}

static esp_err_t wifi_scan_handler(httpd_req_t *req) {
    esp_wifi_scan_result_t networks[ESP_WIFI_SCAN_MAX];
    char item[220];
    char ssid[90];
    int count = esp_time_sync_scan_networks(networks, ESP_WIFI_SCAN_MAX);
    set_json(req);
    httpd_resp_send_chunk(req, "[", 1);
    for (int i = 0; i < count; i++) {
        json_escape(networks[i].ssid, ssid, sizeof(ssid));
        snprintf(item, sizeof(item), "%s{\"ssid\":\"%s\",\"rssi\":%d,\"secure\":%s}",
                 i ? "," : "", ssid, networks[i].rssi,
                 networks[i].secure ? "true" : "false");
        httpd_resp_sendstr_chunk(req, item);
    }
    if (httpd_resp_send_chunk(req, "]", 1) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t wifi_save_handler(httpd_req_t *req) {
    char body[512];
    char ssid[33];
    char password[65];
    if (read_request_body(req, body, sizeof(body)) < 0 ||
        form_value(body, "ssid", ssid, sizeof(ssid)) != 0 || ssid[0] == '\0' ||
        form_value(body, "password", password, sizeof(password)) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid Wi-Fi settings");
    }
    if (esp_time_sync_save_credentials(ssid, password) != 0) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    if (admin_sd_mounted && esp_config_backup_save_platform() != 0) {
        ESP_LOGW(TAG, "failed to back up Wi-Fi settings to SD");
    }
    set_json(req);
    httpd_resp_sendstr(req, "{\"ok\":true,\"restarting\":true}");
    ESP_LOGI(TAG, "Wi-Fi settings saved from web admin; restarting");
    vTaskDelay(pdMS_TO_TICKS(700));
    esp_restart();
    return ESP_OK;
}

static esp_err_t weather_save_handler(httpd_req_t *req) {
    char body[768];
    char host[128];
    char location[64];
    char api_key[128];
    nvs_handle_t nvs;
    if (read_request_body(req, body, sizeof(body)) < 0 ||
        form_value(body, "host", host, sizeof(host)) != 0 || host[0] == '\0' ||
        form_value(body, "location", location, sizeof(location)) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid weather settings");
    }
    form_value(body, "api_key", api_key, sizeof(api_key));
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "NVS open failed");
    }
    nvs_set_str(nvs, "api_host", host);
    nvs_set_str(nvs, "location", location);
    if (api_key[0] != '\0') nvs_set_str(nvs, "api_key", api_key);
    esp_err_t result = nvs_commit(nvs);
    nvs_close(nvs);
    if (result != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
    }
    if (admin_sd_mounted && esp_config_backup_save_platform() != 0) {
        ESP_LOGW(TAG, "failed to back up weather settings to SD");
    }
    if (esp_time_sync_wifi_connected() && esp_weather_request_update() != 0) {
        ESP_LOGW(TAG, "weather update is already running or could not be started");
    }
    set_json(req);
    return httpd_resp_sendstr(req, "{\"ok\":true}");
}

static int request_file_parameters(httpd_req_t *req, char *type, size_t type_size,
                                   char *name, size_t name_size,
                                   const char **directory, const char **extension) {
    char query[512];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) != ESP_OK ||
        httpd_query_key_value(query, "type", type, type_size) != ESP_OK) return -1;
    *directory = directory_for_type(type, extension);
    if (*directory == NULL) return -1;
    if (name != NULL) {
        if (httpd_query_key_value(query, "name", name, name_size) != ESP_OK) return -1;
        url_decode(name);
        if (!safe_filename(name, *extension)) return -1;
    }
    return 0;
}

static esp_err_t files_list_handler(httpd_req_t *req) {
    char type[16];
    const char *directory;
    const char *extension;
    DIR *dir;
    struct dirent *entry;
    if (request_file_parameters(req, type, sizeof(type), NULL, 0,
                                &directory, &extension) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file type");
    }
    set_json(req);
    httpd_resp_send_chunk(req, "[", 1);
    dir = admin_sd_mounted ? opendir(directory) : NULL;
    int first = 1;
    while (dir != NULL && (entry = readdir(dir)) != NULL) {
        char path[512];
        char escaped[500];
        char item[700];
        struct stat info;
        if (!safe_filename(entry->d_name, extension)) continue;
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        if (stat(path, &info) != 0 || S_ISDIR(info.st_mode)) continue;
        json_escape(entry->d_name, escaped, sizeof(escaped));
        snprintf(item, sizeof(item), "%s{\"name\":\"%s\",\"size\":%lu}",
                 first ? "" : ",", escaped, (unsigned long)info.st_size);
        first = 0;
        httpd_resp_sendstr_chunk(req, item);
    }
    if (dir != NULL) closedir(dir);
    if (httpd_resp_send_chunk(req, "]", 1) != ESP_OK) return ESP_FAIL;
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t file_upload_handler(httpd_req_t *req) {
    char type[16];
    char name[256];
    char path[512];
    char buffer[HTTP_BUFFER_SIZE];
    const char *directory;
    const char *extension;
    FILE *file;
    int remaining = req->content_len;
    size_t max_size;
    if (!admin_sd_mounted || request_file_parameters(req, type, sizeof(type), name, sizeof(name),
                                                     &directory, &extension) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid upload");
    }
    max_size = strcmp(type, "books") == 0 ? 32U * 1024U * 1024U : 16U * 1024U * 1024U;
    if (remaining <= 0 || (size_t)remaining > max_size) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "file size rejected");
    }
    mkdir(directory, 0775);
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    file = fopen(path, "wb");
    if (file == NULL) return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open failed");
    while (remaining > 0) {
        int wanted = remaining > (int)sizeof(buffer) ? (int)sizeof(buffer) : remaining;
        int received = httpd_req_recv(req, buffer, wanted);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (received <= 0 || fwrite(buffer, 1, (size_t)received, file) != (size_t)received) {
            fclose(file);
            unlink(path);
            return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "write failed");
        }
        remaining -= received;
    }
    fclose(file);
    if (strcmp(type, "books") == 0) invalidate_book_cache();
    ESP_LOGI(TAG, "uploaded %s", path);
    set_json(req);
    return httpd_resp_sendstr(req, "{\"ok\":true,\"restart_recommended\":true}");
}

static esp_err_t file_delete_handler(httpd_req_t *req) {
    char type[16];
    char name[256];
    char path[512];
    const char *directory;
    const char *extension;
    if (!admin_sd_mounted || request_file_parameters(req, type, sizeof(type), name, sizeof(name),
                                                     &directory, &extension) != 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid delete");
    }
    snprintf(path, sizeof(path), "%s/%s", directory, name);
    if (unlink(path) != 0) return httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "file not found");
    if (strcmp(type, "books") == 0) invalidate_book_cache();
    ESP_LOGI(TAG, "deleted %s", path);
    set_json(req);
    return httpd_resp_sendstr(req, "{\"ok\":true,\"restart_recommended\":true}");
}

static void dns_task(void *argument) {
    unsigned char request[512];
    unsigned char response[528];
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    struct sockaddr_in address = {0};
    (void)argument;
    if (sock < 0) vTaskDelete(NULL);
    address.sin_family = AF_INET;
    address.sin_port = htons(53);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
        close(sock);
        vTaskDelete(NULL);
    }
    while (1) {
        struct sockaddr_in client;
        socklen_t client_length = sizeof(client);
        int length = recvfrom(sock, request, sizeof(request), 0,
                              (struct sockaddr *)&client, &client_length);
        if (length < 12 || length + 16 > (int)sizeof(response)) continue;
        int end = 12;
        while (end < length && request[end] != 0) end += request[end] + 1;
        end += 5;
        if (end > length) continue;
        memcpy(response, request, (size_t)end);
        response[2] = 0x81; response[3] = 0x80;
        response[6] = 0; response[7] = 1;
        response[8] = response[9] = response[10] = response[11] = 0;
        unsigned char answer[] = {0xc0, 0x0c, 0, 1, 0, 1, 0, 0, 0, 60, 0, 4,
                                  192, 168, 4, 1};
        memcpy(response + end, answer, sizeof(answer));
        sendto(sock, response, (size_t)end + sizeof(answer), 0,
               (struct sockaddr *)&client, client_length);
    }
}

int esp_web_admin_start(int sd_mounted, int provisioning_mode) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    const httpd_uri_t routes[] = {
        {.uri = "/api/status", .method = HTTP_GET, .handler = status_handler},
        {.uri = "/api/wifi/scan", .method = HTTP_GET, .handler = wifi_scan_handler},
        {.uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_save_handler},
        {.uri = "/api/weather", .method = HTTP_POST, .handler = weather_save_handler},
        {.uri = "/api/files", .method = HTTP_GET, .handler = files_list_handler},
        {.uri = "/api/upload", .method = HTTP_POST, .handler = file_upload_handler},
        {.uri = "/api/files", .method = HTTP_DELETE, .handler = file_delete_handler},
        {.uri = "/*", .method = HTTP_GET, .handler = root_handler}
    };
    admin_sd_mounted = sd_mounted;
    admin_provisioning = provisioning_mode;
    if (admin_server != NULL) return 0;
    if (sd_mounted) {
        mkdir(BOOK_DIRECTORY, 0775);
        mkdir(FONT_DIRECTORY, 0775);
    }
    config.stack_size = 8192;
    config.max_uri_handlers = 12;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.lru_purge_enable = true;
    if (httpd_start(&admin_server, &config) != ESP_OK) return -1;
    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); i++) {
        if (httpd_register_uri_handler(admin_server, &routes[i]) != ESP_OK) return -1;
    }
    if (provisioning_mode) {
        xTaskCreate(dns_task, "captive_dns", 4096, NULL, tskIDLE_PRIORITY + 1, NULL);
    }
    ESP_LOGI(TAG, "web admin started in %s mode", provisioning_mode ? "provisioning" : "LAN");
    return 0;
}
