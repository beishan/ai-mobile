#ifndef EPD_PANEL_H
#define EPD_PANEL_H

/* Select exactly one physical panel at build time.  The original 4.26-inch
 * panel remains the default so existing builds keep their current behavior. */
#define EPD_PANEL_OSPTEK_426 1
#define EPD_PANEL_HINK_E037A03_A1 2

#ifndef EPD_PANEL_MODEL
#define EPD_PANEL_MODEL EPD_PANEL_OSPTEK_426
#endif

#if EPD_PANEL_MODEL == EPD_PANEL_HINK_E037A03_A1
#define EPD_PANEL_NAME "HINK-E037A03-A1 3.7in 240x416 BW"
#define EPD_DRIVER_NAME "UC8171/IL0324"
#define EPD_NATIVE_WIDTH 240
#define EPD_NATIVE_HEIGHT 416
#define EPD_FRAME_BYTES ((EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT) / 8)
#define EPD_BUSY_ACTIVE_LEVEL 0
#define EPD_NATIVE_PORTRAIT 1
#elif EPD_PANEL_MODEL == EPD_PANEL_OSPTEK_426
#define EPD_PANEL_NAME "4.26in 480x800 BW E-Ink"
#define EPD_DRIVER_NAME "SSD1677"
#define EPD_NATIVE_WIDTH 800
#define EPD_NATIVE_HEIGHT 480
#define EPD_FRAME_BYTES ((EPD_NATIVE_WIDTH * EPD_NATIVE_HEIGHT) / 8)
#define EPD_BUSY_ACTIVE_LEVEL 1
#define EPD_NATIVE_PORTRAIT 0
#else
#error "Unsupported EPD_PANEL_MODEL"
#endif

#endif
