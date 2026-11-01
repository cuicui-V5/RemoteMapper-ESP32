#include "controller.h"
#include "audio/audio_pipeline.h"
#include "ble/ble_remote_client.h"
#include "keymap/key_definitions.h"
#include "keymap/key_state_machine.h"
#include "keymap/mac_pointer.h"
#include "led_indicator.h"
#include <ArduinoJson.h>
#include <atomic>
#include <freertos/queue.h>

extern key_mapper_engine_t g_key_engine;

enum Mode { MAC, ANDROID, AGENT };
enum Stage { IDLE, LISTENING, TRANSCRIBING, CONFIRM_TASK, MODEL, CONFIRM_ACTION, TOOL, DONE, CANCELLED, ERROR };
static const char *s_modes[] = {"mac", "android", "agent"};
static const char *s_stages[] = {"idle", "listening", "transcribing", "confirm_task", "model", "confirm_action", "tool", "done", "cancelled", "error"};
static std::atomic<unsigned> s_mode{MAC};
static std::atomic<bool> s_attached{false};
static std::atomic<bool> s_overflow{false};
static QueueHandle_t s_events;
static QueueHandle_t s_manual_keys;
static bool s_manual_task = false;
static Stage s_stage = IDLE;
static String s_session, s_text, s_action, s_detail;
static String s_last_request, s_last_reply;
static uint32_t s_task = 0, s_step = 0, s_heartbeat = 0, s_changed = 0, s_tv_down = 0, s_command = 0;
static bool s_tv_held = false;

static void stage(Stage value, const char *detail = "") {
    s_stage = value;
    s_changed = millis();
    s_detail = detail;
}

static void cancel(const char *reason, bool stop_voice = true) {
    xQueueReset(s_manual_keys);
    if (stop_voice && s_stage == LISTENING) {
        ble_remote_stop_voice();
        key_engine_release_all(&g_key_engine, millis());
    }
    s_action = "";
    stage(CANCELLED, reason);
}

static bool active() {
    return s_stage >= LISTENING && s_stage <= TOOL;
}

static void set_mode(unsigned value) {
    if (value == s_mode.load()) return;
    xQueueReset(s_manual_keys);
    key_engine_release_all(&g_key_engine, millis());
#ifdef REMOTEMAPPER_MACOS
    mac_pointer_reset();
#endif
    if (active()) cancel("mode_changed");
    s_mode.store(value);
}

static void begin_task(bool manual = false) {
    s_manual_task = manual;
    s_task++;
    s_step = 0;
    s_action = "";
    s_text = "";
}

void agent_controller_init() {
    s_events = xQueueCreate(32, sizeof(remote_event_t));
    s_manual_keys = xQueueCreate(32, sizeof(uint8_t));
    configASSERT(s_events != nullptr);
    configASSERT(s_manual_keys != nullptr);
#ifdef REMOTEMAPPER_MACOS
    mac_pointer_init();
#endif
}

bool agent_controller_mac_mode() {
    return s_mode.load() == MAC;
}

bool agent_controller_event(uint8_t key, bool pressed, remote_event_source_t source, bool changed) {
    bool attached = s_attached.load();
    if (attached && changed) {
        remote_event_t event = {};
        event.key = key;
        event.pressed = pressed;
        event.source = source;
        event.time_ms = millis();
        if (xQueueSend(s_events, &event, 0) != pdTRUE) s_overflow.store(true);
    }
#ifdef REMOTEMAPPER_MACOS
    if (s_mode.load() == MAC && mac_pointer_event(key, pressed, source, changed)) return false;
#endif
    // 语音的音频生命周期始终由已有按键引擎管理。
    return key == MI_KEY_VOICE || (s_mode.load() == MAC && !(attached && key == MI_KEY_TV));
}

static const char *manual_key(uint8_t key) {
    switch (key) {
        case MI_KEY_UP: return "up";
        case MI_KEY_DOWN: return "down";
        case MI_KEY_LEFT: return "left";
        case MI_KEY_RIGHT: return "right";
        case MI_KEY_OK: return "enter";
        case MI_KEY_BACK: return "back";
        case MI_KEY_HOME: return "home";
        case MI_KEY_VOL_UP: return "volume_up";
        case MI_KEY_VOL_DOWN: return "volume_down";
        default: return nullptr;
    }
}

