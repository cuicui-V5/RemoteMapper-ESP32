#include "ble_remote_client.h"
#include "audio/audio_pipeline.h"
#include "led_indicator.h"
#include "keymap/key_state_machine.h"
#include "usb/usb_composite.h"
#include "log/app_log.h"
#include <Arduino.h>
#include <NimBLEDevice.h>
#include <Preferences.h>
#include <ArduinoJson.h>
#include <vector>

static ble_remote_state_t              s_ble_state = BLE_STATE_DISCONNECTED;
static NimBLEClient*                   s_client = nullptr;
static NimBLERemoteCharacteristic*     s_char_cmd = nullptr;
static NimBLERemoteCharacteristic*     s_char_aud = nullptr;
static NimBLERemoteCharacteristic*     s_char_ctl = nullptr;

static Preferences                     s_ble_prefs;
static String                          s_bound_mac = "";
static String                          s_bound_name = "";
static uint8_t                         s_bound_addr_type = BLE_ADDR_RANDOM;
static String                          s_connected_name = "";
static String                          s_connected_mac = "";

// In-memory discovered BLE device cache (thread-safe, fed by Core 0 continuous scan)
struct DiscoveredBleDevice {
    String   name;
    String   mac;
    int      rssi;
    uint8_t  type;
    uint32_t last_seen_ms;
};
static std::vector<DiscoveredBleDevice> s_discovered_devices;
static portMUX_TYPE                    s_disc_mux = portMUX_INITIALIZER_UNLOCKED;

// Asynchronous Request flags from other tasks (e.g. WebServer on Core 1)
static volatile bool                   s_req_unpair = false;
static volatile bool                   s_req_reconnect = false;
static volatile bool                   s_do_connect = false;
static NimBLEAdvertisedDevice*         s_pending_adv_device = nullptr;
static String                          s_pending_mac = "";
static uint8_t                         s_pending_addr_type = BLE_ADDR_RANDOM;

static bool                           s_voice_ready = false;
static uint16_t                       s_atvv_version = 0;
// Defer GATT writes until the BLE task, outside notification callbacks.
static volatile int                   s_mic_request = 0;
static uint8_t                        s_session_id = 0;
static uint32_t                        s_last_audio_ms = 0;
static uint32_t                        s_last_extend_ms = 0;
static uint32_t                        s_last_scan_ms = 0;
static uint32_t                        s_last_keepalive_ms = 0;
static size_t                          s_frame_size = AUDIO_DEFAULT_FRAME_BYTES;

extern key_mapper_engine_t g_key_engine;

// Forward Declarations
static void start_scan();
static bool do_connect_adv_device(NimBLEAdvertisedDevice* advDevice);
static bool do_connect_mac(const String& mac_str, uint8_t addr_type);
static bool setup_services_and_handshake();

// Audio Notification Callback (ATVV Char 0x03)
static void on_audio_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length == 0 || !s_voice_ready || length > AUDIO_DEFAULT_FRAME_SAMPS) return;
    s_last_audio_ms = millis();
    audio_pipeline_feed_adpcm(&g_audio_pipeline, pData, length);
}

