#include "usb_composite.h"
#include "uac_microphone.h"
#include "audio/audio_pipeline.h"
#include "log/app_log.h"
#include "led_indicator.h"
#include "agent/controller.h"
#include "keymap/mac_pointer.h"
#include <Arduino.h>
#include <atomic>
#include "USB.h"
#include "USBCDC.h"
#include "USBHIDKeyboard.h"
#include "USBHIDConsumerControl.h"
#ifdef REMOTEMAPPER_MACOS
#include "USBHIDMouse.h"
#endif
#include "tusb.h"
#include "esp32-hal-tinyusb.h"
#include "esp_system.h"

#if !ARDUINO_USB_CDC_ON_BOOT
USBCDC USBSerial;
#endif

static USBHIDKeyboard        s_keyboard;
static USBHIDConsumerControl s_consumer;
static USBHID                s_hid_reports;
#ifdef REMOTEMAPPER_MACOS
static USBHIDMouse s_mouse;
static std::atomic<uint32_t> s_mouse_sent{0};
static std::atomic<uint32_t> s_mouse_failed{0};
#endif
static bool                  s_usb_ready = false;
static SemaphoreHandle_t      s_hid_mutex = NULL;

static uint32_t              s_voice_press_ms       = 0;
static bool s_voice_mac = true;
static std::atomic<uint32_t> s_consumer_sent{0};
static std::atomic<uint32_t> s_consumer_failed{0};
static std::atomic<uint32_t> s_keyboard_sent{0};
static std::atomic<uint32_t> s_keyboard_failed{0};

extern "C" {

void usb_composite_init(void) {
    s_hid_mutex = xSemaphoreCreateRecursiveMutex();
    configASSERT(s_hid_mutex != NULL);
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
#ifdef REMOTEMAPPER_MACOS
        if (id == ARDUINO_USB_STOPPED_EVENT) {
            mac_pointer_event(0, false, REMOTE_EVENT_DISCONNECT, true);
        }
#endif
        if (id == ARDUINO_USB_SUSPEND_EVENT) {
            app_log("USB", "USB Suspend Event (PC Sleep/Standby)");
        } else if (id == ARDUINO_USB_RESUME_EVENT) {
            app_log("USB", "USB Resume Event (PC Woke up)");
            s_keyboard.releaseAll();
            s_consumer.release();
        } else if (id == ARDUINO_USB_STARTED_EVENT) {
            app_log("USB", "USB Started / Mounted");
        } else if (id == ARDUINO_USB_STOPPED_EVENT) {
            app_log("USB", "USB Stopped / Bus Reset");
        }
    });

#if !ARDUINO_USB_CDC_ON_BOOT
    USBSerial.enableReboot(false);
    USBSerial.begin();
#endif

    uac_microphone_init();

    s_keyboard.begin();
    s_consumer.begin();
#ifdef REMOTEMAPPER_MACOS
    s_mouse.begin();
#endif
    USB.begin();
    s_usb_ready = true;
}

void usb_composite_task(void) {
    uac_microphone_task();
}

static void hid_lock(void) {
    const BaseType_t result = xSemaphoreTakeRecursive(s_hid_mutex, portMAX_DELAY);
    configASSERT(result == pdTRUE);
}

static void hid_unlock(void) {
    const BaseType_t result = xSemaphoreGiveRecursive(s_hid_mutex);
    configASSERT(result == pdTRUE);
}

bool usb_hid_keyboard_press(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    hid_lock();

    if (tud_suspended()) {
        app_log("USB_HW", "Key pressed while PC asleep -> Sending Remote Wakeup!");
        tud_remote_wakeup();
        vTaskDelay(pdMS_TO_TICKS(15));
    }

    KeyReport report = {0};
    report.modifiers = modifier;
    report.keys[0] = keycode;
    bool sent = s_hid_reports.SendReport(HID_REPORT_ID_KEYBOARD, &report, sizeof(report));
    if (sent) s_keyboard_sent.fetch_add(1, std::memory_order_relaxed);
    else s_keyboard_failed.fetch_add(1, std::memory_order_relaxed);

    hid_unlock();
    app_log("USB_HID", "Keyboard press mod=0x%02X key=0x%02X sent=%d", modifier, keycode, sent);
    return sent;
}

bool usb_hid_keyboard_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();

    KeyReport report = {0};
    bool sent = s_hid_reports.SendReport(HID_REPORT_ID_KEYBOARD, &report, sizeof(report));
    if (sent) s_keyboard_sent.fetch_add(1, std::memory_order_relaxed);
    else s_keyboard_failed.fetch_add(1, std::memory_order_relaxed);

    hid_unlock();
    app_log("USB_HID", "Keyboard release sent=%d", sent);
    return sent;
}

bool usb_hid_keyboard_tap(uint8_t modifier, uint8_t keycode) {
    if (!s_usb_ready) return false;
    hid_lock();
    const bool has_chord = modifier != 0 && keycode != 0;
    // 组合键按修饰键、普通键、普通键释放、修饰键释放的顺序发送。
    if (has_chord) {
        if (!usb_hid_keyboard_press(modifier, 0)) {
            usb_hid_keyboard_release();
            hid_unlock();
            return false;
        }
        delay(15);
    }
    bool pressed = usb_hid_keyboard_press(modifier, keycode);
    delay(15);
    bool key_released = true;
    if (has_chord) {
        key_released = usb_hid_keyboard_press(modifier, 0);
        delay(15);
    }
    bool released = usb_hid_keyboard_release();
    hid_unlock();
    return pressed && key_released && released;
}

