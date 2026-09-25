#include "wifi_manager.h"
#include "app_config.h"
#include "log/app_log.h"
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <ArduinoJson.h>

static_assert((int)WIFI_POLICY_ON_DEMAND == WIFI_DEFAULT_POLICY,
              "WIFI_DEFAULT_POLICY must match WIFI_POLICY_ON_DEMAND");

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

// Wi-Fi Power Management state
static wifi_policy_t      s_policy           = (wifi_policy_t)WIFI_DEFAULT_POLICY;
static uint32_t           s_timeout_min      = WIFI_DEFAULT_TIMEOUT_MIN;
static wifi_radio_state_t s_radio_state      = WIFI_STATE_OFF;
static uint32_t           s_last_activity_ms = 0;

static bool wifi_is_valid_timeout(uint32_t minutes) {
    return minutes == WIFI_TIMEOUT_NEVER || minutes == 1 || minutes == 5 ||
           minutes == 10 || minutes == 30;
}

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

    // Policy and timeout are normally seeded by config_manager_init() migration.
    // Defensive defaults keep the manager robust if that ever did not run.
    uint32_t stored_policy = s_prefs.getUInt("policy", WIFI_DEFAULT_POLICY);
    if (stored_policy > WIFI_POLICY_DISABLED) {
        stored_policy = WIFI_DEFAULT_POLICY;
    }
    s_policy = (wifi_policy_t)stored_policy;

    uint32_t stored_timeout = s_prefs.getUInt("timeout_min", WIFI_DEFAULT_TIMEOUT_MIN);
    if (!wifi_is_valid_timeout(stored_timeout)) {
        stored_timeout = WIFI_DEFAULT_TIMEOUT_MIN;
    }
    s_timeout_min = stored_timeout;

    s_wifi_enabled = (s_policy != WIFI_POLICY_DISABLED);

    if (!s_wifi_enabled) {
        // Never touch the WiFi stack when DISABLED: de-initializing WiFi before BLE
        // starts (or tearing it down at runtime) breaks 2.4GHz coexistence and can
        // stop the BLE link from coming up. The radio simply stays uninitialized.
        s_radio_state = WIFI_STATE_OFF;
        app_log("WIFI", "Wi-Fi policy DISABLED; radio left uninitialized (USB CDC/UART: 'wifi on')");
        return;
    }

    // Phase 2: ON_DEMAND still boots the radio on (same behavior as before).
    // The idle-timeout power-down of ON_DEMAND lands in a later phase.
    wifi_apply_config();
    s_radio_state = WIFI_STATE_ON;
    s_last_activity_ms = millis();
    app_log("WIFI", "Wi-Fi policy %s (timeout %u min, state %s)",
            wifi_manager_policy_str(s_policy), (unsigned int)s_timeout_min,
            wifi_manager_state_str(s_radio_state));
}

bool wifi_manager_get_enabled(void) {
    return s_policy != WIFI_POLICY_DISABLED;
}

bool wifi_manager_is_ap_running(void) {
    return s_ap_running;
}

bool wifi_manager_set_enabled(bool enabled) {
    // Legacy whole-radio switch; "enabled" now maps onto the Wi-Fi policy.
    // DISABLED keeps the radio off, any other policy keeps it available.
    return wifi_manager_set_policy(enabled ? WIFI_POLICY_ON_DEMAND : WIFI_POLICY_DISABLED);
}

wifi_policy_t wifi_manager_get_policy(void) {
    return s_policy;
}

bool wifi_manager_set_policy(wifi_policy_t policy) {
    if ((uint32_t)policy > WIFI_POLICY_DISABLED) {
        return false;
    }
    if (s_policy == policy) {
        return true;
    }
    s_prefs.putUInt("policy", (uint32_t)policy);
    s_policy = policy;
    s_wifi_enabled = (policy != WIFI_POLICY_DISABLED);
    // Applying the change requires a reboot: the caller (CLI/web) must restart.
    // Switching the WiFi stack on/off at runtime is unsafe while BLE is running.
    app_log("WIFI", "Wi-Fi policy set to %s; reboot required to apply",
            wifi_manager_policy_str(policy));
    return true;
}

uint32_t wifi_manager_get_timeout_min(void) {
    return s_timeout_min;
}

bool wifi_manager_set_timeout_min(uint32_t minutes) {
    if (!wifi_is_valid_timeout(minutes)) {
        return false;
    }
    if (s_timeout_min == minutes) {
        return true;
    }
    s_prefs.putUInt("timeout_min", minutes);
    s_timeout_min = minutes;
    app_log("WIFI", "Wi-Fi idle timeout set to %u minute(s)", (unsigned int)minutes);
    return true;
}

wifi_radio_state_t wifi_manager_get_radio_state(void) {
    return s_radio_state;
}

void wifi_manager_mark_activity(void) {
    s_last_activity_ms = millis();
}

uint32_t wifi_manager_get_last_activity_ms(void) {
    return s_last_activity_ms;
}

bool wifi_manager_request_wifi(wifi_wake_reason_t reason) {
    if (s_policy == WIFI_POLICY_DISABLED) {
        app_log("WIFI", "Wi-Fi request ignored: policy DISABLED (reason=%d)", (int)reason);
        return false;
    }
    // Phase 2: radio stays on whenever not DISABLED. Idle shutdown & transient
    // ENABLING/SHUTTING_DOWN handling is implemented in a later phase.
    wifi_manager_mark_activity();
    return s_radio_state != WIFI_STATE_OFF;
}

const char* wifi_manager_policy_str(wifi_policy_t policy) {
    switch (policy) {
        case WIFI_POLICY_ALWAYS_ON: return "always_on";
        case WIFI_POLICY_ON_DEMAND: return "on_demand";
        case WIFI_POLICY_DISABLED:  return "disabled";
        default:                    return "unknown";
    }
}

const char* wifi_manager_state_str(wifi_radio_state_t state) {
    switch (state) {
        case WIFI_STATE_OFF:           return "off";
        case WIFI_STATE_ENABLING:      return "enabling";
        case WIFI_STATE_ON:            return "on";
        case WIFI_STATE_SHUTTING_DOWN: return "shutting_down";
        default:                       return "unknown";
    }
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

String wifi_manager_get_sta_ssid(void) {
    return s_prefs.getString("ssid", "");
}

String wifi_manager_get_sta_pass(void) {
    return s_prefs.getString("pass", "");
}

bool wifi_manager_restore_backup(const String& ssid, const String& sta_pass,
                                 const String& ap_pass, wifi_policy_t policy,
                                 uint32_t timeout_min) {
    if ((uint32_t)policy > WIFI_POLICY_DISABLED) {
        return false;
    }
    if (!wifi_is_valid_timeout(timeout_min)) {
        return false;
    }
    String ap = ap_pass;
    ap.trim();
    if (ap.length() > 0 && ap.length() < 8) {
        return false;
    }
    s_prefs.putString("ssid", ssid);
    s_prefs.putString("pass", sta_pass);
    s_prefs.putString("ap_pass", ap);
    s_prefs.putUInt("policy", (uint32_t)policy);
    s_prefs.putUInt("timeout_min", timeout_min);
    s_policy = policy;
    s_timeout_min = timeout_min;
    s_wifi_enabled = (policy != WIFI_POLICY_DISABLED);
    s_sta_configured = (ssid.length() > 0);
    app_log("WIFI", "Wi-Fi config restored from backup (policy=%s, timeout=%u min, ssid=%s)",
            wifi_manager_policy_str(policy), (unsigned int)timeout_min,
            ssid.length() > 0 ? ssid.c_str() : "(none)");
    return true;
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