// Control Notification Callback (ATVV Char 0x04)
static void on_ctl_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;
    uint8_t op = pData[0];

    // AUDIO_START confirms the remote opened the negotiated stream.
    if (op == 0x04 && length >= 2) {
        s_session_id = (length >= 4) ? pData[3] : 0;
        s_ble_state = BLE_STATE_TALKING;
        s_last_audio_ms = millis();
        s_last_extend_ms = millis();

        audio_pipeline_start_session(&g_audio_pipeline, s_session_id);
        app_log("ATVV", ">>> Voice button PRESSED (session %d)", s_session_id);
    }
    // MIC_BUTTON requests microphone activation; it is not AUDIO_STOP.
    else if (op == 0x08) {
        if (s_voice_ready) s_mic_request = 1;
    }
    else if (op == 0x00) {
        audio_pipeline_stop_session(&g_audio_pipeline);
        if (s_ble_state == BLE_STATE_TALKING) {
            s_ble_state = BLE_STATE_CONNECTED;
            key_engine_feed_key(&g_key_engine, MI_KEY_VOICE, false, millis());
            app_log("ATVV", "<<< Voice button RELEASED (op=0x%02X)", op);
        } else {
            app_log("ATVV", "Microphone inactive / standby (op=0x%02X)", op);
        }
        // NOTE: Do NOT send cmd_open (MIC_OPEN) here. Keep microphone off so the remote can sleep.
    }
    // CAPS_RESP: op == 0x0B
    else if (op == 0x0B && length >= 7) {
        uint16_t ver = (pData[1] << 8) | pData[2];
        s_atvv_version = ver;
        uint8_t codecs = ver >= 0x0100 ? pData[3] : (length >= 9 ? pData[4] : 0);
        if (codecs == 0 && length >= 9) codecs = pData[4];
        s_voice_ready = (codecs & 0x02) != 0;
        app_log("ATVV", "Capabilities accepted: voice_ready=%d", s_voice_ready);
        uint16_t fs = (pData[5] << 8) | pData[6];
        if (fs > 0) s_frame_size = fs;
        app_log("ATVV", "CAPS: ver=0x%04X, frame_size=%d", ver, (int)s_frame_size);
    }
    // AUDIO_SYNC: op == 0x0A
    else if (op == 0x0A && length >= 7) {
        int16_t pred = (int16_t)((pData[4] << 8) | pData[5]);
        int8_t step_idx = (int8_t)pData[6];
        audio_pipeline_sync(&g_audio_pipeline, pred, step_idx);
        app_log("ATVV", "SYNC: pred=%d, step=%d", pred, step_idx);
    }
}

static uint8_t s_last_hogp_key = 0;

// HOGP HID Report Notification Callback
static void on_hogp_report_notify(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 1) return;

    // HID payloads are not ATVV ADPCM. Never feed them to that decoder.
    if (length > 8) {
        static uint32_t ignored = 0;
        if ((ignored++ % 128) == 0)
            app_log("HID_AUDIO", "Ignoring non-ATVV report handle=%u len=%u", pChar->getHandle(), (unsigned)length);
        return;
    }

    // Dump raw bytes for diagnostics (only for genuine key reports len <= 8)
    String hex_str = "";
    for (size_t i = 0; i < length; i++) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%02X ", pData[i]);
        hex_str += buf;
    }
    app_log("HOGP_RAW", "Report (len %d): %s", (int)length, hex_str.c_str());

    // 2. Standard Keyboard & Consumer Reports (len <= 8)
    uint8_t raw_key = 0;
    bool is_pressed = false;

    if (length == 8) {
        // Standard 8-byte Keyboard Report: [modifiers, reserved, key0..key5]
        if (pData[2] != 0) {
            raw_key = pData[2];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length == 2) {
        if (pData[1] != 0) {
            raw_key = pData[1];
            is_pressed = true;
        } else if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length == 1) {
        if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    } else if (length >= 3 && length <= 7) {
        if (pData[2] != 0) {
            raw_key = pData[2];
            is_pressed = true;
        } else if (pData[0] != 0) {
            raw_key = pData[0];
            is_pressed = true;
        } else {
            raw_key = s_last_hogp_key;
            is_pressed = false;
        }
    }

    // Auto-release previous key if a new key is pressed without an explicit all-zero release report
    if (s_last_hogp_key != 0 && is_pressed && raw_key != s_last_hogp_key) {
        app_log("HOGP", "Auto-Release key 0x%02X due to new key 0x%02X", s_last_hogp_key, raw_key);
        key_engine_feed_key(&g_key_engine, s_last_hogp_key, false, millis());
        s_last_hogp_key = 0;
    }

    if (is_pressed) {
        s_last_hogp_key = raw_key;
    } else {
        s_last_hogp_key = 0;
    }

    if (raw_key != 0) {
        app_log("HOGP", "Key event: 0x%02X (%s)", raw_key, is_pressed ? "DOWN" : "UP");
        if (raw_key == MI_KEY_VOICE || raw_key == MI_KEY_VOICE_ALT) {
            app_log("VOICE", "Voice button event: 0x%02X (%s)", raw_key, is_pressed ? "DOWN" : "UP");
            if (s_voice_ready) s_mic_request = is_pressed ? 1 : -1;
        }
        key_engine_feed_key(&g_key_engine, raw_key, is_pressed, millis());
    }
}

