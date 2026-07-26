#ifndef ESP_POWER_H
#define ESP_POWER_H

void esp_power_init(int enabled);
void esp_power_set_enabled(int enabled);
/* Returns non-zero when a physical GPIO woke the device. */
int esp_power_light_sleep(int wait_for_power_release);

#endif
