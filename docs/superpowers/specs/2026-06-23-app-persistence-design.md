# App Persistence Design

## Goal

Persist the reader state that users expect to survive restart: per-book current page, per-book bookmark page, recent book, font size, line spacing, WiFi state, city, and power-saving state.

## Scope

This change adds a small app-layer snapshot API and a portable text codec. It does not add ESP32 NVS writes yet, real SD card storage, weather networking, or book file indexing.

## Architecture

`app_state` remains the owner of live navigation and UI state. A new `app_persistence` module converts only durable fields to and from `app_persisted_state_t`. The codec uses a simple line-oriented versioned text format so tests can verify behavior without hardware and ESP32 can later store the same payload in NVS.

## Data Flow

On save, callers pass `app_state_t` to `app_persistence_capture`, then encode the snapshot with `app_persistence_encode`. On restore, callers decode text into `app_persisted_state_t`, initialize the app normally with `app_init`, then call `app_persistence_apply` so dynamic book page counts are already available for clamping.

## Validation

Restore clamps page indexes to current book page counts and settings to known option ranges. Invalid or unsupported payloads fail without modifying the app state.

## Testing

Unit tests cover capture, encode/decode round trip, clamped restore after page-count changes, and rejection of malformed payloads.
