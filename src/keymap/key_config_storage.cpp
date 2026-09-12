#include "key_config_storage.h"
#include "log/app_log.h"
#include "wol_manager.h"
#include <Preferences.h>
#include <ArduinoJson.h>

void key_config_storage_init(key_mapper_engine_t *engine) {
    if (!key_config_storage_load(engine)) {
        key_engine_load_defaults(engine);
        app_log("KEYMAP", "Loaded safe factory defaults (%u bindings)", (unsigned int)engine->layers[0].binding_count);
    } else {
        app_log("KEYMAP", "Loaded custom multi-layer keymap from NVS (active layer: %u)", (unsigned int)engine->active_layer);
    }
}

String key_config_to_json(const key_mapper_engine_t *engine) {
    if (!engine) return "{\"active_layer\":0,\"layers\":[]}";

    JsonDocument doc;
    doc["active_layer"] = engine->active_layer;
    JsonArray layer_arr = doc["layers"].to<JsonArray>();

    for (size_t l = 0; l < MAX_LAYERS; l++) {
        const key_layer_t *layer = &engine->layers[l];
        JsonObject layer_obj = layer_arr.add<JsonObject>();
        layer_obj["id"] = (int)l;
        layer_obj["name"] = layer->name;
        layer_obj["type"] = (int)layer->type;
        layer_obj["timeout"] = layer->timeout_sec;
        
        char color_buf[16];
        snprintf(color_buf, sizeof(color_buf), "0x%06X", (unsigned int)(layer->led_color & 0xFFFFFF));
        layer_obj["color"] = color_buf;

        JsonArray arr = layer_obj["bindings"].to<JsonArray>();
        for (size_t i = 0; i < layer->binding_count; i++) {
            const key_binding_t *b = &layer->bindings[i];
            JsonObject obj = arr.add<JsonObject>();

            obj["source_vk"] = b->source_vk;
            
            // Click action
            if (b->has_click) {
                obj["has_click"] = true;
                obj["click_type"] = (int)b->click_action.type;
                if (b->click_action.modifier != 0) obj["click_mod"] = b->click_action.modifier;
                if (b->click_action.key_code != 0) obj["click_key"] = b->click_action.key_code;
                if (b->click_action.consumer_code != 0) obj["click_cons"] = b->click_action.consumer_code;
                if (b->click_action.type == ACTION_SWITCH_LAYER) obj["click_layer"] = b->click_action.target_layer;
                if (b->click_action.type == ACTION_WOL) {
                    char mac_buf[20];
                    wol_manager_mac_to_str(b->click_action.wol_mac, mac_buf, sizeof(mac_buf));
                    obj["click_mac"] = mac_buf;
                }
            }

            // Long action
            if (b->has_long) {
                obj["has_long"] = true;
                obj["long_ms"] = b->long_ms;
                obj["long_type"] = (int)b->long_action.type;
                if (b->long_action.modifier != 0) obj["long_mod"] = b->long_action.modifier;
                if (b->long_action.key_code != 0) obj["long_key"] = b->long_action.key_code;
                if (b->long_action.consumer_code != 0) obj["long_cons"] = b->long_action.consumer_code;
                if (b->long_action.type == ACTION_SWITCH_LAYER) obj["long_layer"] = b->long_action.target_layer;
                if (b->long_action.type == ACTION_WOL) {
                    char mac_buf[20];
                    wol_manager_mac_to_str(b->long_action.wol_mac, mac_buf, sizeof(mac_buf));
                    obj["long_mac"] = mac_buf;
                }
            }

            // Double action
            if (b->has_double) {
                obj["has_double"] = true;
                obj["double_ms"] = b->double_ms;
                obj["double_type"] = (int)b->double_action.type;
                if (b->double_action.modifier != 0) obj["double_mod"] = b->double_action.modifier;
                if (b->double_action.key_code != 0) obj["double_key"] = b->double_action.key_code;
                if (b->double_action.consumer_code != 0) obj["double_cons"] = b->double_action.consumer_code;
                if (b->double_action.type == ACTION_SWITCH_LAYER) obj["double_layer"] = b->double_action.target_layer;
                if (b->double_action.type == ACTION_WOL) {
                    char mac_buf[20];
                    wol_manager_mac_to_str(b->double_action.wol_mac, mac_buf, sizeof(mac_buf));
                    obj["double_mac"] = mac_buf;
                }
            }
        }
    }

    String out;
    serializeJson(doc, out);
    return out;
}

