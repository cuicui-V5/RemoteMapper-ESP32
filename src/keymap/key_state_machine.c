#include "key_state_machine.h"
#include "wol_manager.h"
#include "app_config.h"
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SemaphoreHandle_t s_key_engine_mutex = NULL;

static void key_engine_lock(void) {
    if (!s_key_engine_mutex) {
        s_key_engine_mutex = xSemaphoreCreateRecursiveMutex();
    }
    if (s_key_engine_mutex) {
        xSemaphoreTakeRecursive(s_key_engine_mutex, portMAX_DELAY);
    }
}

static void key_engine_unlock(void) {
    if (s_key_engine_mutex) {
        xSemaphoreGiveRecursive(s_key_engine_mutex);
    }
}

extern void app_log(const char* tag, const char* format, ...);
extern void led_indicator_set_layer_color(uint32_t rgb_color);

static int get_physical_key_slot(uint8_t raw_key) {
    switch (raw_key) {
        case MI_KEY_POWER: case MI_KEY_POWER_ALT: return 0;
        case MI_KEY_VOICE: case MI_KEY_VOICE_ALT: return 1;
        case MI_KEY_UP:    return 2;
        case MI_KEY_DOWN:  return 3;
        case MI_KEY_LEFT:  return 4;
        case MI_KEY_RIGHT: return 5;
        case MI_KEY_OK:    return 6;
        case MI_KEY_BACK:  return 7;
        case MI_KEY_HOME: case MI_KEY_HOME_ALT: return 8;
        case MI_KEY_MENU: case MI_KEY_MENU_ALT: return 9;
        case MI_KEY_VOL_UP:   return 10;
        case MI_KEY_VOL_DOWN: return 11;
        case MI_KEY_TV: case MI_KEY_TV_ALT: return 12;
        default: return -1;
    }
}

static uint8_t canonical_source_vk(uint8_t raw_key) {
    switch (raw_key) {
        case MI_KEY_POWER_ALT: return MI_KEY_POWER;
        case MI_KEY_VOICE_ALT: return MI_KEY_VOICE;
        case MI_KEY_HOME_ALT:  return MI_KEY_HOME;
        case MI_KEY_MENU_ALT:  return MI_KEY_MENU;
        case MI_KEY_TV_ALT:    return MI_KEY_TV;
        default: return raw_key;
    }
}

static int find_binding_index_in_layer(const key_layer_t *layer, uint8_t raw_key) {
    if (!layer) return -1;
    uint8_t can_vk = canonical_source_vk(raw_key);
    for (size_t i = 0; i < layer->binding_count; i++) {
        if (canonical_source_vk(layer->bindings[i].source_vk) == can_vk) {
            return (int)i;
        }
    }
    return -1;
}

// Merge layer override with Layer 0 base (Layer Transparency)
static void get_effective_binding(const key_mapper_engine_t *engine, uint8_t raw_key, key_binding_t *out_b) {
    memset(out_b, 0, sizeof(key_binding_t));
    out_b->source_vk = canonical_source_vk(raw_key);

    // 1. Start with Layer 0 base
    int idx0 = find_binding_index_in_layer(&engine->layers[0], raw_key);
    if (idx0 >= 0) {
        *out_b = engine->layers[0].bindings[idx0];
    }

    // 2. Overlay active layer if > 0
    uint8_t cur = engine->active_layer;
    if (cur > 0 && cur < MAX_LAYERS) {
        int cur_idx = find_binding_index_in_layer(&engine->layers[cur], raw_key);
        if (cur_idx >= 0) {
            const key_binding_t *ov = &engine->layers[cur].bindings[cur_idx];
            if (ov->has_click) {
                if (ov->click_action.type == ACTION_TRANSPARENT) {
                    // Transparent: keep Layer 0 click
                } else {
                    out_b->has_click = true;
                    out_b->click_action = ov->click_action;
                }
            }
            if (ov->has_long) {
                if (ov->long_action.type == ACTION_TRANSPARENT) {
                    // Transparent: keep Layer 0 long
                } else {
                    out_b->has_long = true;
                    out_b->long_action = ov->long_action;
                    out_b->long_ms = ov->long_ms;
                }
            }
            if (ov->has_double) {
                if (ov->double_action.type == ACTION_TRANSPARENT) {
                    // Transparent: keep Layer 0 double
                } else {
                    out_b->has_double = true;
                    out_b->double_action = ov->double_action;
                    out_b->double_ms = ov->double_ms;
                }
            }
            if (ov->has_repeat) {
                out_b->has_repeat = ov->has_repeat;
                out_b->repeat_action = ov->repeat_action;
                out_b->repeat_delay_ms = ov->repeat_delay_ms;
                out_b->repeat_interval_ms = ov->repeat_interval_ms;
            }
        }
    }
}

