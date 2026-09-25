#include "web_server.h"
#include "web_ui.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "version.h"
#include "ble/ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "keymap/key_config_storage.h"
#include "nvs/nvs_manager.h"
#include "wol_manager.h"
#include "ota/ota_manager.h"
#include "config/config_backup.h"
#include <WebServer.h>
#include <ArduinoJson.h>

#ifndef REMOTEMAPPER_OTA
#define REMOTEMAPPER_OTA 0
#endif

static WebServer s_server(80);
extern key_mapper_engine_t g_key_engine;

static void handle_root() {
    s_server.sendHeader("Connection", "close");
    s_server.send(200, "text/html; charset=utf-8", INDEX_HTML);
}

static void handle_status() {
    JsonDocument doc;
    doc["firmware"] = FIRMWARE_NAME;
    doc["version"] = FIRMWARE_VERSION;
    doc["uptime_sec"] = millis() / 1000;
    doc["ble_state"] = (int)ble_remote_get_state();
    doc["battery_pct"] = ble_remote_get_battery_pct();
    doc["frames_decoded"] = g_audio_pipeline.total_frames_decoded;
    doc["samples_pushed"] = g_audio_pipeline.total_samples_pushed;
    doc["free_heap"] = ESP.getFreeHeap();
    doc["free_psram"] = ESP.getFreePsram();
    doc["ap_ip"] = wifi_manager_get_ap_ip();
    doc["sta_ip"] = wifi_manager_get_sta_ip();
    doc["sta_connected"] = wifi_manager_is_sta_connected();
    doc["ap_ssid"] = AP_SSID;
    doc["wifi_enabled"] = wifi_manager_get_enabled();
    doc["wifi_policy"] = (int)wifi_manager_get_policy();
    doc["wifi_policy_str"] = wifi_manager_policy_str(wifi_manager_get_policy());
    doc["wifi_timeout_min"] = wifi_manager_get_timeout_min();
    doc["wifi_radio_state"] = (int)wifi_manager_get_radio_state();
    String ap_pass = wifi_manager_get_ap_pass();
    doc["ap_secured"] = (ap_pass.length() >= 8);
    doc["ap_pass"] = ap_pass;

    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void handle_logs() {
    String json = app_log_get_json();
    s_server.send(200, "application/json", json);
}

static void handle_logs_clear() {
    app_log_clear();
    s_server.send(200, "application/json", "{\"status\":\"cleared\"}");
}

static void handle_wifi_scan() {
    String json = wifi_manager_scan_json();
    s_server.send(200, "application/json", json);
}

static void handle_wifi_config() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    String ssid = doc["ssid"] | "";
    String pass = doc["pass"] | "";

    if (ssid.length() == 0) {
        s_server.send(400, "application/json", "{\"error\":\"empty_ssid\"}");
        return;
    }

    wifi_manager_save_sta_config(ssid, pass);
    s_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handle_wifi_ap_config() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    String ap_pass = doc["ap_pass"] | "";
    ap_pass.trim();

    if (ap_pass.length() > 0 && ap_pass.length() < 8) {
        s_server.send(400, "application/json", "{\"error\":\"password_too_short\",\"message\":\"AP 密码至少需要 8 位字符，或留空设置为开放热点\"}");
        return;
    }

    if (wifi_manager_save_ap_config(ap_pass)) {
        s_server.send(200, "application/json", "{\"status\":\"ok\",\"secured\":" + String(ap_pass.length() >= 8 ? "true" : "false") + "}");
    } else {
        s_server.send(500, "application/json", "{\"error\":\"save_failed\"}");
    }
}

static void handle_keymap_get() {
    String json = key_config_to_json(&g_key_engine);
    s_server.send(200, "application/json", json);
}

static void handle_keymap_save() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    bool ok = key_config_from_json(&g_key_engine, s_server.arg("plain"));
    if (ok) {
        key_config_storage_save(&g_key_engine);
        s_server.send(200, "application/json", "{\"status\":\"saved\"}");
    } else {
        s_server.send(400, "application/json", "{\"error\":\"invalid_keymap_format\"}");
    }
}

static void handle_keymap_reset() {
    key_config_storage_reset_defaults(&g_key_engine);
    app_log("KEYMAP", "Reset keymap to factory defaults via Web API");
    s_server.send(200, "application/json", "{\"status\":\"reset_ok\"}");
}

static void handle_keymap_telemetry() {
    String json = key_telemetry_to_json(&g_key_engine);
    s_server.send(200, "application/json", json);
}

