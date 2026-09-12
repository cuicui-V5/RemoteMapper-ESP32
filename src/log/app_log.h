#pragma once

#include <stdint.h>
#include <stddef.h>
#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_log_init(void);
void app_log(const char* tag, const char* format, ...);
String app_log_get_json(void);
void app_log_clear(void);
void app_log_set_cdc_enabled(bool enabled);
bool app_log_get_cdc_enabled(void);

#ifdef __cplusplus
}
#endif
