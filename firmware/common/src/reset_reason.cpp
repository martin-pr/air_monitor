#include "reset_reason.h"

const char* resetReasonMessage(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_BROWNOUT: return "RST: brownout";
        case ESP_RST_PANIC:    return "RST: panic";
        case ESP_RST_TASK_WDT: return "RST: task wdt";
        case ESP_RST_INT_WDT:  return "RST: int wdt";
        case ESP_RST_WDT:      return "RST: rtc wdt";
        case ESP_RST_SW:       return "RST: software";
        default:               return "RST: poweron/pin";
    }
}
