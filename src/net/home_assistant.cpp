#include "home_assistant.h"
#include "log/app_log.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

static String s_entity_lists[5 * 16 * 3];
static portMUX_TYPE s_init_mux = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t s_ha_mutex = nullptr;
static QueueHandle_t s_ha_queue = nullptr;
static TaskHandle_t s_ha_task = nullptr;

static void ha_ensure_mutex() {
    if (s_ha_mutex) return;
    portENTER_CRITICAL(&s_init_mux);
    if (!s_ha_mutex) {
        s_ha_mutex = xSemaphoreCreateMutex();
    }
    portEXIT_CRITICAL(&s_init_mux);
}

static void ha_lock() {
    ha_ensure_mutex();
    if (s_ha_mutex) xSemaphoreTake(s_ha_mutex, portMAX_DELAY);
}

static void ha_unlock() {
    if (s_ha_mutex) xSemaphoreGive(s_ha_mutex);
}

void ha_clear_entity_lists() {
    ha_lock();
    for (String& list : s_entity_lists) list = "";
    ha_unlock();
}

bool ha_set_entity_list(uint8_t slot, const String& entities) {
    if (slot >= (sizeof(s_entity_lists) / sizeof(s_entity_lists[0]))) return false;
    ha_lock();
    s_entity_lists[slot] = entities;
    ha_unlock();
    return true;
}

String ha_get_entity_list(uint8_t slot) {
    if (slot >= (sizeof(s_entity_lists) / sizeof(s_entity_lists[0]))) return "";
    ha_lock();
    String copy = s_entity_lists[slot];
    ha_unlock();
    return copy;
}

static String base_url_locked() {
    Preferences p;
    p.begin("ha", true);
    String s = p.getString("url", "");
    p.end();
    s.trim();
    while (s.endsWith("/")) s.remove(s.length() - 1);
    return s;
}

static String auth_token_locked() {
    Preferences p;
    p.begin("ha", true);
    String s = p.getString("token", "");
    p.end();
    return s;
}

static bool request(const String& path, const String& body, String* response = nullptr) {
    ha_lock();
    String url = base_url_locked();
    String token = auth_token_locked();
    ha_unlock();

    if (!url.length() || !token.length() || WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    http.setTimeout(2500);
    bool begun = false;
    if (url.startsWith("https://")) {
        WiFiClientSecure secure;
        secure.setInsecure();
        begun = http.begin(secure, url + path);
        if (!begun) return false;
        http.addHeader("Authorization", "Bearer " + token);
        http.addHeader("Content-Type", "application/json");
        int code = body.length() ? http.POST(body) : http.GET();
        if (response) *response = http.getString();
        http.end();
        return code >= 200 && code < 300;
    }

    WiFiClient client;
    begun = http.begin(client, url + path);
    if (!begun) return false;
    http.addHeader("Authorization", "Bearer " + token);
    http.addHeader("Content-Type", "application/json");
    int code = body.length() ? http.POST(body) : http.GET();
    if (response) *response = http.getString();
    http.end();
    return code >= 200 && code < 300;
}

static bool ha_execute_slot(uint8_t slot) {
    String entity_list = ha_get_entity_list(slot);
    if (!entity_list.length()) return false;

    JsonDocument d;
    JsonArray lights = d["lights"].to<JsonArray>();
    JsonArray buttons = d["buttons"].to<JsonArray>();
    String remaining = entity_list;
    while (remaining.length()) {
        int sep = remaining.indexOf(';');
        String item = sep < 0 ? remaining : remaining.substring(0, sep);
        item.trim();
        if (item.startsWith("light.")) lights.add(item);
        else if (item.startsWith("button.")) buttons.add(item);
        else return false;
        if (sep < 0) break;
        remaining = remaining.substring(sep + 1);
    }

    bool ok = true;
    if (!lights.isNull() && lights.size()) {
        JsonDocument payload;
        JsonArray target = payload["entity_id"].to<JsonArray>();
        for (JsonVariant entity : lights) target.add(entity.as<String>());
        String body;
        serializeJson(payload, body);
        ok = request("/api/services/light/toggle", body) && ok;
    }
    if (!buttons.isNull() && buttons.size()) {
        JsonDocument payload;
        JsonArray target = payload["entity_id"].to<JsonArray>();
        for (JsonVariant entity : buttons) target.add(entity.as<String>());
        String body;
        serializeJson(payload, body);
        ok = request("/api/services/button/press", body) && ok;
    }
    return ok;
}

static void ha_worker_task(void* param) {
    (void)param;
    uint8_t slot = 0;
    while (true) {
        if (xQueueReceive(s_ha_queue, &slot, portMAX_DELAY) == pdTRUE) {
            if (!ha_execute_slot(slot)) {
                app_log("HA", "Toggle failed for mapping slot %u", slot);
            }
        }
    }
}

static bool ha_ensure_worker() {
    ha_ensure_mutex();
    if (!s_ha_queue) {
        s_ha_queue = xQueueCreate(8, sizeof(uint8_t));
        if (!s_ha_queue) return false;
    }
    if (!s_ha_task) {
        if (xTaskCreate(ha_worker_task, "ha_worker", 8192, nullptr, 1, &s_ha_task) != pdPASS) {
            s_ha_task = nullptr;
            return false;
        }
    }
    return true;
}

bool ha_configure(const String& url, const String& token) {
    String u = url;
    u.trim();
    String t = token;
    t.trim();
    if (!(u.startsWith("http://") || u.startsWith("https://"))) return false;

    ha_lock();
    if (!t.length()) {
        t = auth_token_locked();
    }
    if (!t.length()) {
        ha_unlock();
        return false;
    }
    Preferences p;
    p.begin("ha", false);
    p.putString("url", u);
    p.putString("token", t);
    p.end();
    ha_unlock();
    return true;
}

String ha_get_config_json() {
    ha_lock();
    String u = base_url_locked();
    String t = auth_token_locked();
    ha_unlock();

    JsonDocument d;
    d["configured"] = u.length() > 0 && t.length() > 0;
    d["url"] = u;
    String o;
    serializeJson(d, o);
    return o;
}

bool ha_test_connection(String* message) {
    String r;
    bool ok = request("/api/", "", &r);
    if (message) *message = ok ? "HA 连接成功" : "连接失败：请检查地址、长期令牌及设备网络";
    return ok;
}

extern "C" bool ha_toggle_slot(uint8_t slot) {
    if (!ha_ensure_worker()) return false;
    return xQueueSend(s_ha_queue, &slot, 0) == pdTRUE;
}