static bool is_target_remote(NimBLEAdvertisedDevice* dev) {
    String name = dev->getName().c_str();
    String addr = dev->getAddress().toString().c_str();
    addr.toLowerCase();

    // 1. If a remote was previously bound: STRICT EXCLUSIVITY!
    // ONLY reconnect to this specific bound remote! NEVER connect to stranger devices!
    if (s_bound_mac.length() > 0) {
        String bound = s_bound_mac;
        bound.toLowerCase();
        if (addr.equals(bound)) {
            return true;
        }
        // If MAC rotated (e.g. RPA) but device name matches our bound remote:
        if (s_bound_name.length() > 0 && name.length() > 0 && name.equalsIgnoreCase(s_bound_name)) {
            app_log("BLE", "Bound remote name matched (%s), updating target MAC to %s", name.c_str(), addr.c_str());
            return true;
        }
        // STRICT: If bound, NEVER match any other stranger devices!
        return false;
    }

    // 2. Unbound state (initial pairing only):
    // Name MUST contain explicit Xiaomi / Remote keywords (dropped loose "RC" to avoid false positives)
    if (name.indexOf("小米") >= 0 || name.indexOf("遥控") >= 0 ||
        name.indexOf("MI RC") >= 0 || name.indexOf("Xiaomi") >= 0 ||
        name.indexOf("Remote") >= 0) {
        return true;
    }

    // 3. Service UUID matches proprietary ATVV (0xab5e0001)
    // (Note: Generic 0x1812 HID is explicitly removed to prevent hijacking neighbor devices!)
    if (dev->haveServiceUUID()) {
        if (dev->isAdvertisingService(NimBLEUUID(ATVV_SVC_UUID))) {
            return true;
        }
    }

    // 4. Common Xiaomi Bluetooth OUI prefixes
    if (addr.startsWith("c0:5d:39") || addr.startsWith("64:90:c1") ||
        addr.startsWith("7c:49:eb") || addr.startsWith("50:ec:50") ||
        addr.startsWith("04:cf:8c") || addr.startsWith("28:6c:07") ||
        addr.startsWith("34:ce:00") || addr.startsWith("5c:c3:06")) {
        return true;
    }

    return false;
}

// Advertised Device Scan Callbacks
class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override {
        String name = advertisedDevice->getName().c_str();
        String addr = advertisedDevice->getAddress().toString().c_str();
        uint32_t now = millis();

        // Update in-memory discovered BLE devices cache for WebUI (thread-safe, non-blocking)
        portENTER_CRITICAL(&s_disc_mux);
        bool found = false;
        for (auto& item : s_discovered_devices) {
            if (item.mac.equalsIgnoreCase(addr)) {
                if (name.length() > 0) item.name = name;
                item.rssi = advertisedDevice->getRSSI();
                item.type = (uint8_t)advertisedDevice->getAddress().getType();
                item.last_seen_ms = now;
                found = true;
                break;
            }
        }
        if (!found) {
            if (s_discovered_devices.size() >= 30) {
                s_discovered_devices.erase(s_discovered_devices.begin());
            }
            DiscoveredBleDevice d;
            d.name = (name.length() > 0) ? name : "Unnamed BLE Device";
            d.mac = addr;
            d.rssi = advertisedDevice->getRSSI();
            d.type = (uint8_t)advertisedDevice->getAddress().getType();
            d.last_seen_ms = now;
            s_discovered_devices.push_back(d);
        }

        // Purge devices not seen for over 20 seconds
        for (auto it = s_discovered_devices.begin(); it != s_discovered_devices.end(); ) {
            if (now - it->last_seen_ms > 20000) {
                it = s_discovered_devices.erase(it);
            } else {
                ++it;
            }
        }
        portEXIT_CRITICAL(&s_disc_mux);

        if (name.length() > 0) {
            app_log("BLE_SCAN", "Device: %s (%s, RSSI: %d, Type: %d)", 
                    name.c_str(), addr.c_str(), advertisedDevice->getRSSI(), 
                    (int)advertisedDevice->getAddress().getType());
        }

        if (s_ble_state <= BLE_STATE_SCANNING && is_target_remote(advertisedDevice) && !s_do_connect) {
            app_log("BLE", "Matching Target Remote: %s (%s), queueing connection...", name.c_str(), addr.c_str());
            NimBLEDevice::getScan()->stop();
            if (s_pending_adv_device) delete s_pending_adv_device;
            s_pending_adv_device = new NimBLEAdvertisedDevice(*advertisedDevice);
            s_do_connect = true;
        }
    }
};

