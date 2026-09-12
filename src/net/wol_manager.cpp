#include "wol_manager.h"
#include "log/app_log.h"
#include "wifi/wifi_manager.h"
#include "led_indicator.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

extern "C" {

bool wol_manager_parse_mac(const char* mac_str, uint8_t out_mac[6]) {
    if (!mac_str || !out_mac) return false;
    unsigned int m[6];
    if (sscanf(mac_str, "%02x:%02x:%02x:%02x:%02x:%02x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6 ||
        sscanf(mac_str, "%02x-%02x-%02x-%02x-%02x-%02x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
        for (int i = 0; i < 6; i++) out_mac[i] = (uint8_t)m[i];
        return true;
    }
    if (strlen(mac_str) == 12) {
        if (sscanf(mac_str, "%02x%02x%02x%02x%02x%02x", &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) == 6) {
            for (int i = 0; i < 6; i++) out_mac[i] = (uint8_t)m[i];
            return true;
        }
    }
    return false;
}

void wol_manager_mac_to_str(const uint8_t mac[6], char* out_str, size_t out_size) {
    if (!mac || !out_str || out_size < 18) return;
    snprintf(out_str, out_size, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

bool wol_manager_send(const uint8_t mac[6], uint16_t port) {
    if (!mac) return false;
    if (port == 0) port = 9;

    bool all_zero = true;
    for (int i = 0; i < 6; i++) {
        if (mac[i] != 0) {
            all_zero = false;
            break;
        }
    }
    if (all_zero) {
        app_log("WOL", "Error: target MAC address is 00:00:00:00:00:00");
        return false;
    }

    char mac_buf[20];
    wol_manager_mac_to_str(mac, mac_buf, sizeof(mac_buf));

    uint8_t magic_packet[102];
    memset(magic_packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(magic_packet + 6 + (i * 6), mac, 6);
    }

    WiFiUDP udp;
    IPAddress bcast(255, 255, 255, 255);
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress localBcast = WiFi.broadcastIP();
        if (localBcast[0] != 0) {
            bcast = localBcast;
        }
    }

    udp.begin(0);
    udp.beginPacket(bcast, port);
    size_t written = udp.write(magic_packet, sizeof(magic_packet));
    bool ok = udp.endPacket();
    udp.stop();

    if (ok && written == sizeof(magic_packet)) {
        app_log("WOL", "Magic packet sent to %s via %s:%u",
                mac_buf, bcast.toString().c_str(), port);
        led_indicator_trigger_key(false);
        return true;
    } else {
        app_log("WOL", "Failed to send magic packet to %s (written=%u, ok=%d)",
                mac_buf, (unsigned int)written, ok ? 1 : 0);
        return false;
    }
}

bool wol_manager_send_str(const char* mac_str, uint16_t port) {
    if (!mac_str) return false;
    uint8_t mac[6];
    if (!wol_manager_parse_mac(mac_str, mac)) {
        app_log("WOL", "Invalid MAC format: %s", mac_str);
        return false;
    }
    return wol_manager_send(mac, port);
}

} // extern "C"
