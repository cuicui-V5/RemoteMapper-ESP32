#include "remote_events.h"
#include "controller.h"
#include "keymap/key_definitions.h"

#include <ArduinoJson.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <inttypes.h>

static remote_event_journal_t s_journal;
static portMUX_TYPE s_mutex = portMUX_INITIALIZER_UNLOCKED;
static char s_boot_id[17];
static bool s_pressed[256];
static bool s_voice_active;

static uint8_t canonical_key(uint8_t key) {
    switch (key) {
        case MI_KEY_POWER_ALT: return MI_KEY_POWER;
        case MI_KEY_VOICE_ALT: return MI_KEY_VOICE;
        case MI_KEY_HOME_ALT: return MI_KEY_HOME;
        case MI_KEY_MENU_ALT: return MI_KEY_MENU;
        case MI_KEY_TV_ALT: return MI_KEY_TV;
        default: return key;
    }
}

void remote_events_init(void) {
    remote_event_journal_init(&s_journal);
    snprintf(s_boot_id, sizeof(s_boot_id), "%08" PRIx32 "%08" PRIx32, esp_random(), esp_random());
}

bool remote_events_record(uint8_t key, bool pressed, remote_event_source_t source) {
    key = canonical_key(key);
    uint64_t time_ms = (uint64_t)esp_timer_get_time() / 1000;
    portENTER_CRITICAL(&s_mutex);
    bool changed = true;
    if (source == REMOTE_EVENT_ATVV) {
        changed = s_voice_active != pressed;
        s_voice_active = pressed;
    } else if (source == REMOTE_EVENT_DISCONNECT) {
        memset(s_pressed, 0, sizeof(s_pressed));
        s_voice_active = false;
    } else {
        changed = s_pressed[key] != pressed;
        s_pressed[key] = pressed;
    }
    if (changed) {
        // 队列溢出通过 dropped 持续报告，Mac 收到后终止当前任务。
        remote_event_journal_push(&s_journal, time_ms, key, pressed, source);
    }
    portEXIT_CRITICAL(&s_mutex);
    return agent_controller_event(key, pressed, source, changed);
}

String remote_events_command(const String &arguments) {
    JsonDocument request;
    if (arguments.length()) {
        if (deserializeJson(request, arguments) || !request.is<JsonObject>()) {
            return "{\"error\":\"invalid_event_request\"}";
        }
        for (JsonPair item : request.as<JsonObject>()) {
            String key = item.key().c_str();
            if (key != "begin" && key != "boot_id" && key != "stream_id" && key != "ack") {
                return "{\"error\":\"unknown_event_argument\"}";
            }
        }
    }
    bool begin = request["begin"].is<bool>() && request["begin"].as<bool>();
    bool has_ack = !request["ack"].isNull();
    if ((!request["begin"].isNull() && !request["begin"].is<bool>()) ||
        (begin && has_ack) ||
        (has_ack && (!request["ack"].is<uint32_t>() ||
                     !request["stream_id"].is<uint32_t>() || !request["boot_id"].is<const char *>())) ) {
        return "{\"error\":\"invalid_event_request\"}";
    }
    if (has_ack && strcmp(request["boot_id"].as<const char *>(), s_boot_id)) {
        return "{\"error\":\"boot_changed\"}";
    }
    remote_event_t events[REMOTE_EVENT_BATCH];
    bool pressed[256];
    uint32_t stream_id, acknowledged, dropped, latest;
    size_t count, pending;
    bool voice_active;
    uint32_t ack = has_ack ? request["ack"].as<uint32_t>() : 0;
    uint32_t stream = has_ack ? request["stream_id"].as<uint32_t>() : 0;
    const char *error = nullptr;
    portENTER_CRITICAL(&s_mutex);
    if (begin) remote_event_journal_begin(&s_journal);
    if (has_ack && stream != s_journal.stream_id) error = "stream_changed";
    else if (has_ack && !remote_event_journal_ack(&s_journal, ack)) error = "invalid_event_ack";
    count = error ? 0 : remote_event_journal_read(&s_journal, events, REMOTE_EVENT_BATCH);
    stream_id = s_journal.stream_id;
    acknowledged = s_journal.acknowledged;
    dropped = s_journal.dropped;
    latest = s_journal.next_sequence - 1;
    pending = s_journal.count;
    voice_active = s_voice_active;
    memcpy(pressed, s_pressed, sizeof(pressed));
    portEXIT_CRITICAL(&s_mutex);
    JsonDocument response;
    if (error) {
        response["error"] = error;
    } else {
        response["protocol"] = 1;
        response["boot_id"] = s_boot_id;
        response["stream_id"] = stream_id;
        response["acknowledged"] = acknowledged;
        response["latest"] = latest;
        response["pending"] = pending;
        response["dropped"] = dropped;
        response["capacity"] = REMOTE_EVENT_CAPACITY;
        response["voice_active"] = voice_active;
        JsonArray held = response["pressed"].to<JsonArray>();
        for (size_t key = 0; key < 256; key++) if (pressed[key]) held.add(key);
        JsonArray batch = response["events"].to<JsonArray>();
        for (size_t index = 0; index < count; index++) {
            JsonObject item = batch.add<JsonObject>();
            item["sequence"] = events[index].sequence;
            item["time_ms"] = events[index].time_ms;
            item["key"] = events[index].key;
            item["pressed"] = (bool)events[index].pressed;
            item["source"] = events[index].source;
        }
    }
    String result;
    serializeJson(response, result);
    return result;
}