// Client Connection Callbacks
class ClientCallbacks : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) override {
        app_log("BLE", "Remote GATT Connected!");
        s_ble_state = BLE_STATE_CONNECTING;
        // NOTE: Do NOT call secureConnection() here. It blocks the NimBLE host task thread.
    }

    void onDisconnect(NimBLEClient* pClient) override {
        int err = pClient ? pClient->getLastError() : 0;
        app_log("BLE", "Remote Disconnected (lastErr: %d, %s)", err, NimBLEUtils::returnCodeToString(err));
        s_ble_state = BLE_STATE_DISCONNECTED;
        s_do_connect = false;
        s_voice_ready = false;
        s_mic_request = 0;
        s_char_cmd = nullptr;
        s_char_aud = nullptr;
        s_char_ctl = nullptr;
        s_last_hogp_key = 0;
        key_engine_release_all(&g_key_engine, millis());
        usb_hid_keyboard_release();
        usb_hid_consumer_release();
        audio_pipeline_stop_session(&g_audio_pipeline);
        led_indicator_set(LED_STATE_WAIT_CONNECTION);
        // NOTE: Do NOT call start_scan() directly inside GAP disconnect callback.
        // ble_remote_task() on Core 0 automatically restarts scanning on the next tick.
    }

    bool onConnParamsUpdateRequest(NimBLEClient* pClient, const ble_gap_upd_params* params) override {
        return true; // Accept remote requested conn params
    }

    void onAuthenticationComplete(ble_gap_conn_desc* desc) override {
        if (desc->sec_state.encrypted) {
            app_log("BLE_SEC", "Link Encrypted & Bonded! (Bonded:%d)", desc->sec_state.bonded);
        } else {
            app_log("BLE_SEC", "Encryption not established");
        }
    }
};

static void start_scan() {
    if (s_do_connect || s_ble_state == BLE_STATE_CONNECTING || s_ble_state >= BLE_STATE_CONNECTED) {
        return;
    }
    s_ble_state = BLE_STATE_SCANNING;
    led_indicator_set(LED_STATE_WAIT_CONNECTION);
    s_last_scan_ms = millis();
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(BLE_SCAN_INTERVAL_MS);
    pScan->setWindow(BLE_SCAN_WINDOW_MS);
    pScan->start(0, false); // 0 = continuous scan until stopped
    app_log("BLE", "Continuous scanning for Xiaomi Bluetooth Remote active...");
}

static bool setup_services_and_handshake() {
    if (!s_client || !s_client->isConnected()) return false;

    // 1. Security & Bonding (executed safely on Core 0 task context)
    app_log("BLE_SEC", "Initiating secure connection / bonding...");
    if (!s_client->secureConnection()) {
        app_log("BLE_SEC", "secureConnection returned %d, continuing service discovery...", s_client->getLastError());
    } else {
        app_log("BLE_SEC", "secureConnection established successfully");
    }
    vTaskDelay(pdMS_TO_TICKS(50));

    s_voice_ready = false;
    s_atvv_version = 0;
    s_char_cmd = s_char_ctl = s_char_aud = nullptr;
    auto* voice = s_client->getService(NimBLEUUID(ATVV_SVC_UUID));
    if (voice) {
        s_char_cmd = voice->getCharacteristic(NimBLEUUID(ATVV_CHAR_CMD_UUID));
        s_char_ctl = voice->getCharacteristic(NimBLEUUID(ATVV_CHAR_CTL_UUID));
        s_char_aud = voice->getCharacteristic(NimBLEUUID(ATVV_CHAR_AUD_UUID));
    }
    bool subscriptions = false;
    if (s_char_cmd && s_char_ctl && s_char_aud) {
        bool ctl = s_char_ctl->subscribe(true, on_ctl_notify, true);
        bool aud = ctl && s_char_aud->subscribe(true, on_audio_notify, true);
        subscriptions = ctl && aud;
        app_log("ATVV", "Subscriptions: control=%d audio=%d", ctl, aud);
        if (subscriptions) {
            uint8_t caps[] = {0x0A, 0x01, 0x00, 0x00, 0x03, 0x03};
            bool sent = s_char_cmd->writeValue(caps, sizeof(caps), true);
            app_log("ATVV", "Capability query sent=%d", sent);
            for (int i = 0; sent && !s_voice_ready && s_client->isConnected() && i < 40; ++i)
                vTaskDelay(pdMS_TO_TICKS(50));
        }
    } else {
        app_log("ATVV", "Missing characteristic: cmd=%d ctl=%d aud=%d",
                s_char_cmd != nullptr, s_char_ctl != nullptr, s_char_aud != nullptr);
    }

    // Subscribe only input reports, after the private voice service is ready.
    if (s_client->isConnected()) {
        auto* hid = s_client->getService(NimBLEUUID((uint16_t)0x1812));
        if (hid) {
            auto* chars = hid->getCharacteristics(true);
            if (chars) for (auto* chr : *chars) {
                if (chr->getUUID().equals(NimBLEUUID((uint16_t)0x2a4d)) && chr->canNotify()) {
                    bool ok = chr->subscribe(true, on_hogp_report_notify, true);
                    app_log("HOGP", "Input handle=%u subscribed=%d", chr->getHandle(), ok);
                }
            }
        }
    }
    if (!s_client->isConnected()) {
        s_voice_ready = false;
        s_ble_state = BLE_STATE_DISCONNECTED;
        app_log("BLE", "Setup interrupted by disconnect; will rescan");
        return false;
    }
    s_ble_state = BLE_STATE_CONNECTED;
    led_indicator_set(LED_STATE_CONNECTED);
    app_log("BLE", "Connected, ATVV voice_ready=%d", s_voice_ready);
    return true;
}

