#ifndef EPD_CONTROLLER_H
#define EPD_CONTROLLER_H

#include "platform/epd_panel.h"

#if EPD_PANEL_MODEL == EPD_PANEL_HINK_E037A03_A1
#include "platform/il0324.h"
typedef il0324_io_t epd_controller_io_t;
typedef il0324_t epd_controller_t;
#define epd_controller_init il0324_init
#define epd_controller_present il0324_present
#define epd_controller_present_partial il0324_present_partial
#define epd_controller_sleep il0324_sleep
#else
#include "platform/ssd1677.h"
typedef ssd1677_io_t epd_controller_io_t;
typedef ssd1677_t epd_controller_t;
#define epd_controller_init ssd1677_init
#define epd_controller_present ssd1677_present
#define epd_controller_present_partial ssd1677_present_partial
#define epd_controller_sleep ssd1677_sleep
#endif

#endif
