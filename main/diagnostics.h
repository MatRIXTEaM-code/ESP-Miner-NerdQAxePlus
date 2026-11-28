#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_system.h"
#include "ArduinoJson.h"

// Read-only runtime diagnostics for hunting long-uptime bugs. Nothing here
// changes mining, voltage, frequency or fan behaviour.

#define DIAG_RESET_HISTORY 8

struct DiagResetEntry {
    uint8_t reason;          // esp_reset_reason_t
    uint32_t uptime_s;       // how long the previous run lasted (last 10 s tick)
};

// Pure ring-buffer helper (host-testable): keeps the newest `cap` entries.
struct DiagResetHistory {
    DiagResetEntry entries[DIAG_RESET_HISTORY];
    uint8_t count;
    void clear() { count = 0; }
    void push(uint8_t reason, uint32_t uptime_s);
};

// call once early at boot, after the reset reason is known
void diag_boot(esp_reset_reason_t reason);

// start the 10 s housekeeping task (persists uptime across soft resets,
// tracks the peak number of open web-server sockets)
void diag_start();

// counters bumped by the web server
void diag_count_http_file_error();

// fills "diag" object for the system API
void diag_to_json(JsonObject obj);
