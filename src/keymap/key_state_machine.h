#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "key_definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    ACTION_NONE = 0,
    ACTION_KEYBOARD_TAP,        // Tap a key or combo (Down + Up)
    ACTION_KEYBOARD_HOLD,       // Press and hold down
    ACTION_KEYBOARD_RELEASE,    // Release key
    ACTION_CONSUMER_TAP,        // Tap a consumer control usage (e.g. Vol Up/Down)
    ACTION_CONSUMER_HOLD,
    ACTION_CONSUMER_RELEASE,
    ACTION_VOICE_HOLD,          // Trigger voice recording + hold hotkey
    ACTION_VOICE_RELEASE,       // End voice recording + release hotkey
    ACTION_SWITCH_LAYER,        // Switch to target layer (auto-toggles to 0 if current == target)
    ACTION_TRANSPARENT,         // Transparent / Inherit from Layer 0
    ACTION_WOL,                 // 11: Wake-on-LAN Magic Packet
    ACTION_ADV_MACRO,           // 12: Future placeholder (Macro Sequence)
    ACTION_ADV_HTTP,            // 13: Future placeholder (HTTP Webhook)
    ACTION_ADV_MQTT             // 14: Future placeholder (MQTT Publish)
} key_action_type_t;

typedef struct {
    key_action_type_t type;
    uint8_t           modifier;     // USB_MOD_*
    uint8_t           key_code;     // USB_KEY_*
    uint16_t          consumer_code;// USB_CONSUMER_*
    uint8_t           target_layer; // Target layer (0 ~ 4) for ACTION_SWITCH_LAYER
    uint8_t           wol_mac[6];   // Target MAC address for ACTION_WOL
} key_action_t;

typedef struct {
    uint8_t       source_vk;        // MI_KEY_*
    bool          has_click;
    key_action_t  click_action;
    bool          has_long;
    key_action_t  long_action;
    uint16_t      long_ms;          // e.g. 500ms
    bool          has_double;
    key_action_t  double_action;
    uint16_t      double_ms;        // e.g. 250ms
    bool          has_repeat;
    key_action_t  repeat_action;
    uint16_t      repeat_delay_ms;  // e.g. 400ms
    uint16_t      repeat_interval_ms;// e.g. 80ms
} key_binding_t;

typedef struct {
    bool     is_pressed;
    uint32_t press_timestamp;
    uint32_t release_timestamp;
    uint8_t  press_count;
    bool     long_fired;
    uint32_t next_repeat_timestamp;
    bool     waiting_double;
} key_slot_state_t;

typedef struct {
    uint8_t  source_vk;
    bool     is_pressed;
    uint32_t timestamp;
    uint32_t duration_ms;
    uint8_t  action_type;
    uint8_t  modifier;
    uint8_t  key_code;
    uint16_t consumer_code;
    uint8_t  active_layer;
} key_event_telemetry_t;

#define MAX_KEY_BINDINGS   16
#define MAX_LAYERS         5
#define MAX_LAYER_NAME_LEN 24

typedef enum {
    LAYER_TYPE_PERSISTENT = 0,  // Stays in layer until another layer switch
    LAYER_TYPE_ONESHOT    = 1,  // Reverts to Layer 0 after one key action fires
    LAYER_TYPE_TIMEOUT    = 2   // Reverts to Layer 0 after timeout_sec of idle time
} layer_type_t;

typedef struct {
    char          name[MAX_LAYER_NAME_LEN]; // e.g. "默认层", "层1"
    layer_type_t  type;                     // LAYER_TYPE_*
    uint16_t      timeout_sec;              // 3 ~ 300s (for LAYER_TYPE_TIMEOUT)
    uint32_t      led_color;                // RGB 0x00RRGGBB (e.g. 0x00FF00)
    key_binding_t bindings[MAX_KEY_BINDINGS];
    size_t        binding_count;
} key_layer_t;

typedef void (*key_output_callback_t)(const key_action_t *action);

typedef struct {
    key_layer_t           layers[MAX_LAYERS];
    size_t                layer_count;          // Always MAX_LAYERS (5)
    uint8_t               active_layer;         // Currently active layer (0 ~ 4)
    uint32_t              last_activity_time;   // Timestamp of last key action
    key_slot_state_t      states[MAX_KEY_BINDINGS];
    key_output_callback_t output_cb;
    key_event_telemetry_t last_telemetry;
} key_mapper_engine_t;

/**
 * @brief Initialize key mapper engine with default remote mapping table
 */
void key_engine_init(key_mapper_engine_t *engine, key_output_callback_t cb);

/**
 * @brief Load default factory key mapping table for all layers
 */
void key_engine_load_defaults(key_mapper_engine_t *engine);

/**
 * @brief Switch active layer with automatic toggle (if target == current -> revert to 0)
 */
void key_engine_switch_layer(key_mapper_engine_t *engine, uint8_t target_layer, uint32_t now_ms);

/**
 * @brief Get currently active layer index (0 ~ 4)
 */
uint8_t key_engine_get_active_layer(const key_mapper_engine_t *engine);

/**
 * @brief Set or update a key binding in a specific layer
 */
bool key_engine_set_layer_binding(key_mapper_engine_t *engine, uint8_t layer_idx, const key_binding_t *binding);

/**
 * @brief Get binding for a key code in a specific layer
 */
bool key_engine_get_layer_binding(const key_mapper_engine_t *engine, uint8_t layer_idx, uint8_t source_vk, key_binding_t *out_binding);

/**
 * @brief Set or update a key binding in Layer 0 (backward compatible)
 */
bool key_engine_set_binding(key_mapper_engine_t *engine, const key_binding_t *binding);

/**
 * @brief Get binding for a key code in Layer 0 (backward compatible)
 */
bool key_engine_get_binding(const key_mapper_engine_t *engine, uint8_t source_vk, key_binding_t *out_binding);

/**
 * @brief Feed raw physical key event from BLE HOGP
 */
void key_engine_feed_key(key_mapper_engine_t *engine, uint8_t raw_key_code, bool is_pressed, uint32_t now_ms);

/**
 * @brief Periodic timer tick to evaluate long press, double click timeout, and layer idle timeout
 */
void key_engine_tick(key_mapper_engine_t *engine, uint32_t now_ms);

/**
 * @brief Forcefully release all active pressed keys and reset state
 */
void key_engine_release_all(key_mapper_engine_t *engine, uint32_t now_ms);


#ifdef __cplusplus
}
#endif
