#include "mac_pointer.h"
#include "remote_pointer.h"
#include "usb/usb_composite.h"
#include "log/app_log.h"
#include "tusb.h"
#include <ArduinoJson.h>
#include <atomic>
#include <freertos/queue.h>

#ifdef REMOTEMAPPER_MACOS
static remote_pointer_t s_pointer;
static QueueHandle_t s_events;
static std::atomic<bool> s_overflow{false};
static bool s_failed = false;

void mac_pointer_init() {
    remote_pointer_init(&s_pointer);
    s_events = xQueueCreate(32, sizeof(remote_event_t));
    configASSERT(s_events != nullptr);
}

static void send(const remote_pointer_report_t &report) {
    if (!usb_hid_mouse_report(report.buttons, report.x, report.y, report.wheel, report.pan)) {
        s_failed = true;
        remote_pointer_init(&s_pointer);
        xQueueReset(s_events);
        app_log("MOUSE", "USB mouse report failed; pointer stopped");
    }
}

bool mac_pointer_event(uint8_t key, bool pressed, remote_event_source_t source, bool changed) {
    bool handled = remote_pointer_handles(key);
    if (changed && (handled || source == REMOTE_EVENT_DISCONNECT)) {
        remote_event_t event = {};
        event.time_ms = millis();
        event.key = key;
        event.pressed = pressed;
        event.source = source;
        if (xQueueSend(s_events, &event, 0) != pdTRUE) s_overflow.store(true);
    }
    return handled;
}

void mac_pointer_reset() {
    xQueueReset(s_events);
    remote_pointer_report_t report;
    if (remote_pointer_reset(&s_pointer, &report) && tud_mounted()) send(report);
}

void mac_pointer_tick() {
    if (s_overflow.exchange(false)) {
        mac_pointer_reset();
        s_failed = true;
        app_log("MOUSE", "Pointer event queue overflow; pointer stopped");
    }
    if (s_failed) return;
    remote_event_t event;
    while (xQueueReceive(s_events, &event, 0) == pdTRUE) {
        remote_pointer_report_t report;
        if (event.source == REMOTE_EVENT_DISCONNECT) {
            mac_pointer_reset();
        } else if (remote_pointer_key(&s_pointer, event.key, event.pressed, event.time_ms, &report)) {
            send(report);
            if (s_failed) return;
        }
    }
    remote_pointer_report_t report;
    if (remote_pointer_tick(&s_pointer, millis(), &report)) send(report);
}

uint32_t mac_pointer_color() {
    if (s_failed) return 0x200000;
    if (s_pointer.dragging) return 0x180800;
    return s_pointer.scrolling ? 0x001818 : 0;
}

String mac_pointer_status() {
    JsonDocument document;
    document["scrolling"] = s_pointer.scrolling;
    document["dragging"] = s_pointer.dragging;
    document["failed"] = s_failed;
    document["sent"] = usb_hid_mouse_sent();
    document["send_failed"] = usb_hid_mouse_failed();
    String result;
    serializeJson(document, result);
    return result;
}
#endif
