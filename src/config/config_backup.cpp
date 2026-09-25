#include "config_backup.h"
#include "config_manager.h"
#include "version.h"
#include "app_config.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "keymap/key_state_machine.h"
#include "keymap/key_config_storage.h"
#include <ArduinoJson.h>

#define BACKUP_TYPE      "remotemapper-config-backup"
#define BACKUP_SCHEMA    1
#define REDACTED         "********"

extern key_mapper_engine_t g_key_engine;

String config_backup_export(bool full) {
    JsonDocument doc;
    doc["type"] = BACKUP_TYPE;
    doc["schema"] = BACKUP_SCHEMA;
    doc["firmware"] = FIRMWARE_VERSION;
    doc["hardware"] = HARDWARE_TARGET;
    doc["config_version"] = config_manager_get_version();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["policy"] = (int)wifi_manager_get_policy();
    wifi["timeout_min"] = wifi_manager_get_timeout_min();

    String ssid    = wifi_manager_get_sta_ssid();
    String sta_pass = wifi_manager_get_sta_pass();
    wifi["sta_ssid"] = ssid;
    if (full) {
        wifi["sta_pass"] = sta_pass;
    } else {
        wifi["sta_has_pass"] = (sta_pass.length() > 0);
        wifi["sta_pass"]     = REDACTED;
    }

    String ap_pass = wifi_manager_get_ap_pass();
    if (full) {
        wifi["ap_pass"] = ap_pass;
    } else {
        wifi["ap_secured"] = (ap_pass.length() >= 8);
        wifi["ap_pass"]    = REDACTED;
    }

    // Keymap (5-layer binding rules)
    String km = key_config_to_json(&g_key_engine);
    JsonDocument kmDoc;
    if (deserializeJson(kmDoc, km) != DeserializationError::Ok) {
        doc["keymap"] = JsonObject();
    } else {
        doc["keymap"] = kmDoc.as<JsonObject>();
    }

    String out;
    serializeJsonPretty(doc, out);
    app_log("CONFIG", "Config backup exported (mode=%s, wifi_policy=%s)",
            full ? "full" : "safe",
            wifi_manager_policy_str(wifi_manager_get_policy()));
    return out;
}

String config_backup_import(const String& json) {
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        return "不是有效的 JSON 文件";
    }
    if (!doc["wifi"].is<JsonObject>() && !doc["keymap"].is<JsonObject>()) {
        return "备份文件缺少 wifi 或 keymap 内容";
    }

    if (doc["wifi"].is<JsonObject>()) {
        JsonObject w = doc["wifi"].as<JsonObject>();
        String ssid    = w["sta_ssid"] | "";
        String sta_pass = w["sta_pass"] | "";
        String ap_pass = w["ap_pass"] | "";
        int policy    = w["policy"] | (int)WIFI_DEFAULT_POLICY;
        uint32_t timeout = w["timeout_min"] | WIFI_DEFAULT_TIMEOUT_MIN;

        if (sta_pass == REDACTED || ap_pass == REDACTED) {
            return "该备份来自“安全模式”导出，密码已打码无法还原，请使用完整备份或重新输入密码";
        }

        if (!wifi_manager_restore_backup(ssid, sta_pass, ap_pass,
                                         (wifi_policy_t)policy, timeout)) {
            return "Wi-Fi 配置恢复失败（参数非法）";
        }
    }

    if (doc["keymap"].is<JsonObject>()) {
        String km;
        serializeJson(doc["keymap"].as<JsonObject>(), km);
        if (!key_config_from_json(&g_key_engine, km)) {
            return "按键映射恢复失败（格式不兼容）";
        }
        key_config_storage_save(&g_key_engine);
    }

    app_log("CONFIG", "Config backup restored; reboot required for policy change to apply");
    return "";
}