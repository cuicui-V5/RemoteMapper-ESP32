#include "macro_manager.h"
#include "macro_action.h"
#include "key_state_machine.h"
#include "usb/usb_composite.h"
#include "log/app_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <ctype.h>

#define MACRO_SLOT_COUNT (5 * 16 * 3)

static String s_macro_scripts[MACRO_SLOT_COUNT];
static portMUX_TYPE s_macro_init_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_macro_mutex = nullptr;
static QueueHandle_t s_macro_queue = nullptr;
static TaskHandle_t s_macro_task = nullptr;

static void macro_ensure_mutex() {
    if (s_macro_mutex) return;
    portENTER_CRITICAL(&s_macro_init_mux);
    if (!s_macro_mutex) {
        s_macro_mutex = xSemaphoreCreateMutex();
    }
    portEXIT_CRITICAL(&s_macro_init_mux);
}

static void macro_lock() {
    macro_ensure_mutex();
    if (s_macro_mutex) xSemaphoreTake(s_macro_mutex, portMAX_DELAY);
}

static void macro_unlock() {
    if (s_macro_mutex) xSemaphoreGive(s_macro_mutex);
}

void macro_clear_scripts(void) {
    macro_lock();
    for (String& s : s_macro_scripts) s = "";
    macro_unlock();
}

bool macro_set_script(uint8_t slot, const String& script) {
    if (slot >= MACRO_SLOT_COUNT) return false;
    macro_lock();
    s_macro_scripts[slot] = script;
    macro_unlock();
    return true;
}

String macro_get_script(uint8_t slot) {
    if (slot >= MACRO_SLOT_COUNT) return "";
    macro_lock();
    String copy = s_macro_scripts[slot];
    macro_unlock();
    return copy;
}

static bool map_ascii_to_hid(char c, uint8_t* out_mod, uint8_t* out_key) {
    *out_mod = 0;
    *out_key = 0;
    if (c >= 'a' && c <= 'z') {
        *out_key = (uint8_t)(0x04 + (c - 'a'));
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        *out_mod = 0x02; // LShift
        *out_key = (uint8_t)(0x04 + (c - 'A'));
        return true;
    }
    if (c >= '1' && c <= '9') {
        *out_key = (uint8_t)(0x1E + (c - '1'));
        return true;
    }
    if (c == '0') { *out_key = 0x27; return true; }
    switch (c) {
        case ' ':  *out_key = 0x2C; return true;
        case '\n': *out_key = 0x28; return true;
        case '\t': *out_key = 0x2B; return true;
        case '-':  *out_key = 0x2D; return true;
        case '=':  *out_key = 0x2E; return true;
        case '[':  *out_key = 0x2F; return true;
        case ']':  *out_key = 0x30; return true;
        case '\\': *out_key = 0x31; return true;
        case ';':  *out_key = 0x33; return true;
        case '\'': *out_key = 0x34; return true;
        case '`':  *out_key = 0x35; return true;
        case ',':  *out_key = 0x36; return true;
        case '.':  *out_key = 0x37; return true;
        case '/':  *out_key = 0x38; return true;
        // Shifted symbols (US-QWERTY)
        case '!': *out_mod = 0x02; *out_key = 0x1E; return true;
        case '@': *out_mod = 0x02; *out_key = 0x1F; return true;
        case '#': *out_mod = 0x02; *out_key = 0x20; return true;
        case '$': *out_mod = 0x02; *out_key = 0x21; return true;
        case '%': *out_mod = 0x02; *out_key = 0x22; return true;
        case '^': *out_mod = 0x02; *out_key = 0x23; return true;
        case '&': *out_mod = 0x02; *out_key = 0x24; return true;
        case '*': *out_mod = 0x02; *out_key = 0x25; return true;
        case '(': *out_mod = 0x02; *out_key = 0x26; return true;
        case ')': *out_mod = 0x02; *out_key = 0x27; return true;
        case '_': *out_mod = 0x02; *out_key = 0x2D; return true;
        case '+': *out_mod = 0x02; *out_key = 0x2E; return true;
        case '{': *out_mod = 0x02; *out_key = 0x2F; return true;
        case '}': *out_mod = 0x02; *out_key = 0x30; return true;
        case '|': *out_mod = 0x02; *out_key = 0x31; return true;
        case ':': *out_mod = 0x02; *out_key = 0x33; return true;
        case '"': *out_mod = 0x02; *out_key = 0x34; return true;
        case '~': *out_mod = 0x02; *out_key = 0x35; return true;
        case '<': *out_mod = 0x02; *out_key = 0x36; return true;
        case '>': *out_mod = 0x02; *out_key = 0x37; return true;
        case '?': *out_mod = 0x02; *out_key = 0x38; return true;
        default: return false;
    }
}

