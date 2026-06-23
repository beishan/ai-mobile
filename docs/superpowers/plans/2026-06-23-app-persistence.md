# App Persistence Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a portable persistence snapshot for reader progress, bookmarks, and settings.

**Architecture:** Keep live UI state in `app_state_t`. Add `src/app/app_persistence.{h,c}` for durable-field capture, apply, encode, and decode. Tests exercise the module through the existing C test runner.

**Tech Stack:** C11, existing Makefile test runner, ESP-IDF component source list.

## Global Constraints

- Shared code must build for desktop simulators and ESP32 firmware.
- No hardware storage dependency in this task.
- The encoded payload must be versioned and text-based.
- Restore must clamp values against current book counts and setting ranges.

---

### Task 1: Portable Persistence Snapshot

**Files:**
- Create: `src/app/app_persistence.h`
- Create: `src/app/app_persistence.c`
- Modify: `tests/test_runner.c`
- Modify: `Makefile`
- Modify: `src/CMakeLists.txt`

**Interfaces:**
- Produces: `app_persisted_state_t`, `app_persistence_capture`, `app_persistence_apply`, `app_persistence_encode`, `app_persistence_decode`.

- [x] **Step 1: Write failing tests**

Add tests for snapshot capture/apply, encode/decode round trip, clamping, and malformed payload rejection.

- [x] **Step 2: Run test to verify it fails**

Run: `make test`

Expected: compile failure because `app/app_persistence.h` does not exist.

- [x] **Step 3: Implement module**

Create the header and source, add them to desktop and ESP32 build source lists, and keep implementation independent of filesystem or NVS.

- [x] **Step 4: Run tests**

Run: `make test`

Expected: `tests passed`

- [x] **Step 5: Build simulators**

Run: `make reader_sim` and `make reader_sim_sdl`

Expected: both binaries build successfully.
