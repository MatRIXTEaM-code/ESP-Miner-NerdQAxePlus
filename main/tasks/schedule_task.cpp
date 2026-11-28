#include <time.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "global_state.h"
#include "nvs_config.h"
#include "schedule_task.h"

static const char *TAG = "schedule";

bool schedule_in_window(int minute_of_day, int start, int end)
{
    if (start == end) {
        return true;
    }
    if (start < end) {
        return minute_of_day >= start && minute_of_day < end;
    }
    // wraps past midnight, e.g. 22:00 -> 06:00
    return minute_of_day >= start || minute_of_day < end;
}

extern bool is_time_synced(void);

void schedule_task(void *pvParameters)
{
    // let the system settle (and NTP sync) before the first decision
    vTaskDelay(pdMS_TO_TICKS(60 * 1000));

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(30 * 1000));

        if (!Config::isScheduleEnabled()) {
            continue;
        }
        // fail safe: with no valid clock keep mining as normal
        if (!is_time_synced()) {
            continue;
        }

        time_t now = 0;
        time(&now);
        struct tm tm_info;
        localtime_r(&now, &tm_info);
        int minute = tm_info.tm_hour * 60 + tm_info.tm_min;

        bool inside = schedule_in_window(minute, Config::getScheduleStart(), Config::getScheduleEnd());
        bool isDown = POWER_MANAGEMENT_MODULE.isShutdown();

        if (!inside && !isDown) {
            ESP_LOGW(TAG, "outside mining window (%02d:%02d), shutting down ASICs", tm_info.tm_hour, tm_info.tm_min);
            POWER_MANAGEMENT_MODULE.shutdown();
        } else if (inside && isDown) {
            ESP_LOGW(TAG, "mining window started (%02d:%02d), restarting", tm_info.tm_hour, tm_info.tm_min);
            POWER_MANAGEMENT_MODULE.restart();
        }
    }
}
