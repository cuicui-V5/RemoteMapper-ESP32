#include "home_assistant.h"
#include <Preferences.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static String s_entity_lists[5 * 16 * 3];

void ha_clear_entity_lists() { for (String& list : s_entity_lists) list = ""; }
bool ha_set_entity_list(uint8_t slot, const String& entities) {
    if (slot >= (sizeof(s_entity_lists) / sizeof(s_entity_lists[0]))) return false;
    s_entity_lists[slot] = entities;
    return true;
}
String ha_get_entity_list(uint8_t slot) {
    if (slot >= (sizeof(s_entity_lists) / sizeof(s_entity_lists[0]))) return "";
    return s_entity_lists[slot];
}

static String base_url() { Preferences p; p.begin("ha", true); String s=p.getString("url", ""); p.end(); s.trim(); while(s.endsWith("/")) s.remove(s.length()-1); return s; }
static String auth_token() { Preferences p; p.begin("ha", true); String s=p.getString("token", ""); p.end(); return s; }
static bool request(const String& path, const String& body, String* response=nullptr) {
    String url=base_url(), token=auth_token(); if(!url.length() || !token.length() || WiFi.status()!=WL_CONNECTED) return false;
    HTTPClient http; http.setTimeout(2500);
    bool begun=false;
    if(url.startsWith("https://")) { WiFiClientSecure secure; secure.setInsecure(); begun=http.begin(secure,url+path); if(!begun) return false; http.addHeader("Authorization", "Bearer "+token); http.addHeader("Content-Type", "application/json"); int code=body.length()?http.POST(body):http.GET(); if(response) *response=http.getString(); http.end(); return code>=200&&code<300; }
    WiFiClient client; begun=http.begin(client,url+path); if(!begun) return false;
    http.addHeader("Authorization", "Bearer "+token); http.addHeader("Content-Type", "application/json");
    int code=body.length()?http.POST(body):http.GET(); if(response) *response=http.getString(); http.end(); return code>=200 && code<300;
}
bool ha_configure(const String& url, const String& token) {
    String u=url; u.trim(); String t=token; t.trim();
    if(!(u.startsWith("http://")||u.startsWith("https://")) || !t.length()) return false;
    Preferences p; p.begin("ha", false); p.putString("url",u); p.putString("token",t); p.end(); return true;
}
String ha_get_config_json() { JsonDocument d; String u=base_url(); d["configured"]=u.length()>0 && auth_token().length()>0; d["url"]=u; String o; serializeJson(d,o); return o; }
bool ha_test_connection(String* message) { String r; bool ok=request("/api/", "", &r); if(message) *message=ok?"HA 连接成功":"连接失败：请检查地址、长期令牌及设备网络"; return ok; }
extern "C" bool ha_toggle_slot(uint8_t slot) {
    String entity_list=ha_get_entity_list(slot);
    if(!entity_list.length()) return false;
    JsonDocument d;
    JsonArray lights=d["lights"].to<JsonArray>();
    JsonArray buttons=d["buttons"].to<JsonArray>();
    String remaining=entity_list;
    while(remaining.length()) {
        int sep=remaining.indexOf(';');
        String item=sep<0?remaining:remaining.substring(0,sep); item.trim();
        if(item.startsWith("light.")) lights.add(item);
        else if(item.startsWith("button.")) buttons.add(item);
        else return false;
        if(sep<0) break;
        remaining=remaining.substring(sep+1);
    }
    bool ok=true;
    if(!lights.isNull() && lights.size()) {
        JsonDocument payload; JsonArray target=payload["entity_id"].to<JsonArray>();
        for(JsonVariant entity : lights) target.add(entity.as<String>());
        String body; serializeJson(payload,body);
        ok=request("/api/services/light/toggle",body) && ok;
    }
    if(!buttons.isNull() && buttons.size()) {
        JsonDocument payload; JsonArray target=payload["entity_id"].to<JsonArray>();
        for(JsonVariant entity : buttons) target.add(entity.as<String>());
        String body; serializeJson(payload,body);
        ok=request("/api/services/button/press",body) && ok;
    }
    return ok;
}
