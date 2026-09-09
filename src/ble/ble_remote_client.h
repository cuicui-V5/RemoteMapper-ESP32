#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <Arduino.h>
#include "app_config.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BLE_STATE_DISCONNECTED = 0,
    BLE_STATE_SCANNING,
    BLE_STATE_CONNECTING,
    BLE_STATE_CONNECTED,
    BLE_STATE_TALKING
} ble_remote_state_t;

/**
 * @brief Initialize NimBLE Client for Xiaomi Remote
 */
void ble_remote_init(void);

/**
 * @brief Main BLE task tick (handles state machine, watchdogs, and keep-alive packets)
 */
void ble_remote_task(void);

/**
 * @brief Get current BLE connection state
 */
ble_remote_state_t ble_remote_get_state(void);
/** @brief Whether the connected remote advertised a supported ATVV codec. */
bool ble_remote_voice_ready(void);

/**
 * @brief Trigger manual reconnect / re-scan
 */
void ble_remote_trigger_reconnect(void);

/**
 * @brief Scan for nearby BLE devices and return JSON list
 */
String ble_remote_scan_devices_json(void);

/**
 * @brief Manually connect and pair to specific BLE device by MAC address, address type, and optional name
 */
bool ble_remote_connect_target(const String& mac_str, uint8_t addr_type, const String& dev_name);

/**
 * @brief Manually connect and pair to specific BLE device by MAC address
 */
bool ble_remote_connect_mac(const String& mac_str);

/**
 * @brief Unpair and clear saved remote MAC address
 */
void ble_remote_unpair(void);

/**
 * @brief Get connected remote name and MAC
 */
String ble_remote_get_connected_info(void);

#ifdef __cplusplus
}
#endif
