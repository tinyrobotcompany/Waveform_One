#pragma once
#include "FreeRTOS.h"
inline void vTaskDelay(TickType_t) {}
inline void vTaskDelete(void*) {}
inline int xTaskCreatePinnedToCore(void (*)(void*), const char*, unsigned, void*, unsigned, void*, unsigned) { return pdPASS; }