static uint32_t parse_u32_or_hex(JsonVariant v, uint32_t default_val = 0) {
    if (v.isNull()) return default_val;
    if (v.is<int>() || v.is<unsigned int>() || v.is<long>() || v.is<unsigned long>()) {
        return v.as<uint32_t>();
    }
    if (v.is<const char*>() || v.is<String>()) {
        String s = v.as<String>();
        s.trim();
        if (s.startsWith("0x") || s.startsWith("0X")) {
            return (uint32_t)strtoul(s.c_str(), NULL, 16);
        }
        return (uint32_t)strtoul(s.c_str(), NULL, 10);
    }
    return default_val;
}

static void parse_bindings_array(JsonArray arr, key_layer_t *layer) {
    if (!layer || arr.isNull()) return;
    layer->binding_count = 0;
    for (JsonObject obj : arr) {
        if (layer->binding_count >= MAX_KEY_BINDINGS) break;

        key_binding_t b;
        memset(&b, 0, sizeof(b));

        b.source_vk = parse_u32_or_hex(obj["source_vk"], 0);

        b.has_click = obj["has_click"] | false;
        b.click_action.type = (key_action_type_t)parse_u32_or_hex(obj["click_type"], 0);
        b.click_action.modifier = (uint8_t)parse_u32_or_hex(obj["click_mod"], 0);
        b.click_action.key_code = (uint8_t)parse_u32_or_hex(obj["click_key"], 0);
        b.click_action.consumer_code = (uint16_t)parse_u32_or_hex(obj["click_cons"], 0);
        b.click_action.target_layer = (uint8_t)parse_u32_or_hex(obj["click_layer"], 0);
        if (!obj["click_mac"].isNull()) {
            wol_manager_parse_mac(obj["click_mac"].as<String>().c_str(), b.click_action.wol_mac);
        }

        // Normalize MI_KEY_VOICE_ALT (0x3E) to MI_KEY_VOICE (0x04)
        if (b.source_vk == MI_KEY_VOICE_ALT) {
            b.source_vk = MI_KEY_VOICE;
        }

        // FORCE ACTION_VOICE_HOLD for Voice key on Layer 0 only
        if (b.source_vk == MI_KEY_VOICE && b.click_action.type != ACTION_SWITCH_LAYER && b.click_action.type != ACTION_TRANSPARENT) {
            b.click_action.type = ACTION_VOICE_HOLD;
        }

        b.has_long = obj["has_long"] | false;
        b.long_ms = parse_u32_or_hex(obj["long_ms"], 600);
        b.long_action.type = (key_action_type_t)parse_u32_or_hex(obj["long_type"], 1);
        b.long_action.modifier = (uint8_t)parse_u32_or_hex(obj["long_mod"], 0);
        b.long_action.key_code = (uint8_t)parse_u32_or_hex(obj["long_key"], 0);
        b.long_action.consumer_code = (uint16_t)parse_u32_or_hex(obj["long_cons"], 0);
        b.long_action.target_layer = (uint8_t)parse_u32_or_hex(obj["long_layer"], 0);
        if (!obj["long_mac"].isNull()) {
            wol_manager_parse_mac(obj["long_mac"].as<String>().c_str(), b.long_action.wol_mac);
        }

        b.has_double = obj["has_double"] | false;
        b.double_ms = parse_u32_or_hex(obj["double_ms"], 250);
        b.double_action.type = (key_action_type_t)parse_u32_or_hex(obj["double_type"], 1);
        b.double_action.modifier = (uint8_t)parse_u32_or_hex(obj["double_mod"], 0);
        b.double_action.key_code = (uint8_t)parse_u32_or_hex(obj["double_key"], 0);
        b.double_action.consumer_code = (uint16_t)parse_u32_or_hex(obj["double_cons"], 0);
        b.double_action.target_layer = (uint8_t)parse_u32_or_hex(obj["double_layer"], 0);
        if (!obj["double_mac"].isNull()) {
            wol_manager_parse_mac(obj["double_mac"].as<String>().c_str(), b.double_action.wol_mac);
        }

        layer->bindings[layer->binding_count++] = b;
    }
}