static void emit_action(key_mapper_engine_t *engine, const key_action_t *action, uint8_t source_vk, bool is_down) {
    if (!engine || !action || action->type == ACTION_NONE || action->type == ACTION_TRANSPARENT) return;

    // Record last telemetry event
    engine->last_telemetry.source_vk = source_vk;
    engine->last_telemetry.is_pressed = is_down;
    engine->last_telemetry.action_type = action->type;
    engine->last_telemetry.modifier = action->modifier;
    engine->last_telemetry.key_code = action->key_code;
    engine->last_telemetry.consumer_code = action->consumer_code;
    engine->last_telemetry.active_layer = engine->active_layer;

    // Layer switch action with auto-toggle
    if (action->type == ACTION_SWITCH_LAYER) {
        uint8_t target = action->target_layer;
        uint8_t next_layer = target;
        // Auto-toggle: If already in target layer, toggle back to Layer 0!
        if (engine->active_layer == target) {
            next_layer = 0;
        }
        key_engine_switch_layer(engine, next_layer, engine->last_telemetry.timestamp);
        return;
    }

    // Wake-on-LAN Action
    if (action->type == ACTION_WOL) {
        wol_manager_send(action->wol_mac, 9);
        return;
    }

    if (engine->output_cb) {
        engine->output_cb(action);
    }

    // Check One-Shot Layer auto-revert
    if (engine->active_layer != 0 && engine->layers[engine->active_layer].type == LAYER_TYPE_ONESHOT) {
        if (action->type == ACTION_KEYBOARD_TAP || action->type == ACTION_CONSUMER_TAP ||
            action->type == ACTION_KEYBOARD_RELEASE || action->type == ACTION_CONSUMER_RELEASE ||
            action->type == ACTION_VOICE_RELEASE) {
            key_engine_switch_layer(engine, 0, engine->last_telemetry.timestamp);
        }
    }
}

static void emit_action_as_tap_if_hold(key_mapper_engine_t *engine, const key_action_t *action, uint8_t source_vk) {
    if (!action || action->type == ACTION_NONE || action->type == ACTION_TRANSPARENT) return;
    if (action->type == ACTION_KEYBOARD_HOLD) {
        key_action_t tap = { ACTION_KEYBOARD_TAP, action->modifier, action->key_code, 0, 0 };
        emit_action(engine, &tap, source_vk, false);
    } else if (action->type == ACTION_CONSUMER_HOLD) {
        key_action_t tap = { ACTION_CONSUMER_TAP, 0, 0, action->consumer_code, 0 };
        emit_action(engine, &tap, source_vk, false);
    } else {
        emit_action(engine, action, source_vk, false);
    }
}

void key_engine_switch_layer(key_mapper_engine_t *engine, uint8_t target_layer, uint32_t now_ms) {
    if (!engine) return;
    if (target_layer >= MAX_LAYERS) target_layer = 0;
    if (engine->active_layer == target_layer) return;

    // Force release all active pressed keys to prevent sticky keys
    key_engine_release_all(engine, now_ms);

    engine->active_layer = target_layer;
    engine->last_activity_time = now_ms;

    // Update LED color to match current layer
    uint32_t color = engine->layers[target_layer].led_color;
    led_indicator_set_layer_color(color);

    app_log("KEYMAP", "Layer Switched -> [%u: %s]", target_layer, engine->layers[target_layer].name);
}

uint8_t key_engine_get_active_layer(const key_mapper_engine_t *engine) {
    if (!engine) return 0;
    return engine->active_layer;
}