static bool do_connect_adv_device(NimBLEAdvertisedDevice* advDevice) {
    if (!advDevice) return false;
    s_ble_state = BLE_STATE_CONNECTING;

    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
        s_client->setConnectTimeout(4);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }
    s_client->setConnectTimeout(4);

    app_log("BLE", "Connecting to Advertised Device: %s (%s, Type: %d)...", 
            advDevice->getName().c_str(), advDevice->getAddress().toString().c_str(), 
            (int)advDevice->getAddress().getType());

    if (!s_client->connect(advDevice)) {
        app_log("BLE", "Connection Failed to %s", advDevice->getAddress().toString().c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        start_scan();
        return false;
    }

    s_connected_name = advDevice->getName().c_str();
    s_connected_mac = advDevice->getAddress().toString().c_str();
    s_bound_addr_type = advDevice->getAddress().getType();
    if (s_connected_name.length() == 0) {
        s_connected_name = (s_bound_name.length() > 0) ? s_bound_name : "Xiaomi Voice Remote";
    }

    // Save bound MAC to NVS:
    // Only update NVS if not previously bound, or if reconnecting to our bound remote whose MAC rotated
    if (s_bound_mac.length() == 0 || !s_bound_mac.equalsIgnoreCase(s_connected_mac)) {
        s_bound_mac = s_connected_mac;
        s_bound_name = s_connected_name;
        s_ble_prefs.putString("bound_mac", s_bound_mac);
        s_ble_prefs.putString("bound_name", s_bound_name);
        s_ble_prefs.putUChar("bound_type", s_bound_addr_type);
        app_log("BLE", "Bound and saved remote: %s (%s, Type: %d)", s_bound_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);
    }

    return setup_services_and_handshake();
}