static void handle_ble_scan() {
    String json = ble_remote_scan_devices_json();
    s_server.send(200, "application/json", json);
}

static void handle_ble_connect() {
    if (!s_server.hasArg("plain")) {
        app_log("WEB", "BLE connect rejected: missing POST body");
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        app_log("WEB", "BLE connect rejected: invalid JSON payload");
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    String mac = doc["mac"] | "";
    String name = doc["name"] | "";
    uint8_t type = doc["type"] | 0;

    if (mac.length() == 0) {
        app_log("WEB", "BLE connect rejected: empty MAC address");
        s_server.send(400, "application/json", "{\"error\":\"empty_mac\"}");
        return;
    }

    app_log("WEB", "Connecting to BLE target: %s (%s, Type: %d)", 
            name.length() > 0 ? name.c_str() : "Unknown", mac.c_str(), (int)type);

    bool ok = ble_remote_connect_target(mac, type, name);
    s_server.send(200, "application/json", ok ? "{\"status\":\"ok\"}" : "{\"status\":\"failed\"}");
}

static void handle_ble_unpair() {
    app_log("WEB", "BLE unpair requested via Web API");
    ble_remote_unpair();
    s_server.send(200, "application/json", "{\"status\":\"ok\"}");
}

static void handle_ble_info() {
    String json = ble_remote_get_connected_info();
    s_server.send(200, "application/json", json);
}

static void handle_ble_reconnect() {
    app_log("BLE", "Triggered manual reconnect scan via Web API");
    ble_remote_trigger_reconnect();
    s_server.send(200, "application/json", "{\"status\":\"reconnecting\"}");
}

static void handle_system_restart() {
    app_log("SYSTEM", "Rebooting ESP32 via Web API...");
    s_server.send(200, "application/json", "{\"status\":\"rebooting\"}");
    delay(500);
    ESP.restart();
}

static void handle_power_get() {
    JsonDocument doc;
    doc["policy"] = (int)wifi_manager_get_policy();
    doc["policy_str"] = wifi_manager_policy_str(wifi_manager_get_policy());
    doc["timeout_min"] = wifi_manager_get_timeout_min();
    doc["radio_state"] = (int)wifi_manager_get_radio_state();
    doc["radio_state_str"] = wifi_manager_state_str(wifi_manager_get_radio_state());
    doc["wifi_enabled"] = wifi_manager_get_enabled();
    doc["last_activity_sec"] = wifi_manager_get_last_activity_ms() / 1000;
    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void handle_power_set() {
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"error\":\"missing_body\"}");
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
    if (err) {
        s_server.send(400, "application/json", "{\"error\":\"invalid_json\"}");
        return;
    }

    bool policy_changed = false;
    if (!doc["policy"].isNull()) {
        int p = doc["policy"] | -1;
        if (p < (int)WIFI_POLICY_ALWAYS_ON || p > (int)WIFI_POLICY_DISABLED) {
            s_server.send(400, "application/json", "{\"error\":\"invalid_policy\",\"hint\":\"0=always_on,1=on_demand,2=disabled\"}");
            return;
        }
        wifi_policy_t old_p = wifi_manager_get_policy();
        if (wifi_manager_set_policy((wifi_policy_t)p)) {
            policy_changed = (wifi_manager_get_policy() != old_p);
        }
    }
    if (!doc["timeout_min"].isNull()) {
        uint32_t t = doc["timeout_min"] | 0xFFFFFFFF;
        if (!wifi_manager_set_timeout_min(t)) {
            s_server.send(400, "application/json", "{\"error\":\"invalid_timeout\",\"hint\":\"1,5,10,30,0(never)\"}");
            return;
        }
    }

    JsonDocument res;
    res["status"] = "ok";
    res["policy"] = (int)wifi_manager_get_policy();
    res["policy_str"] = wifi_manager_policy_str(wifi_manager_get_policy());
    res["timeout_min"] = wifi_manager_get_timeout_min();
    res["reboot_required"] = policy_changed;
    String out;
    serializeJson(res, out);
    s_server.send(200, "application/json", out);
}

static void handle_nvs_get() {
    String json = nvs_manager_dump_json();
    s_server.send(200, "application/json", json);
}

static void handle_nvs_save() {
    s_server.send(403, "application/json", "{\"error\":\"disabled\",\"message\":\"全量 NVS 直接回写功能已被禁用，按键配置请使用 /api/keymap/save 导入。\"}");
}


static void handle_nvs_reset() {
    bool ok = nvs_manager_erase_all();
    if (ok) {
        s_server.send(200, "application/json", "{\"status\":\"erased\",\"message\":\"NVS已清空，系统即将重启...\"}");
        delay(500);
        ESP.restart();
    } else {
        s_server.send(500, "application/json", "{\"error\":\"erase_failed\"}");
    }
}

static void handle_wol_test() {
    String mac = "";
    uint16_t port = 9;

    if (s_server.hasArg("plain")) {
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, s_server.arg("plain"));
        if (!err) {
            if (doc["mac"].is<const char*>()) mac = doc["mac"].as<String>();
            if (doc["port"].is<int>()) port = (uint16_t)doc["port"].as<int>();
        }
    }
    if (mac.length() == 0 && s_server.hasArg("mac")) {
        mac = s_server.arg("mac");
    }
    if (s_server.hasArg("port")) {
        port = (uint16_t)s_server.arg("port").toInt();
    }
    if (port == 0) port = 9;

    mac.trim();
    if (mac.length() == 0) {
        s_server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"缺少 MAC 地址\"}");
        return;
    }

    bool ok = wol_manager_send_str(mac.c_str(), port);
    if (ok) {
        s_server.send(200, "application/json", "{\"status\":\"ok\",\"message\":\"Wake-on-LAN 唤醒魔术包已广播发送\"}");
    } else {
        s_server.send(400, "application/json", "{\"status\":\"error\",\"message\":\"发送失败，请检查 MAC 地址格式是否正确 (如 AA:BB:CC:DD:EE:FF)\"}");
    }
}

