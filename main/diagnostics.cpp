#include <atomic>
#include <string.h>

#include "esp_attr.h"
#include "esp_heap_caps.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "diagnostics.h"
#include "http_server/http_utils.h"   // extern httpd_handle_t http_server

static const char *TAG = "diag";

void DiagResetHistory::push(uint8_t reason, uint32_t uptime_s)
{
    if (count == DIAG_RESET_HISTORY) {
        memmove(&entries[0], &entries[1], sizeof(entries[0]) * (DIAG_RESET_HISTORY - 1));
        count--;
    }
    entries[count].reason = reason;
    entries[count].uptime_s = uptime_s;
    count++;
}

// survives soft resets, panics and watchdog resets; lost on power loss
struct DiagRtc {
    uint32_t magic;
    uint32_t uptime_s;
    uint32_t boots;
    DiagResetHistory history;
};
static RTC_NOINIT_ATTR DiagRtc s_rtc;
#define DIAG_MAGIC 0xD1A60001u

static std::atomic<uint32_t> s_http_file_errors{0};
static std::atomic<uint32_t> s_peak_sockets{0};
static std::atomic<uint32_t> s_cur_sockets{0};

void diag_boot(esp_reset_reason_t reason)
{
    if (reason == ESP_RST_POWERON || s_rtc.magic != DIAG_MAGIC || s_rtc.history.count > DIAG_RESET_HISTORY) {
        memset(&s_rtc, 0, sizeof(s_rtc));
        s_rtc.magic = DIAG_MAGIC;
    } else {
        s_rtc.history.push((uint8_t) reason, s_rtc.uptime_s);
    }
    s_rtc.boots++;
    s_rtc.uptime_s = 0;
}

void diag_count_http_file_error()
{
    s_http_file_errors++;
}

static void diag_task(void *)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        s_rtc.uptime_s = (uint32_t) (esp_timer_get_time() / 1000000);

        if (http_server) {
            size_t n = 16;
            int fds[16];
            if (httpd_get_client_list(http_server, &n, fds) == ESP_OK) {
                s_cur_sockets = (uint32_t) n;
                if (n > s_peak_sockets) {
                    s_peak_sockets = (uint32_t) n;
                }
            }
        }
    }
}

void diag_start()
{
    xTaskCreate(diag_task, "diag", 3072, NULL, 1, NULL);
}

static const char *reason_name(uint8_t r)
{
    switch (r) {
        case ESP_RST_POWERON: return "poweron";
        case ESP_RST_EXT: return "external";
        case ESP_RST_SW: return "software";
        case ESP_RST_PANIC: return "panic";
        case ESP_RST_INT_WDT: return "int_wdt";
        case ESP_RST_TASK_WDT: return "task_wdt";
        case ESP_RST_WDT: return "other_wdt";
        case ESP_RST_DEEPSLEEP: return "deepsleep";
        case ESP_RST_BROWNOUT: return "brownout";
        default: return "other";
    }
}

void diag_to_json(JsonObject obj)
{
    obj["minFreeHeapInt"]      = heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL);
    obj["largestBlockInt"]     = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    obj["minFreeHeapPsram"]    = heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM);
    obj["largestBlockPsram"]   = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    obj["httpSockets"]         = s_cur_sockets.load();
    obj["httpSocketsPeak"]     = s_peak_sockets.load();
    obj["httpFileErrors"]      = s_http_file_errors.load();
    obj["boots"]               = s_rtc.boots;

    JsonArray hist = obj["resetHistory"].to<JsonArray>();
    for (int i = s_rtc.history.count - 1; i >= 0; i--) {   // newest first
        JsonObject e = hist.add<JsonObject>();
        e["reason"] = reason_name(s_rtc.history.entries[i].reason);   // static string
        e["uptimeSeconds"] = s_rtc.history.entries[i].uptime_s;
    }
}
