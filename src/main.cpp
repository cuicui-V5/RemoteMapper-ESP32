#include <Arduino.h>
#include "app_config.h"
#include "version.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "web/web_server.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include "keymap/key_config_storage.h"
#include "usb/usb_composite.h"
#include "ble/ble_remote_client.h"
#include "cli/cli_manager.h"
#include "led_indicator.h"

#include <nvs_flash.h>
#include <nvs.h>

key_mapper_engine_t g_key_engine;

// Task running on Core 0: BLE Central & Audio Decoding
static void ble_task_core0(void* param) {
    app_log("SYSTEM", "BLE & Audio Task started on Core %d", xPortGetCoreID());
    ble_remote_init();

    while (true) {
        ble_remote_task();
        key_engine_tick(&g_key_engine, millis());
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void setup() {
    // 0. Initialize Flash NVS with auto-recovery for corrupted partitions
    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 1. Initialize Log System first
    app_log_init();
    
    // 1.5. Initialize LED Indicator
    led_indicator_init();

    // 2. Initialize USB Composite Stack (UAC Mic + HID Keyboard + Consumer + CDC)
    usb_composite_init();
    Serial.begin(115200);
    delay(200);

    app_log("SYSTEM", "==================================================");
    app_log("SYSTEM", " %s v%s (%s)", FIRMWARE_NAME, FIRMWARE_VERSION, HARDWARE_TARGET);
    app_log("SYSTEM", " Xiaomi Remote Hardware Bridge (BLE -> USB + Web)");
    app_log("SYSTEM", "==================================================");

    // 3. Initialize Audio Pipeline
    audio_pipeline_init(&g_audio_pipeline);
    app_log("INIT", "Audio Pipeline initialized (16kHz 16-bit Mono UAC 1.0)");

    // 4. Initialize Key Engine with USB HID dispatcher callback and restore NVS mappings
    key_engine_init(&g_key_engine, usb_hid_dispatch_action);
    key_config_storage_init(&g_key_engine);
    app_log("INIT", "Key Engine active with %u layers (Layer 0 has %u mappings)", 
            (unsigned int)g_key_engine.layer_count, (unsigned int)g_key_engine.layers[0].binding_count);

    // 5. Initialize Serial / CDC CLI Manager
    cli_manager_init();

    // 6. Initialize Wi-Fi AP + STA & Captive Portal
    wifi_manager_init();

    // 7. Initialize Embedded Web Server & REST APIs (Wi-Fi only).
    //    lwIP/esp_netif are initialized inside the Wi-Fi library; starting a TCP
    //    server while Wi-Fi is disabled triggers a lwIP assert and reboots the chip.
    if (wifi_manager_get_enabled()) {
        web_server_init();
    } else {
        app_log("SYSTEM", "Wi-Fi disabled: web server not started");
    }

    // 8. Launch BLE Central Task pinned to Core 0
    xTaskCreatePinnedToCore(
        ble_task_core0,
        "ble_audio_task",
        8192,
        NULL,
        PRIO_TASK_BLE,
        NULL,
        TASK_CORE_BLE
    );

    if (wifi_manager_get_enabled()) {
        app_log("SYSTEM", "System initialization complete. Web available at http://192.168.4.1 or http://remotemapper.local");
    } else {
        app_log("SYSTEM", "System initialization complete. Wi-Fi radio off (use CDC/UART: 'wifi on')");
    }
}

void loop() {
    uint32_t now = millis();

    // 1. Service TinyUSB & Audio push
    usb_composite_task();

    // 2. Service Wi-Fi & DNS tasks
    wifi_manager_task();

    // 3. Service HTTP Web Server (only when Wi-Fi is enabled)
    if (wifi_manager_get_enabled()) {
        web_server_task();
    }

    // 4. Service Serial / WebSerial CLI commands
    cli_manager_task();

    // No delay here — USB audio task handles its own timing via vTaskDelayUntil
    vTaskDelay(1); // yield to let higher-priority tasks run (USB, BLE)
}
