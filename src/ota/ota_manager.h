#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    OTA_WATCHDOG_CONTINUE = 0,
    OTA_WATCHDOG_ROLLBACK = 1
} ota_watchdog_action_t;

typedef struct {
    bool          enabled;          /* build supports OTA and an OTA slot exists */
    bool          in_progress;
    bool          last_success;
    uint32_t      last_error;
    uint32_t      running_addr;
    const char*   running_label;
    uint32_t      target_addr;
    const char*   target_label;
    uint32_t      target_capacity;
    uint32_t      received;
    uint32_t      total;
    uint32_t      boot_fail_count;  /* consecutive unconfirmed OTA boots */
    bool          rollback_armed;   /* an OTA boot is awaiting uptime confirmation */
} ota_status_t;

void               ota_manager_init(void);
bool               ota_manager_is_supported(void);
const ota_status_t* ota_manager_get_status(void);
bool               ota_manager_begin(uint32_t image_size);
size_t             ota_manager_write(const uint8_t* data, size_t len);
bool               ota_manager_end(void);
void               ota_manager_abort(void);
const char*        ota_manager_error_str(void);

/* ---- Safe-boot watchdog (roll back a broken OTA) ----
 * After a successful OTA the new image must confirm it boots OK within a grace
 * period, otherwise the boot counter keeps growing and the device flips back
 * to the previous OTA partition. Guards against "boots but crashes at runtime"
 * bricking, without needing bootloader-level rollback support. */
ota_watchdog_action_t ota_manager_watchdog_tick(void);   /* call once at startup */
void               ota_manager_watchdog_confirm(void);   /* call periodically in loop() */