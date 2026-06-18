# ESP32 Hardware Skeleton Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a first real-device ESP32 build path that boots under ESP-IDF/PlatformIO and reuses the existing simulator UI renderer.

**Architecture:** Keep the portable `gfx`, `font`, `app`, and `ui` modules shared between host simulators and ESP firmware. Add an ESP-specific `app_main` and an `esp_display` platform adapter that currently logs framebuffer output and later becomes the concrete E-Ink SPI driver.

**Tech Stack:** C11, PlatformIO, ESP-IDF 5.1.x/5.2.x, ESP32 N16R8-style 16MB flash + PSRAM configuration.

## Global Constraints

- Preserve existing `reader_sim` and `reader_sim_sdl` host builds.
- Keep all UI state/rendering portable; ESP-specific code stays in `src/main_esp.c` and `src/platform/esp_display.*`.
- Do not require hardware-specific EPD controller details in this first skeleton.
- Host `make test` must verify the ESP project files exist and reference the shared renderer.
- Real hardware build command is `pio run -e esp32-n16r8`.

---

### Task 1: Guard ESP Project Files With Host Tests

**Files:**
- Modify: `tests/test_runner.c`

**Interfaces:**
- Consumes: standard C file I/O.
- Produces: host tests that fail when ESP project files or shared source references are missing.

Steps:
- [x] Add tests that check `platformio.ini`, `partitions_16mb.csv`, `sdkconfig.defaults`, `CMakeLists.txt`, and `src/CMakeLists.txt`.
- [x] Run `make test`; expect failure before the files exist.

### Task 2: Add PlatformIO/ESP-IDF Project Files

**Files:**
- Create: `platformio.ini`
- Create: `partitions_16mb.csv`
- Create: `sdkconfig.defaults`
- Create: `CMakeLists.txt`
- Create: `src/CMakeLists.txt`

**Interfaces:**
- Produces: `pio run -e esp32-n16r8` project layout.

Steps:
- [ ] Add ESP32 N16R8 PlatformIO environment using ESP-IDF.
- [ ] Add 16MB OTA partition table.
- [ ] Add sdkconfig defaults for PSRAM, logs, FATFS LFN, and certificate bundle.
- [ ] Register portable UI/rendering source files plus ESP-specific platform files in CMake.
- [ ] Run `make test`; expect pass for project-file tests.

### Task 3: Add ESP Runtime Entry And Display Adapter

**Files:**
- Create: `src/main_esp.c`
- Create: `src/platform/esp_display.h`
- Create: `src/platform/esp_display.c`
- Modify: `README.md`
- Modify: `docs/PROGRESS.md`
- Modify: `requires01.md`

**Interfaces:**
- Produces: `esp_display_t`, `esp_display_init`, `esp_display_present`, `esp_display_sleep`.
- Consumes: `gfx_framebuffer_t`, `font_load_default`, `app_init`, `ui_render_page`.

Steps:
- [ ] Implement `app_main` that initializes app state, framebuffer, font, renders the home page, and presents it through `esp_display`.
- [ ] Implement `esp_display_present` as a real-device-safe logging adapter that counts black/red pixels until the exact EPD controller is wired.
- [ ] Document `pio run -e esp32-n16r8`, `pio run -e esp32-n16r8 -t upload`, and `pio device monitor`.
- [ ] Run `make test`, `make reader_sim`, and `make reader_sim_sdl`.
- [ ] If PlatformIO is available locally, run `pio run -e esp32-n16r8`; otherwise record that hardware build was not executed in this environment.

## Self-Review

- Scope is limited to the first true ESP build skeleton, not the final EPD SPI driver.
- No placeholders remain in the task descriptions.
- Source boundaries keep host and ESP platform code separate.
