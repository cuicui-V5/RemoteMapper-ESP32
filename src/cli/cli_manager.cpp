#include "cli_manager.h"
#include "version.h"
#include "ble/ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include <Arduino.h>
#include <ArduinoJson.h>

extern key_mapper_engine_t g_key_engine;

static String s_input_buffer = "";

static void handle_command(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    if (cmd.equalsIgnoreCase("status") || cmd.equalsIgnoreCase("info")) {
        JsonDocument doc;
        doc["firmware"] = FIRMWARE_NAME;
        doc["version"] = FIRMWARE_VERSION;
        doc["target"] = HARDWARE_TARGET;
        doc["uptime_sec"] = millis() / 1000;
        doc["ble_state"] = (int)ble_remote_get_state();
        doc["voice_ready"] = ble_remote_voice_ready();
        doc["frames_decoded"] = g_audio_pipeline.total_frames_decoded;
        doc["samples_pushed"] = g_audio_pipeline.total_samples_pushed;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["free_psram"] = ESP.getFreePsram();

        String out;
        serializeJson(doc, out);
        Serial.println(out);
    }
    else if (cmd.equalsIgnoreCase("reconnect")) {
        Serial.println("{\"status\":\"reconnecting\"}");
        ble_remote_trigger_reconnect();
    }
    else if (cmd.equalsIgnoreCase("reset_keys")) {
        key_engine_load_defaults(&g_key_engine);
        Serial.println("{\"status\":\"keymap_reset_to_defaults\"}");
    }
    else if (cmd.equalsIgnoreCase("help")) {
        Serial.println("Commands:");
        Serial.println("  status      - Display system info & runtime statistics (JSON)");
        Serial.println("  reconnect   - Trigger BLE remote re-scan");
        Serial.println("  reset_keys  - Reset key bindings to factory defaults");
        Serial.println("  help        - Show available commands");
    }
    else {
        Serial.println("{\"error\":\"unknown_command\",\"hint\":\"type help\"}");
    }
}

extern "C" {

void cli_manager_init(void) {
    s_input_buffer.reserve(256);
}

void cli_manager_task(void) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r' || c == '\n') {
            if (s_input_buffer.length() > 0) {
                handle_command(s_input_buffer);
                s_input_buffer = "";
            }
        } else {
            s_input_buffer += c;
            if (s_input_buffer.length() > 250) {
                s_input_buffer = "";
            }
        }
    }
}

} // extern "C"