static bool do_connect_mac(const String& mac_str, uint8_t addr_type) {
    s_ble_state = BLE_STATE_CONNECTING;
    NimBLEDevice::getScan()->stop();

    if (s_client == nullptr) {
        s_client = NimBLEDevice::createClient();
        s_client->setClientCallbacks(new ClientCallbacks(), false);
        s_client->setConnectTimeout(4);
    } else if (s_client->isConnected()) {
        s_client->disconnect();
    }
    s_client->setConnectTimeout(4);

    // Try primary addr_type
    NimBLEAddress addr1(mac_str.c_str(), addr_type);
    app_log("BLE", "Connecting to MAC: %s (Type: %d)...", mac_str.c_str(), (int)addr_type);
    bool ok = s_client->connect(addr1);

    // If failed, try alternative addr_type (Public vs Random)
    if (!ok) {
        uint8_t alt_type = (addr_type == BLE_ADDR_RANDOM) ? BLE_ADDR_PUBLIC : BLE_ADDR_RANDOM;
        NimBLEAddress addr2(mac_str.c_str(), alt_type);
        app_log("BLE", "Retrying with alternate Type: %d...", (int)alt_type);
        ok = s_client->connect(addr2);
        if (ok) addr_type = alt_type;
    }

    if (!ok) {
        app_log("BLE", "Direct link to %s timed out. Background scanner active, awaiting remote broadcast...", mac_str.c_str());
        s_ble_state = BLE_STATE_DISCONNECTED;
        start_scan();
        return false;
    }

    s_connected_mac = mac_str;
    s_connected_name = "Xiaomi Voice Remote";
    portENTER_CRITICAL(&s_disc_mux);
    for (const auto& d : s_discovered_devices) {
        if (d.mac.equalsIgnoreCase(mac_str) && d.name.length() > 0 && d.name != "Unnamed BLE Device") {
            s_connected_name = d.name;
            break;
        }
    }
    portEXIT_CRITICAL(&s_disc_mux);

    s_bound_mac = s_connected_mac;
    s_bound_name = s_connected_name;
    s_bound_addr_type = addr_type;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_bound_name);
    s_ble_prefs.putUChar("bound_type", s_bound_addr_type);
    app_log("BLE", "Manually paired and saved: %s (%s, Type: %d)", s_bound_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);

    return setup_services_and_handshake();
}

extern "C" {

void ble_remote_init(void) {
    s_ble_prefs.begin("ble_conf", false);
    s_bound_mac = s_ble_prefs.getString("bound_mac", "");
    s_bound_name = s_ble_prefs.getString("bound_name", "");
    s_bound_addr_type = s_ble_prefs.getUChar("bound_type", BLE_ADDR_RANDOM);

    if (s_bound_mac.length() > 0) {
        app_log("BLE", "Loaded previously bound remote: %s (%s, Type: %d)", s_bound_name.c_str(), s_bound_mac.c_str(), (int)s_bound_addr_type);
    }

    NimBLEDevice::init("ESP32-RemoteBridge");
    NimBLEDevice::setPower(ESP_PWR_LVL_P9);
    // Just Works bonding: bonding=true, mitm=false, sc=true
    // (MITM must be false because BLE_HS_IO_NO_INPUT_OUTPUT cannot support MITM authentication)
    NimBLEDevice::setSecurityAuth(true, false, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityInitKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);
    NimBLEDevice::setSecurityRespKey(BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID);

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks());
    
    // Start continuous fast scan immediately (instant catch when remote advertises)
    start_scan();
}

void ble_remote_task(void) {
    uint32_t now = millis();
    int request = s_mic_request;
    if (request && s_client && s_client->isConnected() && s_char_cmd && s_voice_ready) {
        s_mic_request = 0;
        uint8_t cmd[3] = {request > 0 ? (uint8_t)0x0C : (uint8_t)0x0D, 0, 0};
        size_t len = request > 0 ? (s_atvv_version >= 0x0100 ? 2 : 3) : (s_atvv_version >= 0x0100 ? 2 : 1);
        if (request < 0) cmd[1] = s_session_id;
        bool sent = s_char_cmd->writeValue(cmd, len, true);
        app_log("ATVV", "Mic %s sent=%d", request > 0 ? "open" : "close", sent);
    }

    // 1. Process asynchronous unpair / reconnect requests from Core 1
    if (s_req_unpair || s_req_reconnect) {
        bool is_unpair = s_req_unpair;
        s_req_unpair = false;
        s_req_reconnect = false;
        if (s_client && s_client->isConnected()) {
            s_client->disconnect();
        }
        if (s_pending_adv_device) {
            delete s_pending_adv_device;
            s_pending_adv_device = nullptr;
        }
        s_pending_mac = "";
        s_do_connect = false;
        if (is_unpair) {
            s_connected_name = "";
            s_connected_mac = "";
        }
        s_ble_state = BLE_STATE_DISCONNECTED;
        start_scan();
    }

    // 2. Process asynchronous connection requests from FreeRTOS task
    if (s_do_connect) {
        s_do_connect = false;
        if (s_client && s_client->isConnected()) {
            s_client->disconnect();
            vTaskDelay(pdMS_TO_TICKS(50));
        }
        if (s_pending_adv_device) {
            NimBLEAdvertisedDevice* adv = s_pending_adv_device;
            s_pending_adv_device = nullptr;
            do_connect_adv_device(adv);
            delete adv;
        } else if (s_pending_mac.length() > 0) {
            String mac = s_pending_mac;
            uint8_t type = s_pending_addr_type;
            s_pending_mac = "";
            do_connect_mac(mac, type);
        }
    }

    // 3. Auto Re-scan: If not connected, not connecting, and scan is inactive, restart continuous scan
    if (s_ble_state < BLE_STATE_CONNECTING && !s_do_connect && !s_req_unpair && !s_req_reconnect) {
        if (!NimBLEDevice::getScan()->isScanning()) {
            start_scan();
        }
    }
}

