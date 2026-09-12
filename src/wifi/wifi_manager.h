#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

#define AP_SSID         "RemoteMapper-AP"

#ifdef __cplusplus
extern "C" {
#endif

void   wifi_manager_init(void);
void   wifi_manager_task(void);
bool   wifi_manager_get_enabled(void);
bool   wifi_manager_set_enabled(bool enabled);
bool   wifi_manager_is_ap_running(void);
String wifi_manager_get_ap_ip(void);
String wifi_manager_get_sta_ip(void);
bool   wifi_manager_is_sta_connected(void);
int8_t wifi_manager_get_sta_rssi(void);
String wifi_manager_scan_json(void);
bool   wifi_manager_save_sta_config(const String& ssid, const String& password);
String wifi_manager_get_ap_pass(void);
bool   wifi_manager_save_ap_config(const String& ap_password);

#ifdef __cplusplus
}
#endif
