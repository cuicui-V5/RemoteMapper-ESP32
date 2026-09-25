#include "config_manager.h"
#include "app_config.h"
#include "wifi/wifi_manager.h"
#include "log/app_log.h"
#include <Preferences.h>

static bool migrate_v0_to_v1(void) {
    // V0 = legacy firmware without configVersion. Introduce the V1 config
    // fields with DEFAULTS ONLY. Existing user settings are never touched.
    Preferences wifi;
    bool ok = true;
    if (wifi.begin("wifi_conf", false)) {
        if (!wifi.isKey("policy")) {
            wifi_policy_t policy = (wifi_policy_t)WIFI_DEFAULT_POLICY;
            // Legacy whole-radio disable (wifi_enabled=false) maps to DISABLED
            if (wifi.isKey("wifi_enabled") && !wifi.getBool("wifi_enabled", true)) {
                policy = WIFI_POLICY_DISABLED;
            }
            wifi.putUInt("policy", (uint32_t)policy);
            app_log("CONFIG", "Migrated wifi_conf: policy default -> %s",
                    wifi_manager_policy_str(policy));
        }
        if (!wifi.isKey("timeout_min")) {
            wifi.putUInt("timeout_min", WIFI_DEFAULT_TIMEOUT_MIN);
            app_log("CONFIG", "Migrated wifi_conf: timeout_min default -> %u",
                    WIFI_DEFAULT_TIMEOUT_MIN);
        }
        wifi.end();
    }
    return ok;
}

uint32_t config_manager_get_version(void) {
    Preferences prefs;
    if (!prefs.begin(CONFIG_NAMESPACE, true)) {
        return 0;
    }
    uint32_t version = prefs.getUInt(CONFIG_KEY_VERSION, 0);
    prefs.end();
    return version;
}

bool config_manager_set_version(uint32_t version) {
    Preferences prefs;
    if (!prefs.begin(CONFIG_NAMESPACE, false)) {
        return false;
    }
    bool ok = prefs.putUInt(CONFIG_KEY_VERSION, version) != 0;
    prefs.end();
    return ok;
}

void config_manager_init(void) {
    uint32_t current = config_manager_get_version();
    if (current >= CONFIG_VERSION_CURRENT) {
        app_log("CONFIG", "Config schema up-to-date (v%u)", current);
        return;
    }

    app_log("CONFIG", "Config migration started: v%u -> v%u", current, (uint32_t)CONFIG_VERSION_CURRENT);
    while (current < CONFIG_VERSION_CURRENT) {
        switch (current) {
            case 0:
                migrate_v0_to_v1();
                break;
            default:
                app_log("CONFIG", "No migration defined for v%u, skipping", current);
                break;
        }
        current++;
        config_manager_set_version(current);
    }
    app_log("CONFIG", "Config migration complete: v%u", current);
}