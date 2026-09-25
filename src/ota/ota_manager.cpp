#include "ota_manager.h"
#include "log/app_log.h"
#include <Update.h>
#include <esp_ota_ops.h>
#include <Preferences.h>

#ifndef REMOTEMAPPER_OTA
#define REMOTEMAPPER_OTA 0
#endif

#define OTA_WATCHDOG_TRIES        3
#define OTA_WATCHDOG_CONFIRM_MS   (120000UL)
#define OTA_WATCHDOG_NS           "ota_boot"
#define OTA_WATCHDOG_KEY_PENDING  "pending"
#define OTA_WATCHDOG_KEY_FAIL     "fail"

static ota_status_t s_ota = {0};
static bool         s_watchdog_armed = false;

static void ota_refresh_partition_info(void) {
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running != NULL) {
        s_ota.running_addr = running->address;
        s_ota.running_label = running->label;
    }
    const esp_partition_t* next = esp_ota_get_next_update_partition(NULL);
    if (next != NULL) {
        s_ota.target_addr = next->address;
        s_ota.target_label = next->label;
        s_ota.target_capacity = next->size;
    }
}

static bool ota_has_ota_slot(void) {
    return esp_ota_get_next_update_partition(NULL) != NULL;
}

static void ota_mark_watchdog_pending(void) {
    Preferences p;
    if (!p.begin(OTA_WATCHDOG_NS, false)) {
        return;
    }
    p.putUChar(OTA_WATCHDOG_KEY_PENDING, 1);
    p.putUChar(OTA_WATCHDOG_KEY_FAIL, 0);
    p.end();
    s_watchdog_armed = true;
    s_ota.rollback_armed = true;
    s_ota.boot_fail_count = 0;
}

static void ota_mark_watchdog_confirmed(void) {
    Preferences p;
    if (p.begin(OTA_WATCHDOG_NS, false)) {
        p.remove(OTA_WATCHDOG_KEY_PENDING);
        p.remove(OTA_WATCHDOG_KEY_FAIL);
        p.end();
    }
    s_watchdog_armed = false;
    s_ota.rollback_armed = false;
    s_ota.boot_fail_count = 0;
    app_log("OTA", "Safe-boot watchdog cleared: new firmware confirmed healthy");
}

void ota_manager_init(void) {
    s_ota.in_progress = false;
    s_ota.last_success = false;
    s_ota.last_error = 0;
    s_ota.received = 0;
    s_ota.total = 0;
#if REMOTEMAPPER_OTA
    ota_refresh_partition_info();
    s_ota.enabled = (s_ota.target_capacity > 0);
    app_log("INIT", "OTA manager ready (%s@0x%x -> %s@0x%x, %u bytes)",
            s_ota.running_label ? s_ota.running_label : "-",
            s_ota.running_addr,
            s_ota.target_label ? s_ota.target_label : "-",
            s_ota.target_addr,
            s_ota.target_capacity);
#else
    s_ota.enabled = false;
    app_log("INIT", "OTA manager disabled in this build");
#endif

    // Safe-boot watchdog: if an OTA image is pending confirmation, count this
    // boot; roll back to the previous image after too many failed attempts.
    ota_watchdog_action_t act = ota_manager_watchdog_tick();
    if (act == OTA_WATCHDOG_ROLLBACK) {
        app_log("OTA", "Rolling back to previous firmware now...");
        delay(150);
        ESP.restart();
    }
}

bool ota_manager_is_supported(void) {
    return s_ota.enabled;
}

const ota_status_t* ota_manager_get_status(void) {
    return &s_ota;
}

bool ota_manager_begin(uint32_t image_size) {
    if (!s_ota.enabled) {
        app_log("OTA", "OTA begin rejected: not supported");
        return false;
    }
    if (image_size == 0) {
        image_size = UPDATE_SIZE_UNKNOWN;
    }
    if (image_size != UPDATE_SIZE_UNKNOWN && image_size > s_ota.target_capacity) {
        s_ota.last_error = UPDATE_ERROR_SIZE;
        app_log("OTA", "OTA begin rejected: image %u > slot capacity %u", image_size, s_ota.target_capacity);
        return false;
    }
    if (!Update.begin(image_size)) {
        s_ota.last_error = Update.getError();
        app_log("OTA", "Update.begin failed: %s", ota_manager_error_str());
        return false;
    }
    s_ota.in_progress = true;
    s_ota.last_success = false;
    s_ota.last_error = UPDATE_ERROR_OK;
    s_ota.received = 0;
    s_ota.total = image_size;
    app_log("OTA", "OTA upload started (size=%s, slot=%s)",
            image_size == UPDATE_SIZE_UNKNOWN ? "unknown" : String(image_size).c_str(),
            s_ota.target_label ? s_ota.target_label : "?");
    return true;
}

