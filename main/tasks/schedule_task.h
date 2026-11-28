#pragma once

#include <stdbool.h>

// Pure helper (unit-testable): is `minute_of_day` inside [start, end)?
// Windows may wrap past midnight (start > end). start == end means "always".
bool schedule_in_window(int minute_of_day, int start, int end);

// Optional mining schedule (e.g. solar): shuts the ASICs down outside the
// configured local-time window and reboots the board at the start of it.
// Disabled by default. Does nothing until the clock is synced from NTP.
void schedule_task(void *pvParameters);