static bool map_named_key(const String& raw_name, uint8_t* out_mod_bit, uint8_t* out_key, uint16_t* out_cons) {
    *out_mod_bit = 0;
    *out_key = 0;
    *out_cons = 0;

    String name = raw_name;
    name.trim();
    name.toLowerCase();

    // Modifiers
    if (name == "ctrl" || name == "lctrl" || name == "control" || name == "lcontrol") { *out_mod_bit = 0x01; return true; }
    if (name == "rctrl" || name == "rcontrol") { *out_mod_bit = 0x10; return true; }
    if (name == "shift" || name == "lshift") { *out_mod_bit = 0x02; return true; }
    if (name == "rshift") { *out_mod_bit = 0x20; return true; }
    if (name == "alt" || name == "lalt") { *out_mod_bit = 0x04; return true; }
    if (name == "ralt") { *out_mod_bit = 0x40; return true; }
    if (name == "win" || name == "lwin") { *out_mod_bit = 0x08; return true; }
    if (name == "rwin") { *out_mod_bit = 0x80; return true; }

    // Standard keys
    if (name == "enter" || name == "return") { *out_key = 0x28; return true; }
    if (name == "esc" || name == "escape") { *out_key = 0x29; return true; }
    if (name == "backspace" || name == "bs") { *out_key = 0x2A; return true; }
    if (name == "tab") { *out_key = 0x2B; return true; }
    if (name == "space") { *out_key = 0x2C; return true; }
    if (name == "capslock") { *out_key = 0x39; return true; }
    if (name == "printscreen" || name == "prtsc") { *out_key = 0x46; return true; }
    if (name == "scrolllock") { *out_key = 0x47; return true; }
    if (name == "pause") { *out_key = 0x48; return true; }
    if (name == "insert" || name == "ins") { *out_key = 0x49; return true; }
    if (name == "home") { *out_key = 0x4A; return true; }
    if (name == "pgup" || name == "pageup") { *out_key = 0x4B; return true; }
    if (name == "delete" || name == "del") { *out_key = 0x4C; return true; }
    if (name == "end") { *out_key = 0x4D; return true; }
    if (name == "pgdn" || name == "pagedown") { *out_key = 0x4E; return true; }
    if (name == "right") { *out_key = 0x4F; return true; }
    if (name == "left") { *out_key = 0x50; return true; }
    if (name == "down") { *out_key = 0x51; return true; }
    if (name == "up") { *out_key = 0x52; return true; }
    if (name == "numlock") { *out_key = 0x53; return true; }
    if (name == "apps" || name == "menu") { *out_key = 0x65; return true; }

    // Function keys F1..F12
    if (name.length() >= 2 && name[0] == 'f') {
        int fnum = name.substring(1).toInt();
        if (fnum >= 1 && fnum <= 12) {
            *out_key = (uint8_t)(0x3A + (fnum - 1));
            return true;
        }
    }

    // Consumer / Media keys
    if (name == "volume_up" || name == "volup") { *out_cons = 0x00E9; return true; }
    if (name == "volume_down" || name == "voldown") { *out_cons = 0x00EA; return true; }
    if (name == "volume_mute" || name == "mute") { *out_cons = 0x00E2; return true; }
    if (name == "media_play_pause" || name == "playpause") { *out_cons = 0x00CD; return true; }
    if (name == "media_next" || name == "next") { *out_cons = 0x00B5; return true; }
    if (name == "media_prev" || name == "prev") { *out_cons = 0x00B6; return true; }
    if (name == "media_stop" || name == "stop") { *out_cons = 0x00B7; return true; }

    // Single character inside braces e.g. {a}, {!}, {^}
    if (raw_name.length() == 1) {
        uint8_t m = 0, k = 0;
        if (map_ascii_to_hid(raw_name[0], &m, &k)) {
            *out_mod_bit = m;
            *out_key = k;
            return true;
        }
    }

    return false;
}

static void macro_send_tap(uint8_t mod, uint8_t key, uint16_t cons) {
    key_action_t act = {};
    if (cons > 0) {
        act.type = ACTION_CONSUMER_TAP;
        act.consumer_code = cons;
    } else {
        act.type = ACTION_KEYBOARD_TAP;
        act.modifier = mod;
        act.key_code = key;
    }
    usb_hid_dispatch_action(&act);
    vTaskDelay(pdMS_TO_TICKS(15));
}

