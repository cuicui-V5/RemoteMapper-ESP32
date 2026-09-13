#include "usb_composite.h"
#include "uac_microphone.h"
#include "audio/audio_pipeline.h"
#include "log/app_log.h"
#include "led_indicator.h"
#include <Arduino.h>
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#include "tusb.h"
#include "esp32-hal-tinyusb.h"
#include "esp_system.h"

#if !ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial;
#endif

static USBHIDKeyboard        s_keyboard;
static USBHIDConsumerControl s_consumer;
static bool                  s_usb_ready = false;

extern "C" void usbd_edpt_clear_stall(uint8_t rhport, uint8_t ep_addr);
extern "C" bool usbd_edpt_busy(uint8_t rhport, uint8_t ep_addr);

#include "soc/usb_struct.h"

static bool                  s_hw_sleep_detected    = false;
static uint32_t              s_hw_sleep_start_ms    = 0;
static uint16_t              s_last_soffn           = 0;
static uint32_t              s_last_soffn_change_ms = 0;
static uint32_t              s_boot_grace_until_ms  = 0;

extern "C" {

void usb_composite_init(void) {
    USB.VID(0x303A);
    USB.PID(0x8089);
    USB.productName("RemoteMapper Audio & Remote Bridge");
    USB.manufacturerName("RemoteMapper");
    USB.serialNumber("RM-ESP32S3-MIC02");
    USB.usbClass(0xEF);
    USB.usbSubClass(0x02);
    USB.usbProtocol(0x01); // MISC_PROTOCOL_IAD
    USB.usbAttributes(TUSB_DESC_CONFIG_ATT_SELF_POWERED | TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP);

    USB.onEvent([](void* arg, esp_event_base_t base, int32_t id, void* data) {
        if (id == ARDUINO_USB_SUSPEND_EVENT) {
            app_log("USB", "USB Suspend Event (PC Sleep/Standby)");
        } else if (id == ARDUINO_USB_RESUME_EVENT) {
            app_log("USB", "USB Resume Event (PC Woke up)");
            for (uint8_t ep = 1; ep <= 4; ep++) {
                usbd_edpt_clear_stall(0, (uint8_t)(ep | 0x80));
            }
            s_keyboard.releaseAll();
            s_consumer.release();
        } else if (id == ARDUINO_USB_STARTED_EVENT) {
            app_log("USB", "USB Started / Mounted");
            for (uint8_t ep = 1; ep <= 4; ep++) {
                usbd_edpt_clear_stall(0, (uint8_t)(ep | 0x80));
            }
        } else if (id == ARDUINO_USB_STOPPED_EVENT) {
            app_log("USB", "USB Stopped / Bus Reset");
        }
    });

#if !ARDUINO_USB_CDC_ON_BOOT
    USBSerial.begin();
#endif

    uac_microphone_init();

    s_keyboard.begin();
    s_consumer.begin();
    USB.begin();
    s_usb_ready = true;
}

void usb_composite_task(void) {
    uac_microphone_task();

    uint32_t now = millis();
    if (s_boot_grace_until_ms == 0) {
        s_boot_grace_until_ms = now + 5000; // 5s boot grace period
        s_last_soffn_change_ms = now;
    }
    if (now < s_boot_grace_until_ms) {
        return; // Don't check during initial bootup
    }

    // Read hardware DWC2 register on ESP32-S3
    uint32_t dsts = USB0.dsts;
    bool is_hw_suspended = (dsts & 1) != 0; // Bit 0: SuspSts (1 = Suspended)
    uint16_t current_soffn = (dsts >> 8) & 0x3FFF; // Bits 8-21: Frame Number (increments every 1ms when PC is awake)

    if (current_soffn != s_last_soffn) {
        s_last_soffn = current_soffn;
        s_last_soffn_change_ms = now;
    }

    // If SOF packets stopped for > 800ms, Windows is asleep/disconnected
    bool sof_active = (now - s_last_soffn_change_ms < 800);

    // 1. Detect PC entering Sleep
    if ((is_hw_suspended || !sof_active) && !s_hw_sleep_detected) {
        s_hw_sleep_detected = true;
        s_hw_sleep_start_ms = now;
        app_log("USB_HW", "PC Sleep Detected! (SuspSts=%d, SOF stopped)", is_hw_suspended ? 1 : 0);
    }
    // 2. Detect PC waking up (SOF resumed after sleeping for > 2s)
    else if (s_hw_sleep_detected && !is_hw_suspended && sof_active) {
        if (now - s_hw_sleep_start_ms >= 2000) {
            app_log("USB_HW", "PC Wakeup Detected! SOF resumed -> performing clean USB hardware re-enumeration...");
            vTaskDelay(pdMS_TO_TICKS(500));
            // Pull D+ low to signal disconnect to Windows kernel
            pinMode(20, OUTPUT);
            pinMode(19, OUTPUT);
            digitalWrite(20, LOW);
            digitalWrite(19, LOW);
            vTaskDelay(pdMS_TO_TICKS(250));
            esp_restart();
        } else {
            s_hw_sleep_detected = false;
        }
    }
}

static SemaphoreHandle_t s_hid_mutex = NULL;

static void hid_lock(void) {
    if (!s_hid_mutex) {
        s_hid_mutex = xSemaphoreCreateMutex();
    }
    if (s_hid_mutex) {
        xSemaphoreTake(s_hid_mutex, portMAX_DELAY);
    }
}

static void hid_unlock(void) {
    if (s_hid_mutex) {
        xSemaphoreGive(s_hid_mutex);
    }
}

bool usb_hid_keyboard_press(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    hid_lock();

    if (s_hw_sleep_detected || (USB0.dsts & 1) != 0 || tud_suspended()) {
        app_log("USB_HW", "Key pressed while PC asleep -> Sending Remote Wakeup!");
        tud_remote_wakeup();
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    KeyReport report = {0};
    report.modifiers = modifier;
    report.keys[0] = keycode;
    s_keyboard.sendReport(&report);

    hid_unlock();
    return true;
}

bool usb_hid_keyboard_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();

    KeyReport report = {0}; // All zeroes
    s_keyboard.sendReport(&report);
    s_keyboard.releaseAll();

    hid_unlock();
    return true;
}

bool usb_hid_keyboard_tap(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    usb_hid_keyboard_press(modifier, keycode);
    delay(15);
    usb_hid_keyboard_release();
    return true;
}

bool usb_hid_consumer_press(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    hid_lock();

    if (tud_suspended()) {
        app_log("USB", "PC Suspended -> tud_remote_wakeup");
        tud_remote_wakeup();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    s_consumer.press(usage_code);

    hid_unlock();
    return true;
}

bool usb_hid_consumer_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();

    s_consumer.release();

    hid_unlock();
    return true;
}

bool usb_hid_consumer_tap(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    usb_hid_consumer_press(usage_code);
    delay(15);
    usb_hid_consumer_release();
    return true;
}

void usb_hid_dispatch_action(const key_action_t *action) {
    if (!action) return;

    app_log("USB_HID", "Emit Action: type=%d, mod=0x%02X, key=0x%02X, cons=0x%04X", 
            action->type, action->modifier, action->key_code, action->consumer_code);

    if (action->type == ACTION_VOICE_HOLD) {
        led_indicator_set(LED_STATE_MIC_STREAMING); // Solid Blue while voice recording
    } else if (action->type == ACTION_VOICE_RELEASE) {
        led_indicator_set(LED_STATE_CONNECTED);     // Solid Green when voice recording ends
    } else {
        led_indicator_trigger_key(false);           // Yellow flash on ordinary HID key actions
    }

    switch (action->type) {
        case ACTION_KEYBOARD_TAP:
            usb_hid_keyboard_tap(action->modifier, action->key_code);
            break;
        case ACTION_KEYBOARD_HOLD:
            usb_hid_keyboard_press(action->modifier, action->key_code);
            break;
        case ACTION_KEYBOARD_RELEASE:
            usb_hid_keyboard_release();
            break;
        case ACTION_CONSUMER_TAP:
            usb_hid_consumer_tap(action->consumer_code);
            break;
        case ACTION_CONSUMER_HOLD:
            usb_hid_consumer_press(action->consumer_code);
            break;
        case ACTION_CONSUMER_RELEASE:
            usb_hid_consumer_release();
            break;
        case ACTION_VOICE_HOLD:
            // Hold Voice Hotkey and start audio session
            audio_pipeline_start_session(&g_audio_pipeline, 0);
            if (action->modifier != 0 || action->key_code != 0) {
                usb_hid_keyboard_press(action->modifier, action->key_code);
            }
            break;
        case ACTION_VOICE_RELEASE:
            // Release Voice Hotkey and stop audio session
            usb_hid_keyboard_release();
            audio_pipeline_stop_session(&g_audio_pipeline);
            break;
        default:
            break;
    }
}

void usb_audio_task(void) {
    uac_microphone_task();
}

} // extern "C"