ble_remote_state_t ble_remote_get_state(void) {
    return s_ble_state;
}

void ble_remote_trigger_reconnect(void) {
    app_log("BLE", "Triggering BLE reconnection on Core 0...");
    s_req_reconnect = true;
}

String ble_remote_scan_devices_json(void) {
    JsonDocument doc;
    JsonArray arr = doc["devices"].to<JsonArray>();

    portENTER_CRITICAL(&s_disc_mux);
    for (const auto& dev : s_discovered_devices) {
        JsonObject obj = arr.add<JsonObject>();
        obj["name"] = dev.name;
        obj["mac"] = dev.mac;
        obj["rssi"] = dev.rssi;
        obj["type"] = (int)dev.type;
    }
    portEXIT_CRITICAL(&s_disc_mux);

    String out;
    serializeJson(doc, out);
    return out;
}

bool ble_remote_connect_target(const String& mac_str, uint8_t addr_type, const String& dev_name) {
    if (mac_str.length() == 0) return false;

    app_log("BLE", "Manual target requested: %s (%s, Type: %d)", 
            dev_name.length() > 0 ? dev_name.c_str() : "Unknown", 
            mac_str.c_str(), (int)addr_type);

    // Delete any old bond for this target address so fresh pairing can proceed cleanly
    NimBLEDevice::deleteBond(NimBLEAddress(mac_str.c_str(), addr_type));

    // Save target as bound remote immediately in memory and NVS
    s_bound_mac = mac_str;
    s_bound_name = (dev_name.length() > 0 && dev_name != "Unnamed BLE Device") ? dev_name : "Xiaomi Voice Remote";
    s_bound_addr_type = addr_type;
    s_ble_prefs.putString("bound_mac", s_bound_mac);
    s_ble_prefs.putString("bound_name", s_bound_name);
    s_ble_prefs.putUChar("bound_type", s_bound_addr_type);
    app_log("BLE", "Bound target saved to NVS: %s (%s)", s_bound_name.c_str(), s_bound_mac.c_str());

    // Queue connection to be handled safely on Core 0
    s_pending_mac = mac_str;
    s_pending_addr_type = addr_type;
    s_do_connect = true;
    return true;
}

bool ble_remote_connect_mac(const String& mac_str) {
    uint8_t addr_type = BLE_ADDR_PUBLIC;
    String dev_name = "Xiaomi Voice Remote";
    portENTER_CRITICAL(&s_disc_mux);
    for (const auto& d : s_discovered_devices) {
        if (d.mac.equalsIgnoreCase(mac_str)) {
            addr_type = d.type;
            if (d.name.length() > 0 && d.name != "Unnamed BLE Device") {
                dev_name = d.name;
            }
            break;
        }
    }
    portEXIT_CRITICAL(&s_disc_mux);
    return ble_remote_connect_target(mac_str, addr_type, dev_name);
}

void ble_remote_unpair(void) {
    s_bound_mac = "";
    s_bound_name = "";
    s_ble_prefs.remove("bound_mac");
    s_ble_prefs.remove("bound_name");
    s_ble_prefs.remove("bound_type");
    NimBLEDevice::deleteAllBonds();
    app_log("BLE", "Unpaired: deleted all NimBLE bonds and cleared saved MAC from NVS");
    s_req_unpair = true;
}

String ble_remote_get_connected_info(void) {
    JsonDocument doc;
    doc["connected"] = (s_ble_state >= BLE_STATE_CONNECTED);
    doc["state"] = (int)s_ble_state;
    doc["name"] = s_connected_name;
    doc["mac"] = s_connected_mac;
    doc["bound_mac"] = s_bound_mac;
    doc["bound_name"] = s_bound_name;
    String out;
    serializeJson(doc, out);
    return out;
}

bool ble_remote_voice_ready(void) {
    return s_voice_ready;
}

} // extern "C"
