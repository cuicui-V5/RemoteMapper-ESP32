#include "wifi_manager.h"
#include "log/app_log.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

#define DNS_PORT        53
#define MDNS_HOSTNAME   "remotemapper"

static DNSServer        s_dns_server;
static Preferences      s_prefs;
static IPAddress        s_ap_ip(192, 168, 4, 1);
static IPAddress        s_ap_netmask(255, 255, 255, 0);

static bool             s_sta_configured = false;
static uint32_t         s_last_sta_check = 0;
static bool             s_wifi_enabled = true;
static bool             s_ap_running = false;
static bool             s_mdns_started = false;

static void wifi_start_ap(const String& ap_pass) {
    if (s_ap_running) {
        s_dns_server.stop();
        WiFi.softAPdisconnect(false);
    }
    WiFi.softAPConfig(s_ap_ip, s_ap_ip, s_ap_netmask);
    if (ap_pass.length() >= 8) {
        WiFi.softAP(AP_SSID, ap_pass.c_str());
        app_log("WIFI", "AP Started: %s (WPA2-PSK, IP: 192.168.4.1)", AP_SSID);
    } else {
        WiFi.softAP(AP_SSID, "");
        app_log("WIFI", "AP Started: %s (Open Network, IP: 192.168.4.1)", AP_SSID);
    }
    s_dns_server.setErrorReplyCode(DNSReplyCode::NoError);
    s_dns_server.start(DNS_PORT, "*", s_ap_ip);
    s_ap_running = true;
}

static void wifi_apply_config(void) {
    String sta_ssid = s_prefs.getString("ssid", "");
    String sta_pass = s_prefs.getString("pass", "");
    String ap_pass  = s_prefs.getString("ap_pass", "");

    // Set Wi-Fi Mode (AP + STA)
    WiFi.mode(WIFI_AP_STA);

    // 1. Configure and start AP Mode
    wifi_start_ap(ap_pass);

    // 2. Connect to Home Wi-Fi if saved
    if (sta_ssid.length() > 0) {
        s_sta_configured = true;
        app_log("WIFI", "Connecting to Home Wi-Fi: %s...", sta_ssid.c_str());
        WiFi.begin(sta_ssid.c_str(), sta_pass.c_str());
    } else {
        app_log("WIFI", "No Home Wi-Fi configured, running in AP Setup mode");
    }

    // 3. Start mDNS (once per boot)
    if (!s_mdns_started && MDNS.begin(MDNS_HOSTNAME)) {
        MDNS.addService("http", "tcp", 80);
        app_log("WIFI", "mDNS responder started: http://%s.local", MDNS_HOSTNAME);
        s_mdns_started = true;
    }
}

void wifi_manager_init(void) {
    s_prefs.begin("wifi_conf", false);
    s_wifi_enabled = s_prefs.getBool("wifi_enabled", true);

    if (!s_wifi_enabled) {
        // Never touch the WiFi stack when disabled: de-initializing WiFi before BLE
        // starts (or tearing it down at runtime) breaks 2.4GHz coexistence and can
        // stop the BLE link from coming up. The radio simply stays uninitialized.
        app_log("WIFI", "Wi-Fi disabled by config; radio left uninitialized (USB CDC/UART: 'wifi on')");
        return;
    }

    // 1. Start AP + STA + mDNS
    wifi_apply_config();
}

bool wifi_manager_get_enabled(void) {
    return s_wifi_enabled;
}

bool wifi_manager_is_ap_running(void) {
    return s_ap_running;
}

bool wifi_manager_set_enabled(bool enabled) {
    if (s_wifi_enabled == enabled) {
        return true;
    }

    s_prefs.putBool("wifi_enabled", enabled);
    s_wifi_enabled = enabled;

    // Applying the change requires a reboot: the caller (CLI/web) must restart.
    // Switching the WiFi stack on/off at runtime is unsafe while BLE is running.
    app_log("WIFI", "Wi-Fi %s saved; reboot required to apply", enabled ? "enable" : "disable");
    return true;
}

void wifi_manager_task(void) {
    if (!s_wifi_enabled) {
        return;
    }
    if (s_ap_running) {
        s_dns_server.processNextRequest();
    }

    uint32_t now = millis();
    if (s_sta_configured && (now - s_last_sta_check > 5000)) {
        s_last_sta_check = now;
        if (WiFi.status() == WL_CONNECTED) {
            static bool s_logged_connected = false;
            if (!s_logged_connected) {
                app_log("WIFI", "Connected to Home Wi-Fi! Local IP: %s", WiFi.localIP().toString().c_str());
                s_logged_connected = true;
            }
        }
    }
}

String wifi_manager_get_ap_ip(void) {
    if (!s_wifi_enabled) return "Disabled";
    return WiFi.softAPIP().toString();
}

String wifi_manager_get_sta_ip(void) {
    if (!s_wifi_enabled) return "Disabled";
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.localIP().toString();
    }
    return "Disconnected";
}

bool wifi_manager_is_sta_connected(void) {
    if (!s_wifi_enabled) return false;
    return WiFi.status() == WL_CONNECTED;
}

int8_t wifi_manager_get_sta_rssi(void) {
    if (!s_wifi_enabled) return 0;
    if (WiFi.status() == WL_CONNECTED) {
        return WiFi.RSSI();
    }
    return 0;
}

String wifi_manager_scan_json(void) {
    if (!s_wifi_enabled) {
        return "{\"networks\":[],\"error\":\"wifi_disabled\"}";
    }
    app_log("WIFI", "Scanning for 2.4GHz Wi-Fi networks...");
    int n = WiFi.scanNetworks();
    JsonDocument doc;
    JsonArray arr = doc["networks"].to<JsonArray>();

    for (int i = 0; i < n; i++) {
        JsonObject obj = arr.add<JsonObject>();
        obj["ssid"] = WiFi.SSID(i);
        obj["rssi"] = WiFi.RSSI(i);
        obj["secure"] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    }

    String out;
    serializeJson(doc, out);
    WiFi.scanDelete();
    return out;
}

bool wifi_manager_save_sta_config(const String& ssid, const String& password) {
    if (!s_wifi_enabled) return false;
    if (ssid.length() == 0) return false;

    s_prefs.putString("ssid", ssid);
    s_prefs.putString("pass", password);
    s_sta_configured = true;

    app_log("WIFI", "Saved new Wi-Fi credentials for: %s, connecting...", ssid.c_str());
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    return true;
}

String wifi_manager_get_ap_pass(void) {
    return s_prefs.getString("ap_pass", "");
}

bool wifi_manager_save_ap_config(const String& ap_password) {
    if (!s_wifi_enabled) return false;
    String p = ap_password;
    p.trim();
    if (p.length() > 0 && p.length() < 8) {
        return false;
    }
    s_prefs.putString("ap_pass", p);

    WiFi.softAPConfig(s_ap_ip, s_ap_ip, s_ap_netmask);
    if (p.length() >= 8) {
        WiFi.softAP(AP_SSID, p.c_str());
        app_log("WIFI", "AP reconfigured: %s (WPA2-PSK)", AP_SSID);
    } else {
        WiFi.softAP(AP_SSID, "");
        app_log("WIFI", "AP reconfigured: %s (Open Network)", AP_SSID);
    }
    return true;
}