void key_engine_load_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;

    memset(engine->layers, 0, sizeof(engine->layers));
    engine->layer_count = MAX_LAYERS;
    engine->active_layer = 0;
    engine->last_activity_time = 0;

    // ----------------------------------------------------
    // Layer 0: 默认层
    // ----------------------------------------------------
    key_layer_t *l0 = &engine->layers[0];
    strncpy(l0->name, "默认层", sizeof(l0->name) - 1);
    l0->type = LAYER_TYPE_PERSISTENT;
    l0->timeout_sec = 0;
    l0->led_color = 0x00FF00; // Green
    l0->binding_count = 0;

    // 1. Power: 0x66 -> Click: Alt+Tab, Long: Sleep
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_POWER;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_LALT, USB_KEY_TAB, 0, 0 };
        b.has_long = true;
        b.long_ms = 600;
        b.long_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_SLEEP, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 2. Voice: 0x04 -> Voice Hold & Release
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOICE;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_VOICE_HOLD, DEFAULT_VOICE_MODIFIER, DEFAULT_VOICE_KEY, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 3. D-Pad Up: 0x52 -> Keyboard Up
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_UP;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_UP, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 4. D-Pad Down: 0x51 -> Keyboard Down
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_DOWN;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_DOWN, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 5. D-Pad Left: 0x50 -> Keyboard Left
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_LEFT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_LEFT, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 6. D-Pad Right: 0x4F -> Keyboard Right
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_RIGHT;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_HOLD, USB_MOD_NONE, USB_KEY_RIGHT, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 7. OK: 0x28 -> Enter
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_OK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_RETURN, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 8. Back: 0xF1 -> Browser Back (AC Back)
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_BACK;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_AC_BACK, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 9. Home: 0x24 -> Win+D
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_HOME;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_LGUI, USB_KEY_D, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 10. Menu: 0x5D -> Space
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_MENU;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_SPACE, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // 11. Vol+: 0x80 -> Vol Up with repeat
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOL_UP;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_UP, 0 };
        b.has_repeat = true;
        b.repeat_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_UP, 0 };
        b.repeat_delay_ms = 350;
        b.repeat_interval_ms = 70;
        l0->bindings[l0->binding_count++] = b;
    }

    // 12. Vol-: 0x81 -> Vol Down with repeat
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_VOL_DOWN;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_DOWN, 0 };
        b.has_repeat = true;
        b.repeat_action = (key_action_t){ ACTION_CONSUMER_TAP, USB_MOD_NONE, USB_KEY_NONE, USB_CONSUMER_VOLUME_DOWN, 0 };
        b.repeat_delay_ms = 350;
        b.repeat_interval_ms = 70;
        l0->bindings[l0->binding_count++] = b;
    }

    // 13. TV: 0xC0 -> Click: F8
    {
        key_binding_t b;
        memset(&b, 0, sizeof(b));
        b.source_vk = MI_KEY_TV;
        b.has_click = true;
        b.click_action = (key_action_t){ ACTION_KEYBOARD_TAP, USB_MOD_NONE, USB_KEY_F8, 0, 0 };
        l0->bindings[l0->binding_count++] = b;
    }

    // ----------------------------------------------------
    // Preset Layers 1 ~ 4 (Inherit Layer 0 by default)
    // ----------------------------------------------------
    strncpy(engine->layers[1].name, "层1", sizeof(engine->layers[1].name) - 1);
    engine->layers[1].type = LAYER_TYPE_TIMEOUT;
    engine->layers[1].timeout_sec = 15;
    engine->layers[1].led_color = 0x06B6D4; // Cyan
    engine->layers[1].binding_count = 0;

    strncpy(engine->layers[2].name, "层2", sizeof(engine->layers[2].name) - 1);
    engine->layers[2].type = LAYER_TYPE_PERSISTENT;
    engine->layers[2].timeout_sec = 0;
    engine->layers[2].led_color = 0xA855F7; // Purple
    engine->layers[2].binding_count = 0;

    strncpy(engine->layers[3].name, "层3", sizeof(engine->layers[3].name) - 1);
    engine->layers[3].type = LAYER_TYPE_ONESHOT;
    engine->layers[3].timeout_sec = 0;
    engine->layers[3].led_color = 0xEAB308; // Yellow
    engine->layers[3].binding_count = 0;

    strncpy(engine->layers[4].name, "层4", sizeof(engine->layers[4].name) - 1);
    engine->layers[4].type = LAYER_TYPE_PERSISTENT;
    engine->layers[4].timeout_sec = 0;
    engine->layers[4].led_color = 0xFFFFFF; // White
    engine->layers[4].binding_count = 0;
}

bool key_engine_set_layer_binding(key_mapper_engine_t *engine, uint8_t layer_idx, const key_binding_t *binding) {
    if (!engine || !binding || layer_idx >= MAX_LAYERS) return false;
    key_engine_lock();

    key_layer_t *layer = &engine->layers[layer_idx];
    int idx = find_binding_index_in_layer(layer, binding->source_vk);
    if (idx >= 0) {
        layer->bindings[idx] = *binding;
        key_engine_unlock();
        return true;
    }
    if (layer->binding_count < MAX_KEY_BINDINGS) {
        layer->bindings[layer->binding_count++] = *binding;
        key_engine_unlock();
        return true;
    }
    key_engine_unlock();
    return false;
}

