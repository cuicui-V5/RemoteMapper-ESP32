#include "nvs_manager.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include "keymap/key_config_storage.h"
#include "keymap/key_state_machine.h"
#include "log/app_log.h"

extern key_mapper_engine_t g_key_engine;

static bool is_valid_printable_str(const char* buf, size_t len) {
    if (!buf || len == 0) return false;
    for (size_t i = 0; i < len && buf[i] != '\0'; i++) {
        uint8_t c = (uint8_t)buf[i];
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') return false;
    }
    return true;
}

static String sanitize_str(const char* buf, size_t len) {
    if (!buf || len == 0) return "";
    String s = "";
    for (size_t i = 0; i < len && buf[i] != '\0'; i++) {
        uint8_t c = (uint8_t)buf[i];
        if (c >= 32 || c == '\t' || c == '\n' || c == '\r') {
            s += (char)c;
        }
    }
    return s;
}

static String blob_to_hex_str(const uint8_t* buf, size_t len) {
    if (!buf || len == 0) return "0x00";
    String hex = "0x";
    const char* digits = "0123456789ABCDEF";
    for (size_t i = 0; i < len; i++) {
        hex += digits[(buf[i] >> 4) & 0x0F];
        hex += digits[buf[i] & 0x0F];
    }
    return hex;
}