void agent_controller_tick() {
    if (s_overflow.exchange(false)) {
        cancel("event_overflow");
        s_attached.store(false);
        set_mode(MAC);
    }
    if (s_attached.load() && (uint32_t)(millis() - s_heartbeat) > 5000) {
        cancel("host_disconnected");
        s_attached.store(false);
        set_mode(MAC);
    }
    remote_event_t event;
    while (xQueueReceive(s_events, &event, 0) == pdTRUE) {
        if (!s_attached.load()) continue;
        if (event.source == REMOTE_EVENT_DISCONNECT) {
            cancel("remote_disconnected");
            continue;
        }
        if (event.key == MI_KEY_TV) {
            if (event.pressed) { s_tv_down = event.time_ms; s_tv_held = true; }
            else {
                if (s_tv_held && event.time_ms - s_tv_down >= 700 && !g_audio_pipeline.active) set_mode((s_mode.load() + 1) % 3);
                s_tv_held = false;
            }
            continue;
        }
        if (s_mode.load() == MAC) continue;
        if (event.source == REMOTE_EVENT_ATVV) {
            if (event.pressed) {
                if (active()) cancel("new_voice_request", false);
                begin_task();
                stage(LISTENING);
            } else if (s_stage == LISTENING) stage(TRANSCRIBING);
            continue;
        }
        if (event.key == MI_KEY_VOICE || !event.pressed) continue;
        if (event.key == MI_KEY_BACK && active() && !s_manual_task) {
            cancel("physical_back");
            continue;
        }
        if (event.key == MI_KEY_OK && (s_stage == CONFIRM_TASK || s_stage == CONFIRM_ACTION)) {
            stage(s_stage == CONFIRM_TASK ? MODEL : TOOL);
            continue;
        }
        if (event.key == MI_KEY_OK && active() && !s_manual_task) continue;
        const char *key = manual_key(event.key);
        if (key && (s_mode.load() == ANDROID || active())) {
            if (active() && !s_manual_task) cancel("manual_takeover");
            s_mode.store(ANDROID);
            if (xQueueSend(s_manual_keys, &event.key, 0) != pdTRUE) {
                cancel("manual_queue_full");
                s_attached.store(false);
                set_mode(MAC);
                stage(ERROR, "manual_queue_full");
            }
        }
    }
    uint8_t manual;
    if (!active() && s_mode.load() == ANDROID && xQueueReceive(s_manual_keys, &manual, 0) == pdTRUE) {
        begin_task(true);
        JsonDocument action;
        action["operation"] = "press";
        action["key"] = manual_key(manual);
        action["manual"] = true;
        serializeJson(action, s_action);
        stage(TOOL);
    }
    if (active() && (uint32_t)(millis() - s_changed) > 120000) cancel("task_timeout");
    uint32_t color = 0;
#ifdef REMOTEMAPPER_MACOS
    if (s_mode.load() == MAC) {
        mac_pointer_tick();
        color = mac_pointer_color();
    }
#endif
    if (s_mode.load() != MAC) {
        color = s_mode.load() == ANDROID ? 0x001010 : 0x100010;
        if (s_stage == LISTENING) color = 0x000020;
        else if (s_stage == CONFIRM_TASK || s_stage == CONFIRM_ACTION) color = 0x181000;
        else if (s_stage == MODEL || s_stage == TOOL || s_stage == TRANSCRIBING) color = (millis() % 800 < 400) ? 0x100018 : 0x030006;
        else if (s_stage == ERROR) color = 0x200000;
        else if (s_stage == DONE) color = 0x001800;
    }
    led_indicator_agent_color(color);
}

static String status() {
    JsonDocument doc;
    doc["protocol"] = 1;
    doc["attached"] = s_attached.load();
    doc["mode"] = s_modes[s_mode.load()];
    doc["stage"] = s_stages[s_stage];
    doc["task"] = s_task;
    doc["step"] = s_step;
    doc["manual_pending"] = uxQueueMessagesWaiting(s_manual_keys);
    doc["text"] = s_text;
    doc["detail"] = s_detail;
    if (s_action.length()) {
        JsonDocument action;
        configASSERT(!deserializeJson(action, s_action));
        doc["action"] = action;
    }
    String result;
    serializeJson(doc, result);
    return result;
}