static void handle_ota_status() {
    const ota_status_t* st = ota_manager_get_status();
    JsonDocument doc;
    doc["supported"] = st->enabled;
    doc["in_progress"] = st->in_progress;
    doc["last_success"] = st->last_success;
    doc["last_error"] = ota_manager_error_str();
    doc["version"] = FIRMWARE_VERSION;
    doc["running_label"] = st->running_label ? st->running_label : "";
    doc["running_addr"] = st->running_addr;
    doc["target_label"] = st->target_label ? st->target_label : "";
    doc["target_addr"] = st->target_addr;
    doc["target_capacity"] = st->target_capacity;
    doc["received"] = st->received;
    doc["total"] = st->total;
    doc["rollback_armed"] = st->rollback_armed;
    doc["boot_fail_count"] = st->boot_fail_count;
    String out;
    serializeJson(doc, out);
    s_server.send(200, "application/json", out);
}

static void handle_ota_upload() {
    HTTPUpload& upload = s_server.upload();
    switch (upload.status) {
        case UPLOAD_FILE_START:
            app_log("OTA", "Upload start: %s (%u bytes)", upload.filename.c_str(), upload.totalSize);
            if (!ota_manager_begin(upload.totalSize)) {
                app_log("OTA", "Upload rejected: %s", ota_manager_error_str());
            }
            break;
        case UPLOAD_FILE_WRITE:
            if (ota_manager_write(upload.buf, upload.currentSize) != upload.currentSize) {
                app_log("OTA", "Write error at +%u bytes: %s",
                        (unsigned int)ota_manager_get_status()->received,
                        ota_manager_error_str());
            }
            break;
        case UPLOAD_FILE_END:
            app_log("OTA", "Upload complete, verifying image...");
            ota_manager_end();
            break;
        case UPLOAD_FILE_ABORTED:
            app_log("OTA", "Upload aborted by client");
            ota_manager_abort();
            break;
        default:
            break;
    }
}

static void handle_ota_done() {
    const ota_status_t* st = ota_manager_get_status();
    if (st->last_success) {
        s_server.sendHeader("Connection", "close");
        s_server.send(200, "application/json", "{\"success\":true,\"message\":\"固件升级成功，设备重启中，请稍候约 20~40 秒...\"}");
        delay(300);
        ESP.restart();
    } else {
        s_server.sendHeader("Connection", "close");
        s_server.send(500, "application/json",
                      String("{\"success\":false,\"message\":\"固件升级失败: ") + ota_manager_error_str() + "\"}");
    }
}

static void handle_config_export() {
    bool full = s_server.hasArg("mode") && s_server.arg("mode") == "full";
    s_server.sendHeader("Content-Type", "application/json");
    s_server.sendHeader("Content-Disposition",
                        String("attachment; filename=RemoteMapper_Config_") +
                        (full ? "full_" : "safe_") + FIRMWARE_VERSION + ".json");
    s_server.send(200, "application/json", config_backup_export(full));
}

