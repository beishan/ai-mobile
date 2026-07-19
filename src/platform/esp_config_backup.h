#ifndef ESP_CONFIG_BACKUP_H
#define ESP_CONFIG_BACKUP_H

/* Save Wi-Fi and QWeather NVS values to SD, or restore only values that are
 * absent from NVS. The SD card must already be mounted. */
int esp_config_backup_save_platform(void);
int esp_config_backup_restore_platform(void);

#endif
