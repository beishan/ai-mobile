#ifndef ESP_BATTERY_H
#define ESP_BATTERY_H

/* Returns 0 when a configured ADC sample is available, otherwise -1. */
int esp_battery_read_percent(int *percent, int *millivolts);

#endif