String agent_controller_command(const String &arguments) {
    agent_controller_tick();
    if (!arguments.length()) return status();
    JsonDocument request;
    if (deserializeJson(request, arguments) || !request.is<JsonObject>()) return "{\"error\":\"invalid_request\"}";
    const char *op = request["op"] | "";
    if (!strcmp(op, "attach")) {
        const char *session = request["session"] | "";
        if (strlen(session) != 32) return "{\"error\":\"invalid_session\"}";
        if (s_attached.load() && s_session != session) return "{\"error\":\"already_attached\"}";
        if (!s_attached.load()) {
            s_session = session;
            s_command = 0;
            s_last_request = "";
            s_last_reply = "";
            s_text = "";
            s_action = "";
            stage(IDLE);
            s_attached.store(true);
        }
        s_heartbeat = millis();
        return status();
    }
    if (!s_attached.load() || s_session != (request["session"] | "")) return "{\"error\":\"session_expired\"}";
    if (!request["command"].is<uint32_t>()) return "{\"error\":\"missing_command\"}";
    uint32_t command = request["command"];
    if (command == s_command && arguments == s_last_request) return s_last_reply;
    if (command != s_command + 1) return "{\"error\":\"command_out_of_order\"}";
    const char *error = nullptr;
    if (!strcmp(op, "heartbeat")) s_heartbeat = millis();
    else if (!strcmp(op, "cancel")) cancel("host_cancelled");
    else if (!strcmp(op, "mode")) {
        String mode = request["mode"] | "";
        if (mode == s_modes[s_mode.load()]) { }
        else if (g_audio_pipeline.active) error = "voice_active";
        else if (mode == "mac") set_mode(MAC);
        else if (mode == "android") set_mode(ANDROID);
        else if (mode == "agent") set_mode(AGENT);
        else error = "invalid_mode";
    } else if (!strcmp(op, "submit")) {
        String text = request["text"] | "";
        if (s_mode.load() == MAC || (active() && s_stage != TRANSCRIBING)) error = "task_busy";
        else if (!text.length() || text.length() > 1024) error = "invalid_text";
        else if (s_stage == TRANSCRIBING && (!request["task"].is<uint32_t>() || request["task"].as<uint32_t>() != s_task)) error = "task_changed";
        else {
            if (s_stage != TRANSCRIBING) begin_task();
            s_text = text;
            stage(CONFIRM_TASK);
        }
    } else if (!request["task"].is<uint32_t>() || request["task"].as<uint32_t>() != s_task || !request["step"].is<uint32_t>() || request["step"].as<uint32_t>() != s_step) error = "task_changed";
    else if (!strcmp(op, "decision") && s_stage == MODEL) {
        JsonObject action = request["action"].as<JsonObject>();
        String operation = action["operation"] | "";
        if (operation == "done") { stage(DONE); s_detail = action["explanation"] | ""; }
        else if (operation == "failed") { stage(ERROR); s_detail = action["explanation"] | ""; }
        else if (operation == "app_start" || operation == "click" || operation == "set_text" || operation == "press") {
            if (measureJson(action) > 3072) error = "action_too_large";
            else { s_action = ""; serializeJson(action, s_action); stage(CONFIRM_ACTION); }
        } else error = "unknown_action";
    } else if (!strcmp(op, "result") && s_stage == TOOL) {
        bool ok = request["ok"] | false;
        JsonDocument action;
        configASSERT(!deserializeJson(action, s_action));
        bool manual = action["manual"] | false;
        s_action = "";
        if (!ok) { xQueueReset(s_manual_keys); stage(ERROR, "tool_failed"); }
        else if (manual) stage(DONE);
        else if (++s_step >= 12) stage(ERROR, "step_limit");
        else stage(MODEL);
    } else if (!strcmp(op, "fail")) { xQueueReset(s_manual_keys); stage(ERROR, "host_operation_failed"); }
    else error = "invalid_transition";
    s_command = command;
    s_last_request = arguments;
    s_last_reply = error ? String("{\"error\":\"") + error + "\"}" : status();
    return s_last_reply;
}
