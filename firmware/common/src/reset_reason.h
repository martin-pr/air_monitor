#pragma once

#include <esp_system.h>

// Short human-readable label for a reset reason, prefixed with "RST: ".
// Used on the boot-diagnostic column of the display.
const char* resetReasonMessage(esp_reset_reason_t reason);
