#include "platform/esp_weather.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "miniz.h"
#include "nvs.h"

#define WEATHER_NVS_NAMESPACE "qweather"
#define WEATHER_RESPONSE_MAX 4096
#define WEATHER_JSON_MAX 8192
#define WEATHER_TASK_STACK 12288

typedef struct {
    char *data;
    size_t used;
    size_t capacity;
} weather_response_t;

static const char *TAG = "weather_api";
static SemaphoreHandle_t result_mutex;
static esp_weather_result_t current_result;
static volatile int request_running;

static void set_result_message(esp_weather_result_t *result, const char *message) {
    if (result == NULL) return;
    snprintf(result->error, sizeof(result->error), "%s",
             message != NULL ? message : "天气更新失败");
}

static void publish_result(esp_weather_result_t *result) {
    if (result_mutex == NULL) result_mutex = xSemaphoreCreateMutex();
    if (result_mutex != NULL && xSemaphoreTake(result_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        result->generation = current_result.generation + 1;
        current_result = *result;
        xSemaphoreGive(result_mutex);
        if (result->valid) {
            ESP_LOGI(TAG, "weather updated: %s %dC humidity=%d%%",
                     result->text, result->temperature, result->humidity);
        } else {
            ESP_LOGW(TAG, "weather update unavailable: %s", result->error);
        }
    }
}

static int load_configuration(char *host, size_t host_size,
                              char *api_key, size_t key_size,
                              char *location, size_t location_size) {
    nvs_handle_t nvs;
    size_t size;
    host[0] = api_key[0] = location[0] = '\0';
    if (nvs_open(WEATHER_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return -1;
    size = host_size;
    nvs_get_str(nvs, "api_host", host, &size);
    size = key_size;
    nvs_get_str(nvs, "api_key", api_key, &size);
    size = location_size;
    nvs_get_str(nvs, "location", location, &size);
    nvs_close(nvs);
    return host[0] != '\0' && api_key[0] != '\0' && location[0] != '\0' ? 0 : -1;
}

int esp_weather_is_configured(void) {
    char host[128];
    char api_key[128];
    char location[64];
    return load_configuration(host, sizeof(host), api_key, sizeof(api_key),
                              location, sizeof(location)) == 0;
}

static esp_err_t weather_http_event(esp_http_client_event_t *event) {
    weather_response_t *response = event->user_data;
    if (event->event_id == HTTP_EVENT_ON_DATA && response != NULL &&
        event->data != NULL && event->data_len > 0) {
        size_t available = response->capacity - response->used - 1;
        size_t copy = (size_t)event->data_len < available ? (size_t)event->data_len : available;
        if (copy > 0) {
            memcpy(response->data + response->used, event->data, copy);
            response->used += copy;
            response->data[response->used] = '\0';
        }
    }
    return ESP_OK;
}

static int json_string(const char *json, const char *key, char *dest, size_t capacity) {
    char pattern[48];
    const char *cursor;
    size_t used = 0;
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    cursor = strstr(json, pattern);
    if (cursor == NULL || (cursor = strchr(cursor + strlen(pattern), ':')) == NULL) return -1;
    cursor++;
    while (isspace((unsigned char)*cursor)) cursor++;
    if (*cursor++ != '"') return -1;
    while (*cursor != '\0' && *cursor != '"' && used + 1 < capacity) {
        if (*cursor == '\\' && cursor[1] != '\0') {
            cursor++;
            if (*cursor == 'n' || *cursor == 'r' || *cursor == 't') {
                cursor++;
                continue;
            }
        }
        dest[used++] = *cursor++;
    }
    dest[used] = '\0';
    return *cursor == '"' ? 0 : -1;
}

static int weather_type_for(const char *icon, const char *text) {
    int code = atoi(icon);
    if ((code >= 300 && code < 400) || strstr(text, "雨") != NULL) return 2;
    if ((code >= 400 && code < 500) || strstr(text, "雪") != NULL) return 3;
    if (code == 100 || strstr(text, "晴") != NULL) return 0;
    return 1;
}

static int valid_location(const char *location) {
    if (location == NULL || location[0] == '\0') return 0;
    while (*location != '\0') {
        if (!isalnum((unsigned char)*location) && *location != ',' &&
            *location != '.' && *location != '-') return 0;
        location++;
    }
    return 1;
}

static int gzip_decompress(const unsigned char *input, size_t input_size,
                           char *output, size_t output_size) {
    size_t cursor = 10;
    unsigned int flags;
    size_t input_remaining;
    size_t output_available;
    tinfl_status status;
    tinfl_decompressor *decompressor;
    if (input == NULL || output == NULL || output_size < 2 || input_size < 18 ||
        input[0] != 0x1f || input[1] != 0x8b || input[2] != 8) return -1;
    flags = input[3];
    if ((flags & 0xe0U) != 0) return -1;
    if ((flags & 0x04U) != 0) {
        size_t extra_length;
        if (cursor + 2 > input_size - 8) return -1;
        extra_length = (size_t)input[cursor] | ((size_t)input[cursor + 1] << 8);
        cursor += 2;
        if (cursor + extra_length > input_size - 8) return -1;
        cursor += extra_length;
    }
    if ((flags & 0x08U) != 0) {
        while (cursor < input_size - 8 && input[cursor++] != 0) {}
        if (cursor >= input_size - 8 && input[cursor - 1] != 0) return -1;
    }
    if ((flags & 0x10U) != 0) {
        while (cursor < input_size - 8 && input[cursor++] != 0) {}
        if (cursor >= input_size - 8 && input[cursor - 1] != 0) return -1;
    }
    if ((flags & 0x02U) != 0) {
        if (cursor + 2 > input_size - 8) return -1;
        cursor += 2;
    }
    if (cursor >= input_size - 8) return -1;
    decompressor = heap_caps_malloc(sizeof(*decompressor),
                                    MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (decompressor == NULL) decompressor = malloc(sizeof(*decompressor));
    if (decompressor == NULL) return -1;
    tinfl_init(decompressor);
    input_remaining = input_size - cursor - 8;
    output_available = output_size - 1;
    status = tinfl_decompress(decompressor, input + cursor, &input_remaining,
                              (unsigned char *)output, (unsigned char *)output,
                              &output_available, TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    free(decompressor);
    if (status != TINFL_STATUS_DONE || output_available >= output_size) return -1;
    output[output_available] = '\0';
    return (int)output_available;
}

static void weather_task(void *argument) {
    char host[128];
    char api_key[128];
    char location[64];
    char url[320];
    char *body = malloc(WEATHER_RESPONSE_MAX);
    char *decoded_body = NULL;
    weather_response_t response = {body, 0, WEATHER_RESPONSE_MAX};
    esp_weather_result_t result = {0};
    (void)argument;
    if (body == NULL) {
        set_result_message(&result, "天气任务内存不足");
        goto finished;
    }
    if (load_configuration(host, sizeof(host), api_key, sizeof(api_key),
                           location, sizeof(location)) != 0) {
        set_result_message(&result, "天气配置不完整");
        goto finished;
    }
    if (!valid_location(location)) {
        set_result_message(&result, "天气城市配置无效");
        goto finished;
    }
    size_t host_length = strlen(host);
    while (host_length > 0 && host[host_length - 1] == '/') host[--host_length] = '\0';
    snprintf(url, sizeof(url), "%s%s/v7/weather/now?location=%s&lang=zh&unit=m",
             strstr(host, "://") == NULL ? "https://" : "", host, location);
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = weather_http_event,
        .user_data = &response,
        .timeout_ms = 12000,
        .buffer_size = 2048,
        .crt_bundle_attach = esp_crt_bundle_attach
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        set_result_message(&result, "天气请求初始化失败");
        goto finished;
    }
    esp_http_client_set_header(client, "X-QW-Api-Key", api_key);
    esp_http_client_set_header(client, "Accept-Encoding", "gzip");
    esp_err_t error = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    if (error == ESP_OK && status == 200 && response.used > 0) {
        const char *json = body;
        if (response.used >= 2 && (unsigned char)body[0] == 0x1f &&
            (unsigned char)body[1] == 0x8b) {
            int decoded_size;
            decoded_body = malloc(WEATHER_JSON_MAX);
            decoded_size = decoded_body != NULL
                               ? gzip_decompress((const unsigned char *)body, response.used,
                                                 decoded_body, WEATHER_JSON_MAX)
                               : -1;
            if (decoded_size < 0) {
                set_result_message(&result, "天气压缩数据解码失败");
                ESP_LOGW(TAG, "QWeather gzip decompression failed: bytes=%u",
                         (unsigned int)response.used);
            } else {
                json = decoded_body;
                ESP_LOGD(TAG, "QWeather gzip decoded: %u -> %d bytes",
                         (unsigned int)response.used, decoded_size);
            }
        }
        const char *now = result.error[0] == '\0' ? strstr(json, "\"now\"") : NULL;
        char code[8] = "";
        char temp[12];
        char humidity[12];
        char icon[12];
        char wind_dir[20];
        char wind_scale[12];
        if (result.error[0] == '\0' &&
            json_string(json, "code", code, sizeof(code)) == 0 && strcmp(code, "200") == 0 &&
            now != NULL && json_string(now, "temp", temp, sizeof(temp)) == 0 &&
            json_string(now, "humidity", humidity, sizeof(humidity)) == 0 &&
            json_string(now, "icon", icon, sizeof(icon)) == 0 &&
            json_string(now, "text", result.text, sizeof(result.text)) == 0) {
            result.temperature = atoi(temp);
            result.humidity = atoi(humidity);
            result.type = weather_type_for(icon, result.text);
            wind_dir[0] = wind_scale[0] = '\0';
            json_string(now, "windDir", wind_dir, sizeof(wind_dir));
            json_string(now, "windScale", wind_scale, sizeof(wind_scale));
            snprintf(result.wind, sizeof(result.wind), "%.18s %.6s级",
                     wind_dir, wind_scale);
            result.valid = 1;
        } else if (code[0] != '\0') {
            if (strcmp(code, "401") == 0 || strcmp(code, "403") == 0) {
                set_result_message(&result, "API Key或Host无效");
            } else if (strcmp(code, "402") == 0) {
                set_result_message(&result, "天气API无访问权限");
            } else if (strcmp(code, "404") == 0) {
                set_result_message(&result, "天气城市编号无效");
            } else if (strcmp(code, "429") == 0) {
                set_result_message(&result, "天气API请求已超限");
            } else {
                snprintf(result.error, sizeof(result.error), "天气API错误 %s", code);
            }
            ESP_LOGW(TAG, "QWeather API rejected request: code=%s body=%.180s", code, json);
        } else if (result.error[0] == '\0') {
            set_result_message(&result, "天气响应解析失败");
            ESP_LOGW(TAG, "QWeather response parse failed: bytes=%u body=%.180s",
                     (unsigned int)response.used, json);
        }
    } else {
        ESP_LOGW(TAG, "QWeather request failed: error=%s HTTP=%d",
                 esp_err_to_name(error), status);
        if (error != ESP_OK) {
            set_result_message(&result, "天气网络请求失败");
        } else {
            snprintf(result.error, sizeof(result.error), "天气服务 HTTP %d", status);
        }
    }
    esp_http_client_cleanup(client);

finished:
    if (!result.valid && result.error[0] == '\0') set_result_message(&result, "天气更新失败");
    publish_result(&result);
    ESP_LOGI(TAG, "weather task stack minimum free: %u bytes",
             (unsigned int)uxTaskGetStackHighWaterMark(NULL));
    free(decoded_body);
    free(body);
    request_running = 0;
    vTaskDelete(NULL);
}

int esp_weather_request_update(void) {
    if (request_running || !esp_weather_is_configured()) return -1;
    request_running = 1;
    if (xTaskCreate(weather_task, "weather_api", WEATHER_TASK_STACK, NULL,
                    tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        request_running = 0;
        return -1;
    }
    return 0;
}

int esp_weather_get_result(esp_weather_result_t *result) {
    if (result == NULL) return -1;
    if (result_mutex == NULL) result_mutex = xSemaphoreCreateMutex();
    if (result_mutex == NULL || xSemaphoreTake(result_mutex, pdMS_TO_TICKS(50)) != pdTRUE) return -1;
    *result = current_result;
    xSemaphoreGive(result_mutex);
    return result->generation > 0 ? 0 : -1;
}