bool usb_hid_consumer_press(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    hid_lock();

    if (tud_suspended()) {
        app_log("USB", "PC Suspended -> tud_remote_wakeup");
        tud_remote_wakeup();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    bool sent = s_consumer.press(usage_code);
    if (sent) s_consumer_sent.fetch_add(1, std::memory_order_relaxed);
    else s_consumer_failed.fetch_add(1, std::memory_order_relaxed);
    hid_unlock();
    app_log("USB_HID", "Consumer press usage=0x%04X sent=%d", usage_code, sent);
    return sent;
}

bool usb_hid_consumer_release(void) {
    if (!s_usb_ready) return false;
    hid_lock();

    bool sent = s_consumer.release();
    if (sent) s_consumer_sent.fetch_add(1, std::memory_order_relaxed);
    else s_consumer_failed.fetch_add(1, std::memory_order_relaxed);
    hid_unlock();
    app_log("USB_HID", "Consumer release sent=%d", sent);
    return sent;
}

bool usb_hid_consumer_tap(uint16_t usage_code) {
    if (!s_usb_ready) return false;
    bool pressed = usb_hid_consumer_press(usage_code);
    delay(15);
    bool released = usb_hid_consumer_release();
    return pressed && released;
}

uint32_t usb_hid_consumer_sent(void) { return s_consumer_sent.load(std::memory_order_relaxed); }
uint32_t usb_hid_consumer_failed(void) { return s_consumer_failed.load(std::memory_order_relaxed); }
uint32_t usb_hid_keyboard_sent(void) { return s_keyboard_sent.load(std::memory_order_relaxed); }
uint32_t usb_hid_keyboard_failed(void) { return s_keyboard_failed.load(std::memory_order_relaxed); }

#ifdef REMOTEMAPPER_MACOS
bool usb_hid_mouse_report(uint8_t buttons, int8_t x, int8_t y, int8_t wheel, int8_t pan) {
    if (!s_usb_ready) return false;
    hid_lock();
    if (tud_suspended()) {
        tud_remote_wakeup();
        vTaskDelay(pdMS_TO_TICKS(15));
    }
    hid_mouse_report_t report = {buttons, x, y, wheel, pan};
    bool sent = s_hid_reports.SendReport(HID_REPORT_ID_MOUSE, &report, sizeof(report));
    if (sent) s_mouse_sent.fetch_add(1, std::memory_order_relaxed);
    else s_mouse_failed.fetch_add(1, std::memory_order_relaxed);
    hid_unlock();
    return sent;
}

uint32_t usb_hid_mouse_sent(void) { return s_mouse_sent.load(std::memory_order_relaxed); }
uint32_t usb_hid_mouse_failed(void) { return s_mouse_failed.load(std::memory_order_relaxed); }
#endif

void usb_hid_dispatch_action(const key_action_t *action) {
    if (!action) return;

    app_log("USB_HID", "Emit Action: type=%d, mod=0x%02X, key=0x%02X, cons=0x%04X", 
            action->type, action->modifier, action->key_code, action->consumer_code);

    if (action->type == ACTION_VOICE_HOLD || action->type == ACTION_VOICE_TOGGLE) {
        led_indicator_set(LED_STATE_MIC_STREAMING); // Solid Blue while voice recording
    } else if (action->type == ACTION_VOICE_RELEASE || action->type == ACTION_VOICE_TOGGLE_RELEASE) {
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
            s_voice_mac = agent_controller_mac_mode();
            if (s_voice_mac && (action->modifier != 0 || action->key_code != 0)) {
                usb_hid_keyboard_press(action->modifier, action->key_code);
            }
            break;
        case ACTION_VOICE_RELEASE:
            // Release Voice Hotkey and stop audio session
            if (s_voice_mac) usb_hid_keyboard_release();
            audio_pipeline_stop_session(&g_audio_pipeline);
            break;
        case ACTION_VOICE_TOGGLE:
            // Start audio session and tap hotkey to trigger start (TypeLess / Bageshuo)
            audio_pipeline_start_session(&g_audio_pipeline, 0);
            s_voice_press_ms = millis();
            s_voice_mac = agent_controller_mac_mode();
            if (s_voice_mac && (action->modifier != 0 || action->key_code != 0)) {
                usb_hid_keyboard_tap(action->modifier, action->key_code);
            }
            break;
        case ACTION_VOICE_TOGGLE_RELEASE: {
            // Guard against too-fast release (<120ms) so target app receives discrete start & stop taps
            uint32_t elapsed = millis() - s_voice_press_ms;
            if (elapsed < 120) {
                delay(120 - elapsed);
            }
            // Tap hotkey again to trigger stop & transcribe, then end audio session
            if (s_voice_mac && (action->modifier != 0 || action->key_code != 0)) {
                usb_hid_keyboard_tap(action->modifier, action->key_code);
            }
            audio_pipeline_stop_session(&g_audio_pipeline);
            break;
        }
        default:
            break;
    }
}

void usb_audio_task(void) {
    uac_microphone_task();
}

} // extern "C"
