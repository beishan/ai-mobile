# Current Development Target

The active project target is an ESP32-S3 N16R8 reader for a 4.26 inch 480 x 800 black/white high-refresh E-Ink panel driven by SSD1677 over SPI.

Current scope:

- Shared 480 x 800 framebuffer.
- Black/white logical color only.
- Single 48,000-byte 1bpp display plane.
- Home modules: reading, SD files, weather, calendar, English, settings, and about.
- Reader-first simulator and firmware with a full-refresh SSD1677 hardware path.
- SDL and PPM simulators share the same app state and renderer as the ESP32 firmware.
- Reader book metadata and page text flow through `src/app/reader_library.*`; ESP32 firmware mounts `/sdcard` and loads UTF-8 TXT files from `/sdcard/books` through that boundary.
- Source text supports both explicit form-feed page breaks and automatic UTF-8-safe page splitting.

Obsolete historical plans for the former small-format color-accent simulator have been removed from the current docs tree so new work follows this target.
