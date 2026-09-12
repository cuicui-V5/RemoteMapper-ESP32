#include "cli_manager.h"
#include "version.h"
#include "wifi/wifi_manager.h"
#include "log/app_log.h"
#include "ble/ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "keymap/key_state_machine.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <USBCDC.h>

#if !ARDUINO_USB_CDC_ON_BOOT
extern USBCDC USBSerial;
#endif

extern key_mapper_engine_t g_key_engine;

static String s_input_buffer = "";

static void handle_command(const String& line);

static void cli_write_line(const String& line) {
    Serial.println(line);
#if !ARDUINO_USB_CDC_ON_BOOT
    if (USBSerial) {
        USBSerial.println(line);
    }
#endif
}

static void cli_feed_char(char c) {
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

static void handle_wifi_command(const String& arg) {
    bool want_on = arg.equalsIgnoreCase("on");
    bool want_off = arg.equalsIgnoreCase("off");
    if (want_on || want_off) {
        if (wifi_manager_get_enabled() == want_on) {
            cli_write_line(want_on
                ? "{\"status\":\"nochange\",\"wifi_enabled\":true}"
                : "{\"status\":\"nochange\",\"wifi_enabled\":false}");
            return;
        }
        wifi_manager_set_enabled(want_on);
        cli_write_line(want_on
            ? "{\"status\":\"ok\",\"wifi_enabled\":true,\"rebooting\":true}"
            : "{\"status\":\"ok\",\"wifi_enabled\":false,\"rebooting\":true}");
        cli_write_line(want_on
            ? "Wi-Fi 将在重启后开启（AP 热点 RemoteMapper-AP 恢复）..."
            : "Wi-Fi 将在重启后关闭（后台改用 USB CDC/UART 的 'wifi on' 恢复）...");
        delay(300);
        ESP.restart();
    } else {
        JsonDocument doc;
        doc["wifi_enabled"] = wifi_manager_get_enabled();
        doc["ap_running"] = wifi_manager_is_ap_running();
        doc["ap_ip"] = wifi_manager_get_ap_ip();
        doc["sta_connected"] = wifi_manager_is_sta_connected();
        doc["sta_ip"] = wifi_manager_get_sta_ip();
        String out;
        serializeJson(doc, out);
        cli_write_line(out);
    }
}

static void handle_log_command(const String& arg) {
    if (arg.equalsIgnoreCase("on")) {
        app_log_set_cdc_enabled(true);
        cli_write_line("{\"status\":\"ok\",\"cdc_log_enabled\":true}");
        cli_write_line("CDC 日志镜像已开启（注意：语音会话期间日志较密集）");
    } else if (arg.equalsIgnoreCase("off")) {
        app_log_set_cdc_enabled(false);
        cli_write_line("{\"status\":\"ok\",\"cdc_log_enabled\":false}");
        cli_write_line("CDC 日志镜像已关闭（UART 日志不受影响）");
    } else {
        JsonDocument doc;
        doc["cdc_log_enabled"] = app_log_get_cdc_enabled();
        String out;
        serializeJson(doc, out);
        cli_write_line(out);
    }
}

static void handle_command(const String& line) {
    String cmd = line;
    cmd.trim();
    if (cmd.length() == 0) return;

    // Split into "head [tail]" so wifi on/off/status can share one dispatcher
    int sp = cmd.indexOf(' ');
    String head = (sp < 0) ? cmd : cmd.substring(0, sp);
    String tail = (sp < 0) ? String("") : cmd.substring(sp + 1);
    tail.trim();

    if (head.equalsIgnoreCase("wifi")) {
        handle_wifi_command(tail);
        return;
    }

    if (head.equalsIgnoreCase("log")) {
        handle_log_command(tail);
        return;
    }

    if (cmd.equalsIgnoreCase("status") || cmd.equalsIgnoreCase("info")) {
        JsonDocument doc;
        doc["firmware"] = FIRMWARE_NAME;
        doc["version"] = FIRMWARE_VERSION;
        doc["target"] = HARDWARE_TARGET;
        doc["uptime_sec"] = millis() / 1000;
        doc["ble_state"] = (int)ble_remote_get_state();
        doc["frames_decoded"] = g_audio_pipeline.total_frames_decoded;
        doc["samples_pushed"] = g_audio_pipeline.total_samples_pushed;
        doc["free_heap"] = ESP.getFreeHeap();
        doc["free_psram"] = ESP.getFreePsram();
        doc["wifi_enabled"] = wifi_manager_get_enabled();

        String out;
        serializeJson(doc, out);
        cli_write_line(out);
    }
    else if (cmd.equalsIgnoreCase("reconnect")) {
        cli_write_line("{\"status\":\"reconnecting\"}");
        ble_remote_trigger_reconnect();
    }
    else if (cmd.equalsIgnoreCase("reset_keys")) {
        key_engine_load_defaults(&g_key_engine);
        cli_write_line("{\"status\":\"keymap_reset_to_defaults\"}");
    }
    else if (cmd.equalsIgnoreCase("help")) {
        cli_write_line("Commands:");
        cli_write_line("  status        - Display system info & runtime statistics (JSON)");
        cli_write_line("  reconnect     - Trigger BLE remote re-scan");
        cli_write_line("  reset_keys    - Reset key bindings to factory defaults");
        cli_write_line("  wifi on|off   - Enable/disable the whole Wi-Fi radio (persisted)");
        cli_write_line("  wifi status   - Show Wi-Fi radio & connection status (JSON)");
        cli_write_line("  log on|off    - Mirror full logs to USB CDC (default: off)");
        cli_write_line("  log status    - Show CDC log mirror state (JSON)");
        cli_write_line("  help          - Show available commands");
    }
    else {
        cli_write_line("{\"error\":\"unknown_command\",\"hint\":\"type help\"}");
    }
}

extern "C" {

void cli_manager_init(void) {
    s_input_buffer.reserve(256);
}

void cli_manager_task(void) {
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        cli_feed_char(c);
    }
#if !ARDUINO_USB_CDC_ON_BOOT
    if (USBSerial) {
        while (USBSerial.available() > 0) {
            char c = (char)USBSerial.read();
            cli_feed_char(c);
        }
    }
#endif
}

} // extern "C"