size_t ota_manager_write(const uint8_t* data, size_t len) {
    if (!s_ota.in_progress) {
        return 0;
    }
    size_t written = Update.write(const_cast<uint8_t*>(data), len);
    if (written > 0) {
        s_ota.received += written;
    }
    s_ota.last_error = Update.getError();
    return written;
}

bool ota_manager_end(void) {
    if (!s_ota.in_progress) {
        return false;
    }
    bool ok = Update.end(true);
    s_ota.last_success = ok;
    s_ota.in_progress = false;
    s_ota.last_error = Update.getError();
    if (ok) {
        ota_refresh_partition_info();
        ota_mark_watchdog_pending();
        app_log("OTA", "OTA success: next boot from %s@0x%x, safe-boot watchdog armed",
                s_ota.target_label ? s_ota.target_label : "-", s_ota.target_addr);
    } else {
        app_log("OTA", "OTA failed: %s", ota_manager_error_str());
    }
    return ok;
}

void ota_manager_abort(void) {
    if (!s_ota.in_progress) {
        return;
    }
    Update.abort();
    s_ota.last_error = Update.getError();
    s_ota.in_progress = false;
    app_log("OTA", "OTA upload aborted");
}

const char* ota_manager_error_str(void) {
    switch (Update.getError()) {
        case UPDATE_ERROR_OK:           return "OK";
        case UPDATE_ERROR_WRITE:        return "Flash 写入失败";
        case UPDATE_ERROR_ERASE:        return "Flash 擦除失败";
        case UPDATE_ERROR_READ:         return "新固件读取校验失败";
        case UPDATE_ERROR_SPACE:        return "Flash 空间不足";
        case UPDATE_ERROR_SIZE:         return "固件体积超过分区容量";
        case UPDATE_ERROR_STREAM:       return "固件流解析错误";
        case UPDATE_ERROR_MD5:          return "MD5 校验失败，固件已损坏";
        case UPDATE_ERROR_MAGIC_BYTE:   return "无效固件头，请确认选择正确型号的 .bin";
        case UPDATE_ERROR_ACTIVATE:     return "启动分区激活失败";
        case UPDATE_ERROR_NO_PARTITION: return "没有可用的 OTA 分区";
        case UPDATE_ERROR_BAD_ARGUMENT: return "参数错误";
        case UPDATE_ERROR_ABORT:        return "升级已中止（网络中断或手动取消）";
        default:                        return "未知错误";
    }
}

ota_watchdog_action_t ota_manager_watchdog_tick(void) {
    Preferences p;
    if (!p.begin(OTA_WATCHDOG_NS, false)) {
        return OTA_WATCHDOG_CONTINUE;
    }
    uint8_t pending = p.getUChar(OTA_WATCHDOG_KEY_PENDING, 0);
    if (!pending) {
        p.end();
        return OTA_WATCHDOG_CONTINUE;
    }

    uint8_t fail = p.getUChar(OTA_WATCHDOG_KEY_FAIL, 0) + 1;
    p.putUChar(OTA_WATCHDOG_KEY_FAIL, fail);
    s_ota.boot_fail_count = fail;
    s_watchdog_armed = true;
    s_ota.rollback_armed = true;

    if (fail >= OTA_WATCHDOG_TRIES) {
        // Too many unconfirmed boots: switch back to the other OTA partition.
        if (ota_has_ota_slot()) {
            const esp_partition_t* other = esp_ota_get_next_update_partition(NULL);
            esp_err_t err = esp_ota_set_boot_partition(other);
            if (err == ESP_OK) {
                p.remove(OTA_WATCHDOG_KEY_PENDING);
                p.remove(OTA_WATCHDOG_KEY_FAIL);
                p.end();
                app_log("OTA", "SAFE-BOOT ROLLBACK: %u consecutive failures, "
                        "switching boot to %s@0x%x", (unsigned int)fail,
                        other->label, (unsigned int)other->address);
                s_watchdog_armed = false;
                s_ota.rollback_armed = false;
                s_ota.boot_fail_count = 0;
                return OTA_WATCHDOG_ROLLBACK;
            }
            app_log("OTA", "Safe-boot rollback FAILED to set partition: %s",
                    esp_err_to_name(err));
        } else {
            app_log("OTA", "Safe-boot rollback impossible: no OTA partition found");
        }
    } else {
        app_log("OTA", "Safe-boot watchdog: boot %d/%d awaiting confirmation "
                "(new firmware seems unstable)", (unsigned int)fail,
                (unsigned int)OTA_WATCHDOG_TRIES);
    }
    p.end();
    return OTA_WATCHDOG_CONTINUE;
}

void ota_manager_watchdog_confirm(void) {
    if (!s_watchdog_armed) {
        return;
    }
    if (millis() < OTA_WATCHDOG_CONFIRM_MS) {
        return;
    }
    ota_mark_watchdog_confirmed();
}