bool key_config_from_json(key_mapper_engine_t *engine, const String &json_str) {
    if (!engine || json_str.length() == 0) return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        app_log("KEYMAP", "JSON deserialize failed: %s", err.c_str());
        return false;
    }

    // Check if new multi-layer schema
    if (doc["layers"].is<JsonArray>()) {
        JsonArray layers_arr = doc["layers"].as<JsonArray>();
        for (JsonObject l_obj : layers_arr) {
            uint8_t id = (uint8_t)parse_u32_or_hex(l_obj["id"], 0);
            if (id >= MAX_LAYERS) continue;

            key_layer_t *layer = &engine->layers[id];
            if (!l_obj["name"].isNull()) {
                String nm = l_obj["name"].as<String>();
                strncpy(layer->name, nm.c_str(), sizeof(layer->name) - 1);
            }
            layer->type = (layer_type_t)parse_u32_or_hex(l_obj["type"], (uint32_t)layer->type);
            layer->timeout_sec = (uint16_t)parse_u32_or_hex(l_obj["timeout"], layer->timeout_sec);
            if (!l_obj["color"].isNull()) {
                layer->led_color = parse_u32_or_hex(l_obj["color"], layer->led_color);
            }

            JsonArray b_arr = l_obj["bindings"].as<JsonArray>();
            if (!b_arr.isNull()) {
                parse_bindings_array(b_arr, layer);
            }
        }
        app_log("KEYMAP", "Loaded multi-layer keymap from JSON (5 layers)");
        return true;
    }

    // Backward compatibility: old single layer JSON with root "bindings" array
    if (doc["bindings"].is<JsonArray>()) {
        parse_bindings_array(doc["bindings"].as<JsonArray>(), &engine->layers[0]);
        app_log("KEYMAP", "Migrated legacy single-layer keymap into Layer 0 (%u bindings)", 
                (unsigned int)engine->layers[0].binding_count);
        return true;
    }

    return false;
}

bool key_config_storage_save(key_mapper_engine_t *engine) {
    if (!engine) return false;
    String json = key_config_to_json(engine);

    Preferences prefs;
    if (!prefs.begin("keymap_conf", false)) {
        app_log("KEYMAP", "Failed to open keymap_conf NVS namespace for writing!");
        return false;
    }
    size_t written = prefs.putString("cfg_json", json);
    prefs.end();

    if (written == 0) {
        app_log("KEYMAP", "ERROR: putString to NVS failed (written 0 bytes)!");
        return false;
    }
    app_log("KEYMAP", "Saved keymap to NVS successfully (%u bytes written)", (unsigned int)written);
    return true;
}

bool key_config_storage_load(key_mapper_engine_t *engine) {
    if (!engine) return false;

    Preferences prefs;
    if (!prefs.begin("keymap_conf", true)) {
        key_engine_load_defaults(engine);
        return false;
    }
    String json = prefs.getString("cfg_json", "");
    prefs.end();

    if (json.length() == 0) {
        key_engine_load_defaults(engine);
        return false;
    }
    bool ok = key_config_from_json(engine, json);
    if (!ok || engine->layers[0].binding_count == 0) {
        app_log("KEYMAP", "NVS keymap is invalid or corrupted -> auto-fallback to safe defaults!");
        key_engine_load_defaults(engine);
        return false;
    }
    return true;
}

void key_config_storage_reset_defaults(key_mapper_engine_t *engine) {
    if (!engine) return;

    Preferences prefs;
    if (prefs.begin("keymap_conf", false)) {
        prefs.remove("cfg_json");
        prefs.end();
    }
    key_engine_load_defaults(engine);
    app_log("KEYMAP", "Reset keymap to factory defaults (%u bindings in Layer 0)", (unsigned int)engine->layers[0].binding_count);
}

String key_telemetry_to_json(const key_mapper_engine_t *engine) {
    JsonDocument doc;
    if (engine) {
        doc["source_vk"] = engine->last_telemetry.source_vk;
        doc["is_pressed"] = engine->last_telemetry.is_pressed;
        doc["duration_ms"] = engine->last_telemetry.duration_ms;
        doc["action_type"] = engine->last_telemetry.action_type;
        doc["modifier"] = engine->last_telemetry.modifier;
        doc["key_code"] = engine->last_telemetry.key_code;
        doc["consumer_code"] = engine->last_telemetry.consumer_code;
        doc["active_layer"] = engine->active_layer;
    }
    String out;
    serializeJson(doc, out);
    return out;
}