String nvs_manager_dump_json(void) {
    JsonDocument doc;
    nvs_iterator_t it = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY);
    while (it != NULL) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        nvs_handle_t handle;
        esp_err_t err = nvs_open(info.namespace_name, NVS_READONLY, &handle);
        if (err == ESP_OK) {
            JsonObject nsObj = doc[info.namespace_name].is<JsonObject>() ? 
                               doc[info.namespace_name].as<JsonObject>() : 
                               doc[info.namespace_name].to<JsonObject>();
            switch (info.type) {
                case NVS_TYPE_U8: { uint8_t val = 0; if (nvs_get_u8(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_I8: { int8_t val = 0; if (nvs_get_i8(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_U16: { uint16_t val = 0; if (nvs_get_u16(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_I16: { int16_t val = 0; if (nvs_get_i16(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_U32: { uint32_t val = 0; if (nvs_get_u32(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_I32: { int32_t val = 0; if (nvs_get_i32(handle, info.key, &val) == ESP_OK) nsObj[info.key] = val; break; }
                case NVS_TYPE_STR: {
                    size_t req_len = 0;
                    if (nvs_get_str(handle, info.key, NULL, &req_len) == ESP_OK && req_len > 0) {
                        char* buf = (char*)malloc(req_len + 1);
                        if (buf) {
                            if (nvs_get_str(handle, info.key, buf, &req_len) == ESP_OK) {
                                buf[req_len] = '\0';
                                JsonDocument subDoc;
                                if (deserializeJson(subDoc, buf) == DeserializationError::Ok) {
                                    nsObj[info.key] = subDoc;
                                } else {
                                    nsObj[info.key] = sanitize_str(buf, req_len);
                                }
                            }
                            free(buf);
                        }
                    }
                    break;
                }
                case NVS_TYPE_BLOB: {
                    size_t req_len = 0;
                    if (nvs_get_blob(handle, info.key, NULL, &req_len) == ESP_OK && req_len > 0) {
                        uint8_t* buf = (uint8_t*)malloc(req_len + 1);
                        if (buf) {
                            if (nvs_get_blob(handle, info.key, buf, &req_len) == ESP_OK) {
                                buf[req_len] = '\0';
                                JsonDocument subDoc;
                                if (deserializeJson(subDoc, (char*)buf) == DeserializationError::Ok) {
                                    nsObj[info.key] = subDoc;
                                } else if (is_valid_printable_str((char*)buf, req_len)) {
                                    nsObj[info.key] = sanitize_str((char*)buf, req_len);
                                } else {
                                    nsObj[info.key] = blob_to_hex_str(buf, req_len);
                                }
                            }
                            free(buf);
                        }
                    }
                    break;
                }
                default: break;
            }
            nvs_close(handle);
        }
        it = nvs_entry_next(it);
    }
    Preferences prefs;
    if (prefs.begin("wifi_conf", true)) {
        if (doc["wifi_conf"].isNull()) doc["wifi_conf"].to<JsonObject>();
        JsonObject wifiObj = doc["wifi_conf"].as<JsonObject>();
        if (prefs.isKey("ssid"))     wifiObj["ssid"]     = prefs.getString("ssid", "");
        if (prefs.isKey("pass"))     wifiObj["pass"]     = prefs.getString("pass", "");
        if (prefs.isKey("sta_ssid")) wifiObj["sta_ssid"] = prefs.getString("sta_ssid", "");
        if (prefs.isKey("sta_pass")) wifiObj["sta_pass"] = prefs.getString("sta_pass", "");
        if (prefs.isKey("ap_ssid"))  wifiObj["ap_ssid"]  = prefs.getString("ap_ssid", "");
        if (prefs.isKey("ap_pass"))  wifiObj["ap_pass"]  = prefs.getString("ap_pass", "");
        if (prefs.isKey("wifi_enabled")) wifiObj["wifi_enabled"] = prefs.getBool("wifi_enabled", true);
        prefs.end();
    }
    if (prefs.begin("ble_conf", true)) {
        if (doc["ble_conf"].isNull()) doc["ble_conf"].to<JsonObject>();
        JsonObject bleObj = doc["ble_conf"].as<JsonObject>();
        if (prefs.isKey("bound_mac"))  bleObj["bound_mac"]  = prefs.getString("bound_mac", "");
        if (prefs.isKey("bound_name")) bleObj["bound_name"] = prefs.getString("bound_name", "");
        if (prefs.isKey("bound_type")) bleObj["bound_type"] = prefs.getUChar("bound_type", 0);
        prefs.end();
    }
    if (prefs.begin("keymap_conf", true)) {
        if (doc["keymap_conf"].isNull()) doc["keymap_conf"].to<JsonObject>();
        JsonObject keyObj = doc["keymap_conf"].as<JsonObject>();
        if (prefs.isKey("cfg_json")) {
            String cj = prefs.getString("cfg_json", "");
            JsonDocument subDoc;
            if (deserializeJson(subDoc, cj) == DeserializationError::Ok) { keyObj["cfg_json"] = subDoc; }
            else { keyObj["cfg_json"] = cj; }
        }
        prefs.end();
    }
    String out;
    serializeJsonPretty(doc, out);
    return out;
}

bool nvs_manager_apply_json(const String& json_str, String& err_msg) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json_str);
    if (err) {
        err_msg = String("JSON 格式语法错误: ") + err.c_str();
        return false;
    }
    if (!doc.is<JsonObject>()) {
        err_msg = "根节点必须是 JSON 对象 (Root element must be a JSON Object)";
        return false;
    }
    JsonObject root = doc.as<JsonObject>();

    // 1. Dry-run safety validation for keymap_conf
    if (!root["keymap_conf"].isNull()) {
        JsonVariant km = root["keymap_conf"];
        if (km.is<JsonObject>() && !km.as<JsonObject>()["cfg_json"].isNull()) {
            JsonVariant cfg = km["cfg_json"];
            String testJson = "";
            if (cfg.is<JsonObject>() || cfg.is<JsonArray>()) {
                serializeJson(cfg, testJson);
            } else if (cfg.is<String>()) {
                testJson = cfg.as<String>();
            }
            if (testJson.length() > 0) {
                key_mapper_engine_t test_engine;
                memset(&test_engine, 0, sizeof(test_engine));
                if (!key_config_from_json(&test_engine, testJson) || test_engine.layers[0].binding_count == 0) {
                    err_msg = "【安全保护拦截】keymap_conf.cfg_json 按键映射格式错误或为空，已拒绝写入 Flash 以避免设备故障！";
                    return false;
                }
            }
        }
    }

    // 2. Protected system-internal partitions (prevent user edits from corrupting RF calibration or BLE keys)
    const char* const PROTECTED_NAMESPACES[] = {
        "nvs.net80211", "phy", "dhcp_state", "nimble_bond"
    };

    for (JsonPair nsPair : root) {
        String nsName = nsPair.key().c_str();
        if (!nsPair.value().is<JsonObject>()) continue;

        bool isProtected = false;
        for (const char* prot : PROTECTED_NAMESPACES) {
            if (nsName.equalsIgnoreCase(prot)) {
                isProtected = true;
                break;
            }
        }
        if (isProtected) {
            app_log("NVS", "Preserved low-level system partition: %s", nsName.c_str());
            continue;
        }

        Preferences prefs;
        prefs.begin(nsName.c_str(), false);
        JsonObject nsObj = nsPair.value().as<JsonObject>();
        for (JsonPair kv : nsObj) {
            String key = kv.key().c_str();
            JsonVariant val = kv.value();
            if (val.is<bool>()) {
                prefs.putBool(key.c_str(), val.as<bool>());
            } else if (val.is<int>() || val.is<long>() || val.is<uint32_t>()) {
                prefs.putLong(key.c_str(), val.as<long>());
            } else if (val.is<double>() || val.is<float>()) {
                prefs.putDouble(key.c_str(), val.as<double>());
            } else if (val.is<JsonObject>() || val.is<JsonArray>()) {
                String subStr;
                serializeJson(val, subStr);
                prefs.putString(key.c_str(), subStr);
            } else if (val.is<const char*>() || val.is<String>()) {
                String sVal = val.as<String>();
                prefs.putString(key.c_str(), sVal);
            }
        }
        prefs.end();
    }

    // 3. Hot-reload Key Engine with safe auto-fallback
    if (!key_config_storage_load(&g_key_engine)) {
        app_log("NVS", "Keymap reload fallback to safe defaults");
        key_engine_load_defaults(&g_key_engine);
    }

    app_log("NVS", "NVS configuration validated & updated safely via Web Manager");
    return true;
}

bool nvs_manager_erase_all(void) {
    app_log("NVS", "Erasing all NVS Flash partitions...");
    esp_err_t err = nvs_flash_erase();
    if (err == ESP_OK) {
        nvs_flash_init();
        return true;
    }
    return false;
}