bool key_engine_get_layer_binding(const key_mapper_engine_t *engine, uint8_t layer_idx, uint8_t source_vk, key_binding_t *out_binding) {
    if (!engine || !out_binding || layer_idx >= MAX_LAYERS) return false;
    key_engine_lock();

    const key_layer_t *layer = &engine->layers[layer_idx];
    int idx = find_binding_index_in_layer(layer, source_vk);
    if (idx >= 0) {
        *out_binding = layer->bindings[idx];
        key_engine_unlock();
        return true;
    }
    key_engine_unlock();
    return false;
}

bool key_engine_set_binding(key_mapper_engine_t *engine, const key_binding_t *binding) {
    return key_engine_set_layer_binding(engine, 0, binding);
}

bool key_engine_get_binding(const key_mapper_engine_t *engine, uint8_t source_vk, key_binding_t *out_binding) {
    return key_engine_get_layer_binding(engine, 0, source_vk, out_binding);
}

void key_engine_init(key_mapper_engine_t *engine, key_output_callback_t cb) {
    if (!engine) return;
    key_engine_lock();
    memset(engine, 0, sizeof(key_mapper_engine_t));
    engine->output_cb = cb;
    key_engine_load_defaults(engine);
    key_engine_unlock();
}

void key_engine_feed_key(key_mapper_engine_t *engine, uint8_t raw_key_code, bool is_pressed, uint32_t now_ms) {
    if (!engine) return;
    key_engine_lock();

    engine->last_activity_time = now_ms;

    int slot = get_physical_key_slot(raw_key_code);
    if (slot < 0 || slot >= MAX_KEY_BINDINGS) {
        key_engine_unlock();
        return;
    }

    key_binding_t b;
    get_effective_binding(engine, raw_key_code, &b);
    key_slot_state_t *s = &engine->states[slot];

    // Immediately record telemetry on every state change
    engine->last_telemetry.source_vk = raw_key_code;
    engine->last_telemetry.is_pressed = is_pressed;
    engine->last_telemetry.timestamp = now_ms;
    engine->last_telemetry.action_type = b.click_action.type;
    engine->last_telemetry.modifier = b.click_action.modifier;
    engine->last_telemetry.key_code = b.click_action.key_code;
    engine->last_telemetry.consumer_code = b.click_action.consumer_code;
    engine->last_telemetry.active_layer = engine->active_layer;

    if (is_pressed) {
        if (!s->is_pressed) {
            s->is_pressed = true;
            s->press_timestamp = now_ms;
            s->long_fired = false;

            if (b.has_repeat) {
                s->next_repeat_timestamp = now_ms + b.repeat_delay_ms;
            }

            // Instant pass-through if no long and no double
            if (!b.has_long && !b.has_double) {
                if (b.click_action.type == ACTION_KEYBOARD_HOLD || 
                    b.click_action.type == ACTION_CONSUMER_HOLD || 
                    b.click_action.type == ACTION_VOICE_HOLD) {
                    emit_action(engine, &b.click_action, raw_key_code, true);
                } else if (b.click_action.type == ACTION_SWITCH_LAYER || b.click_action.type == ACTION_WOL) {
                    emit_action(engine, &b.click_action, raw_key_code, true);
                }
            }
        }
    } else {
        if (s->is_pressed) {
            s->is_pressed = false;
            s->release_timestamp = now_ms;
            uint32_t duration = now_ms - s->press_timestamp;
            engine->last_telemetry.duration_ms = duration;

            if (!b.has_long && !b.has_double) {
                // Pure pass-through release
                if (b.click_action.type == ACTION_KEYBOARD_HOLD) {
                    key_action_t rel = { ACTION_KEYBOARD_RELEASE, 0, 0, 0, 0 };
                    emit_action(engine, &rel, raw_key_code, false);
                } else if (b.click_action.type == ACTION_CONSUMER_HOLD) {
                    key_action_t rel = { ACTION_CONSUMER_RELEASE, 0, 0, 0, 0 };
                    emit_action(engine, &rel, raw_key_code, false);
                } else if (b.click_action.type == ACTION_VOICE_HOLD) {
                    key_action_t rel = { ACTION_VOICE_RELEASE, 0, 0, 0, 0 };
                    emit_action(engine, &rel, raw_key_code, false);
                } else if (b.has_click && b.click_action.type != ACTION_SWITCH_LAYER && b.click_action.type != ACTION_WOL) {
                    emit_action(engine, &b.click_action, raw_key_code, false);
                }
            } else {
                // Key has Long Press or Double Click configured
                if (s->long_fired) {
                    if (b.long_action.type == ACTION_KEYBOARD_HOLD) {
                        key_action_t rel = { ACTION_KEYBOARD_RELEASE, 0, 0, 0, 0 };
                        emit_action(engine, &rel, raw_key_code, false);
                    } else if (b.long_action.type == ACTION_CONSUMER_HOLD) {
                        key_action_t rel = { ACTION_CONSUMER_RELEASE, 0, 0, 0, 0 };
                        emit_action(engine, &rel, raw_key_code, false);
                    }
                } else if (b.has_click) {
                    if (!b.has_double) {
                        emit_action_as_tap_if_hold(engine, &b.click_action, raw_key_code);
                    } else {
                        s->press_count++;
                        if (s->press_count == 1) {
                            s->waiting_double = true;
                        } else if (s->press_count >= 2) {
                            s->waiting_double = false;
                            s->press_count = 0;
                            emit_action_as_tap_if_hold(engine, &b.double_action, raw_key_code);
                        }
                    }
                }
            }
        }
    }
    key_engine_unlock();
}

