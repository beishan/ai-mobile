#ifndef ESP_WEB_ADMIN_H
#define ESP_WEB_ADMIN_H

int esp_web_admin_start(int sd_mounted, int provisioning_mode);
void esp_web_admin_stop(void);
int esp_web_admin_is_running(void);
int esp_web_admin_take_books_changed(void);

#endif