static void handle_config_import() {
    String body = s_server.arg("plain");
    if (body.length() == 0) {
        if (s_server.hasArg("body")) body = s_server.arg("body");
    }
    if (body.length() == 0) {
        s_server.send(400, "application/json",
                      "{\"status\":\"error\",\"message\":\"缺少备份内容\"}");
        return;
    }
    String err = config_backup_import(body);
    if (err.length() == 0) {
        s_server.send(200, "application/json",
                      "{\"status\":\"ok\",\"message\":\"配置已恢复，正在重启设备...\",\"reboot_required\":true}");
    } else {
        s_server.send(400, "application/json",
                      String("{\"status\":\"error\",\"message\":\"恢复失败: ") +
                      err + "\"}");
    }
}

static void handle_captive_portal() {
    String host = s_server.hostHeader();
    if (host != "192.168.4.1" && host != "remotemapper.local") {
        s_server.sendHeader("Location", "http://192.168.4.1/", true);
        s_server.send(302, "text/plain", "");
        return;
    }
    handle_root();
}

void web_server_init(void) {
    // API Routes
    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/api/status", HTTP_GET, handle_status);
    s_server.on("/api/logs", HTTP_GET, handle_logs);
    s_server.on("/api/logs/clear", HTTP_POST, handle_logs_clear);
    s_server.on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
    s_server.on("/api/wifi/config", HTTP_POST, handle_wifi_config);
    s_server.on("/api/wifi/ap", HTTP_POST, handle_wifi_ap_config);
    s_server.on("/api/keymap", HTTP_GET, handle_keymap_get);
    s_server.on("/api/keymap/save", HTTP_POST, handle_keymap_save);
    s_server.on("/api/keymap/reset", HTTP_POST, handle_keymap_reset);
    s_server.on("/api/keymap/telemetry", HTTP_GET, handle_keymap_telemetry);
    s_server.on("/api/wol/test", HTTP_POST, handle_wol_test);
    s_server.on("/api/ble/scan", HTTP_GET, handle_ble_scan);
    s_server.on("/api/ble/connect", HTTP_POST, handle_ble_connect);
    s_server.on("/api/ble/unpair", HTTP_POST, handle_ble_unpair);
    s_server.on("/api/ble/info", HTTP_GET, handle_ble_info);
    s_server.on("/api/ble/reconnect", HTTP_POST, handle_ble_reconnect);
    s_server.on("/api/nvs", HTTP_GET, handle_nvs_get);
    s_server.on("/api/nvs/save", HTTP_POST, handle_nvs_save);
    s_server.on("/api/nvs/reset", HTTP_POST, handle_nvs_reset);
    s_server.on("/api/power", HTTP_GET, handle_power_get);
    s_server.on("/api/power", HTTP_POST, handle_power_set);
    s_server.on("/api/system/restart", HTTP_POST, handle_system_restart);
    s_server.on("/api/config/export", HTTP_GET, handle_config_export);
    s_server.on("/api/config/import", HTTP_POST, handle_config_import);

#if REMOTEMAPPER_OTA
    s_server.on("/api/ota/status", HTTP_GET, handle_ota_status);
    s_server.on("/api/ota/upload", HTTP_POST, handle_ota_done, handle_ota_upload);
#else
    s_server.on("/api/ota/status", HTTP_GET, []() {
        s_server.send(200, "application/json", "{\"supported\":false,\"message\":\"当前型号固件未启用在线升级\"}");
    });
    s_server.on("/api/ota/upload", HTTP_POST, []() {
        s_server.send(501, "application/json", "{\"supported\":false,\"message\":\"当前型号固件未启用在线升级\"}");
    });
#endif

    // Captive Portal probe redirects
    s_server.on("/generate_204", HTTP_GET, handle_captive_portal);
    s_server.on("/hotspot-detect.html", HTTP_GET, handle_captive_portal);
    s_server.on("/canonical.html", HTTP_GET, handle_captive_portal);
    s_server.on("/connecttest.txt", HTTP_GET, handle_captive_portal);
    s_server.onNotFound(handle_captive_portal);

    s_server.begin();
    app_log("WEB", "HTTP Web Server started on port 80");
}

void web_server_task(void) {
    s_server.handleClient();
}
