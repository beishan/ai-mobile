#ifndef ESP_SD_H
#define ESP_SD_H

#include "sdmmc_cmd.h"

typedef struct {
    sdmmc_card_t *card;
    int mounted;
} esp_sd_t;

int esp_sd_init(esp_sd_t *sd);
int esp_sd_is_mounted(const esp_sd_t *sd);

#endif
