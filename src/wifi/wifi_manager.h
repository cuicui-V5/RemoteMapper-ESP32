#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <Arduino.h>

#define AP_SSID         "RemoteMapper-AP"

typedef enum {
    WIFI_POLICY_ALWAYS_ON = 0,
    WIFI_POLICY_ON_DEMAND = 1,
    WIFI_POLICY_DISABLED  = 2
} wifi_policy_t;

typedef enum {
    WIFI_STATE_OFF           = 0,
    WIFI_STATE_ENABLING      = 1,
    WIFI_STATE_ON            = 2,
    WIFI_STATE_SHUTTING_DOWN = 3
} wifi_radio_state_t;

typedef enum {
    WIFI_WAKE_SYSTEM = 0,
    WIFI_WAKE_MANUAL = 1,
    WIFI_WAKE_WEB_UI = 2,
    WIFI_WAKE_WOL    = 3
} wifi_wake_reason_t;

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
// Persisted STA credentials (for config backup/restore)
String wifi_manager_get_sta_ssid(void);
String wifi_manager_get_sta_pass(void);
// Atomically persist a full Wi-Fi config from a backup restore. Does not touch
// the radio; the caller is expected to reboot afterwards for policy to apply.
bool wifi_manager_restore_backup(const String& ssid, const String& sta_pass,
                                 const String& ap_pass, wifi_policy_t policy,
                                 uint32_t timeout_min);

// ---- Wi-Fi Power Management (Policy / State Machine) ----
wifi_policy_t      wifi_manager_get_policy(void);
bool               wifi_manager_set_policy(wifi_policy_t policy);
uint32_t           wifi_manager_get_timeout_min(void);
bool               wifi_manager_set_timeout_min(uint32_t minutes);
wifi_radio_state_t wifi_manager_get_radio_state(void);
// Request the radio to be available (broadcasts/WOL/API access). Phase 2 keeps
// the radio always on when enabled; idle shutdown lands in a later phase.
bool               wifi_manager_request_wifi(wifi_wake_reason_t reason);
// Refresh the "last user-initiated activity" timestamp (idle timeout anchor).
void               wifi_manager_mark_activity(void);
// Milliseconds since boot of the last user-initiated activity (0 = none yet).
uint32_t           wifi_manager_get_last_activity_ms(void);
const char*        wifi_manager_policy_str(wifi_policy_t policy);
const char*        wifi_manager_state_str(wifi_radio_state_t state);

#ifdef __cplusplus
}
#endif