void key_engine_tick(key_mapper_engine_t *engine, uint32_t now_ms) {
    if (!engine) return;
    key_engine_lock();

    // Check timeout for LAYER_TYPE_TIMEOUT
    if (engine->active_layer != 0 && engine->layers[engine->active_layer].type == LAYER_TYPE_TIMEOUT) {
        uint32_t timeout_ms = (uint32_t)engine->layers[engine->active_layer].timeout_sec * 1000;
        if (timeout_ms > 0 && (now_ms - engine->last_activity_time >= timeout_ms)) {
            app_log("KEYMAP", "Layer %u timed out (%u s) -> Auto-reverting to Layer 0", 
                    engine->active_layer, engine->layers[engine->active_layer].timeout_sec);
            key_engine_switch_layer(engine, 0, now_ms);
        }
    }

    uint8_t raw_keys[] = {
        MI_KEY_POWER, MI_KEY_VOICE, MI_KEY_UP, MI_KEY_DOWN,
        MI_KEY_LEFT, MI_KEY_RIGHT, MI_KEY_OK, MI_KEY_BACK,
        MI_KEY_HOME, MI_KEY_MENU, MI_KEY_VOL_UP, MI_KEY_VOL_DOWN, MI_KEY_TV
    };

    for (int slot = 0; slot < 13; slot++) {
        key_slot_state_t *s = &engine->states[slot];
        if (!s->is_pressed && !s->waiting_double) continue;

        uint8_t raw_key = raw_keys[slot];
        key_binding_t b;
        get_effective_binding(engine, raw_key, &b);

        if (s->is_pressed) {
            uint32_t hold_time = now_ms - s->press_timestamp;
            if (b.has_long && !s->long_fired && hold_time >= b.long_ms) {
                s->long_fired = true;
                emit_action(engine, &b.long_action, b.source_vk, true);
            }

            if (b.has_repeat && now_ms >= s->next_repeat_timestamp) {
                emit_action(engine, &b.repeat_action, b.source_vk, true);
                s->next_repeat_timestamp = now_ms + b.repeat_interval_ms;
            }
        } else {
            if (s->waiting_double && (now_ms - s->release_timestamp >= b.double_ms)) {
                s->waiting_double = false;
                s->press_count = 0;
                emit_action_as_tap_if_hold(engine, &b.click_action, b.source_vk);
            }
        }
    }
    key_engine_unlock();
}

void key_engine_release_all(key_mapper_engine_t *engine, uint32_t now_ms) {
    if (!engine) return;
    key_engine_lock();

    uint8_t raw_keys[] = {
        MI_KEY_POWER, MI_KEY_VOICE, MI_KEY_UP, MI_KEY_DOWN,
        MI_KEY_LEFT, MI_KEY_RIGHT, MI_KEY_OK, MI_KEY_BACK,
        MI_KEY_HOME, MI_KEY_MENU, MI_KEY_VOL_UP, MI_KEY_VOL_DOWN, MI_KEY_TV
    };

    for (int slot = 0; slot < 13; slot++) {
        key_slot_state_t *s = &engine->states[slot];
        if (s->is_pressed) {
            key_engine_feed_key(engine, raw_keys[slot], false, now_ms);
        }
        s->waiting_double = false;
        s->press_count = 0;
    }
    key_engine_unlock();
}