static void execute_ahk_script(const String& script) {
    if (!script.length()) return;

    uint8_t held_mods = 0;
    uint8_t prefix_mods = 0;
    size_t i = 0;
    size_t len = script.length();

    while (i < len) {
        char c = script[i];

        // Skip carriage returns
        if (c == '\r') {
            i++;
            continue;
        }

        // AHK modifier prefixes
        if (c == '^') { prefix_mods |= 0x01; i++; continue; }
        if (c == '+') { prefix_mods |= 0x02; i++; continue; }
        if (c == '!') { prefix_mods |= 0x04; i++; continue; }
        if (c == '#') { prefix_mods |= 0x08; i++; continue; }

        if (c == '{') {
            int close = script.indexOf('}', (unsigned int)(i + 1));
            if (close < 0) {
                // Unclosed brace: treat as literal '{'
                uint8_t cm = 0, ck = 0;
                if (map_ascii_to_hid('{', &cm, &ck)) {
                    macro_send_tap(held_mods | prefix_mods | cm, ck, 0);
                }
                prefix_mods = 0;
                i++;
                continue;
            }

            String token = script.substring((unsigned int)(i + 1), (unsigned int)close);
            token.trim();
            i = (size_t)close + 1;

            if (!token.length()) {
                prefix_mods = 0;
                continue;
            }

            // Split key name and optional argument (down / up / count)
            int sp = token.indexOf(' ');
            String key_part = (sp < 0) ? token : token.substring(0, sp);
            String arg_part = (sp < 0) ? "" : token.substring(sp + 1);
            key_part.trim();
            arg_part.trim();

            String key_lower = key_part;
            key_lower.toLowerCase();
            String arg_lower = arg_part;
            arg_lower.toLowerCase();

            // {Sleep ms} / {Wait ms}
            if (key_lower == "sleep" || key_lower == "wait") {
                long ms = arg_part.toInt();
                if (ms < 0) ms = 0;
                if (ms > 30000) ms = 30000;
                if (ms > 0) vTaskDelay(pdMS_TO_TICKS((uint32_t)ms));
                prefix_mods = 0;
                continue;
            }

            uint8_t m_bit = 0, k_code = 0;
            uint16_t c_code = 0;
            if (!map_named_key(key_part, &m_bit, &k_code, &c_code)) {
                prefix_mods = 0;
                continue;
            }

            if (arg_lower == "down") {
                if (k_code == 0 && m_bit != 0) {
                    held_mods |= m_bit;
                    key_action_t act = {};
                    act.type = ACTION_KEYBOARD_HOLD;
                    act.modifier = held_mods | prefix_mods;
                    usb_hid_dispatch_action(&act);
                } else if (k_code != 0) {
                    key_action_t act = {};
                    act.type = ACTION_KEYBOARD_HOLD;
                    act.modifier = held_mods | prefix_mods | m_bit;
                    act.key_code = k_code;
                    usb_hid_dispatch_action(&act);
                }
                vTaskDelay(pdMS_TO_TICKS(15));
            } else if (arg_lower == "up") {
                if (k_code == 0 && m_bit != 0) {
                    held_mods &= (uint8_t)(~m_bit);
                    if (held_mods == 0) {
                        key_action_t act = {};
                        act.type = ACTION_KEYBOARD_RELEASE;
                        usb_hid_dispatch_action(&act);
                    } else {
                        key_action_t act = {};
                        act.type = ACTION_KEYBOARD_HOLD;
                        act.modifier = held_mods;
                        usb_hid_dispatch_action(&act);
                    }
                } else {
                    key_action_t act = {};
                    act.type = ACTION_KEYBOARD_RELEASE;
                    usb_hid_dispatch_action(&act);
                    if (held_mods != 0) {
                        key_action_t rehold = {};
                        rehold.type = ACTION_KEYBOARD_HOLD;
                        rehold.modifier = held_mods;
                        usb_hid_dispatch_action(&rehold);
                    }
                }
                vTaskDelay(pdMS_TO_TICKS(15));
            } else {
                int count = arg_part.length() ? arg_part.toInt() : 1;
                if (count < 1) count = 1;
                if (count > 50) count = 50;
                for (int r = 0; r < count; r++) {
                    macro_send_tap(held_mods | prefix_mods | m_bit, k_code, c_code);
                }
            }

            prefix_mods = 0;
            continue;
        }

        // Plain character
        uint8_t cm = 0, ck = 0;
        if (map_ascii_to_hid(c, &cm, &ck)) {
            macro_send_tap(held_mods | prefix_mods | cm, ck, 0);
        }
        prefix_mods = 0;
        i++;
    }

    if (held_mods != 0) {
        key_action_t rel = {};
        rel.type = ACTION_KEYBOARD_RELEASE;
        usb_hid_dispatch_action(&rel);
    }
}

static void macro_worker_task(void* param) {
    (void)param;
    uint8_t slot = 0;
    while (true) {
        if (xQueueReceive(s_macro_queue, &slot, portMAX_DELAY) == pdTRUE) {
            String script = macro_get_script(slot);
            if (!script.length()) {
                app_log("MACRO", "Empty macro script for slot %u", slot);
                continue;
            }
            execute_ahk_script(script);
        }
    }
}

static bool macro_ensure_worker() {
    macro_ensure_mutex();
    if (!s_macro_queue) {
        s_macro_queue = xQueueCreate(8, sizeof(uint8_t));
        if (!s_macro_queue) return false;
    }
    if (!s_macro_task) {
        if (xTaskCreate(macro_worker_task, "macro_worker", 4096, nullptr, 1, &s_macro_task) != pdPASS) {
            s_macro_task = nullptr;
            return false;
        }
    }
    return true;
}

extern "C" bool macro_trigger_slot(uint8_t slot) {
    if (!macro_ensure_worker()) return false;
    return xQueueSend(s_macro_queue, &slot, 0) == pdTRUE;
}
