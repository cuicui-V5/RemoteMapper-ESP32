#include "usb/uac_microphone.h"
#include "esp32-hal-tinyusb.h"
#include "led_indicator.h"
#include "audio/audio_pipeline.h"
#include "log/app_log.h"
#include "tusb.h"
#include "device/usbd_pvt.h"
#include <atomic>

#define UAC_DESC_TOTAL_LEN  108

static uint8_t s_uac_ep_in   = 0;
static uint8_t s_uac_itf_ac  = 0;
static uint8_t s_uac_itf_as  = 0;
static uint8_t s_uac_str_idx = 0;
static uint8_t s_uac_alt     = 0;
static std::atomic<bool> s_uac_streaming{false};
static bool          s_uac_initialized = false;
static bool          s_endpoint_open = false;
static tusb_desc_endpoint_t s_endpoint_descriptor = {};

// Controls
static uint8_t  s_mic_mute   = 0;
static int16_t  s_mic_volume = 0x0000;

static constexpr size_t UAC_PACKET_SAMPLES = AUDIO_SAMPLE_RATE / 1000;
static constexpr size_t UAC_PACKET_BYTES = UAC_PACKET_SAMPLES * sizeof(int16_t);
static_assert(UAC_PACKET_BYTES == 32, "UAC requires 16 kHz mono 16-bit audio");
static DRAM_ATTR int16_t s_tx_buf[UAC_PACKET_SAMPLES];
static uint8_t s_sample_frequency[3] = {0x80, 0x3E, 0x00};
static uint8_t s_requested_sample_frequency[3] = {};

// UAC 1.0 每 1 ms 传输 16 个采样，空闲期间由音频模块提供静音。
static std::atomic<uint32_t> s_xfer_cb_count{0};
static std::atomic<uint32_t> s_xfer_failed_count{0};

static bool uac_queue_packet(uint8_t rhport) {
    uint8_t ep_addr = (uint8_t)(s_uac_ep_in | 0x80);
    if (usbd_edpt_busy(rhport, ep_addr)) return true;
    if (!usbd_edpt_claim(rhport, ep_addr)) return true;

    memset(s_tx_buf, 0, sizeof(s_tx_buf));
    if (!s_mic_mute) {
        audio_pipeline_read_for_usb(&g_audio_pipeline, s_tx_buf, UAC_PACKET_SAMPLES);
    }
    if (!usbd_edpt_xfer(rhport, ep_addr, (uint8_t*)s_tx_buf, sizeof(s_tx_buf))) {
        usbd_edpt_release(rhport, ep_addr);
        app_log("UAC", "Queuing audio transfer failed");
        return false;
    }
    return true;
}

static void uac_watchdog_task(void* arg) {
    uint32_t last_count = 0;
    uint32_t stuck_ticks = 0;
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // ONLY monitor if USB is fully mounted, active, NOT suspended by host, and streaming
        const uint32_t completed = s_xfer_cb_count.load(std::memory_order_relaxed);
        if (tud_mounted() && !tud_suspended() && s_uac_streaming.load(std::memory_order_relaxed)) {
            if (completed == last_count) {
                stuck_ticks += 100;
                if (stuck_ticks >= 3000) { // Relaxed to 3.0 seconds
                    app_log("UAC", "Audio transfer idle for 3s: endpoint_busy=%d",
                            usbd_edpt_busy(0, (uint8_t)(s_uac_ep_in | 0x80)));
                    last_count = s_xfer_cb_count.load(std::memory_order_relaxed);
                    stuck_ticks = 0;
                }
            } else {
                last_count = completed;
                stuck_ticks = 0;
            }
        } else {
            stuck_ticks = 0;
            last_count = completed;
        }
    }
}

// ---------------------------------------------------------------------------
// Descriptor
// ---------------------------------------------------------------------------
static uint16_t uac_load_descriptor(uint8_t *dst, uint8_t *itf) {
    if (!dst || !itf) return 0;

    s_uac_itf_ac = *itf;
    s_uac_itf_as = *itf + 1;
    *itf += 2;

    s_uac_str_idx = tinyusb_add_string_descriptor("RemoteMapper Wireless Microphone");

    uint8_t desc[UAC_DESC_TOTAL_LEN] = {
        // 1. IAD — 8 bytes
        0x08, 0x0B, s_uac_itf_ac, 0x02, 0x01, 0x00, 0x00, s_uac_str_idx,
        // 2. Standard AC Interface — 9 bytes
        0x09, 0x04, s_uac_itf_ac, 0x00, 0x00, 0x01, 0x01, 0x00, s_uac_str_idx,
        // 3. CS AC Header — 9 bytes
        0x09, 0x24, 0x01, 0x00, 0x01, 0x27, 0x00, 0x01, s_uac_itf_as,
        // 4. Input Terminal (Microphone) — 12 bytes
        0x0C, 0x24, 0x02, 0x01, 0x01, 0x02, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00,
        // 5. Feature Unit (Mute + Volume) — 9 bytes
        0x09, 0x24, 0x06, 0x02, 0x01, 0x01, 0x01, 0x02, 0x00,
        // 6. Output Terminal (USB Streaming) — 9 bytes
        0x09, 0x24, 0x03, 0x03, 0x01, 0x01, 0x00, 0x02, 0x00,
        // 7. Standard AS Interface Alt 0 (Zero BW) — 9 bytes
        0x09, 0x04, s_uac_itf_as, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00,
        // 8. Standard AS Interface Alt 1 (Active 16kHz) — 9 bytes
        0x09, 0x04, s_uac_itf_as, 0x01, 0x01, 0x01, 0x02, 0x00, 0x00,
        // 9. CS AS General — 7 bytes
        0x07, 0x24, 0x01, 0x03, 0x01, 0x01, 0x00,
        // 10. Format Type I (16kHz, 16-bit, Mono) — 11 bytes
        0x0B, 0x24, 0x02, 0x01, 0x01, 0x02, 0x10, 0x01, 0x80, 0x3E, 0x00,
        // 11. UAC 1.0 音频端点：每 1 ms 传输 32 字节。
        0x09, 0x05, (uint8_t)(s_uac_ep_in | 0x80), 0x05, 0x20, 0x00, 0x01, 0x00, 0x00,
        // 12. CS Endpoint General — 7 bytes
        0x07, 0x25, 0x01, 0x01, 0x00, 0x00, 0x00
    };

    memcpy(dst, desc, sizeof(desc));
    return sizeof(desc);
}

// ---------------------------------------------------------------------------
// TinyUSB Custom Class Driver
// ---------------------------------------------------------------------------
static void uac_driver_init(void) {}

static void uac_driver_reset(uint8_t rhport) {
    (void)rhport;
    s_uac_streaming.store(false, std::memory_order_relaxed);
    s_endpoint_open = false;
    s_uac_alt = 0;
    s_xfer_cb_count.store(0, std::memory_order_relaxed);
    s_xfer_failed_count.store(0, std::memory_order_relaxed);
    app_log("UAC", "USB Bus Reset detected -> UAC state reset");
}

static uint16_t uac_driver_open(uint8_t rhport, tusb_desc_interface_t const *desc_intf, uint16_t max_len) {
    if (desc_intf->bInterfaceClass != TUSB_CLASS_AUDIO) return 0;
    uint8_t const *p = (uint8_t const *)desc_intf;
    uint16_t len = 0;
    while (len < max_len) {
        if (p[1] == TUSB_DESC_INTERFACE) {
            if (((tusb_desc_interface_t const*)p)->bInterfaceClass != TUSB_CLASS_AUDIO) break;
        } else if (p[1] == TUSB_DESC_ENDPOINT) {
            s_endpoint_descriptor = *(tusb_desc_endpoint_t const *)p;
            if (!usbd_edpt_open(rhport, &s_endpoint_descriptor)) {
                app_log("UAC", "Opening audio endpoint failed");
                return 0;
            }
            s_endpoint_open = true;
        }
        len += p[0]; p += p[0];
    }
    return len;
}

static bool uac_driver_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                         tusb_control_request_t const *req) {
    const uint8_t recipient = req->bmRequestType_bit.recipient;
    const uint8_t direction = req->bmRequestType_bit.direction;
    const uint8_t type = req->bmRequestType_bit.type;
    const uint8_t endpoint = (uint8_t)(s_uac_ep_in | 0x80);
    if (stage == CONTROL_STAGE_DATA && type == TUSB_REQ_TYPE_CLASS &&
        recipient == TUSB_REQ_RCPT_ENDPOINT && req->bRequest == 0x01 &&
        req->wIndex == endpoint && req->wValue == 0x0100 && req->wLength == 3) {
        app_log("UAC", "Sampling frequency SET_CUR: %02X %02X %02X; fixed=16000 Hz",
                s_requested_sample_frequency[0], s_requested_sample_frequency[1],
                s_requested_sample_frequency[2]);
        return memcmp(s_requested_sample_frequency, s_sample_frequency,
                      sizeof(s_sample_frequency)) == 0;
    }
    if (stage != CONTROL_STAGE_SETUP) return true;

    // AC 只有 alternate 0；AS 使用 alternate 0 和 1 管理音频传输。
    if (type == TUSB_REQ_TYPE_STANDARD && recipient == TUSB_REQ_RCPT_INTERFACE) {
        const bool is_ac = req->wIndex == s_uac_itf_ac;
        const bool is_as = req->wIndex == s_uac_itf_as;
        if ((is_ac || is_as) && req->bRequest == TUSB_REQ_SET_INTERFACE &&
            direction == TUSB_DIR_OUT && req->wLength == 0 &&
            ((is_ac && req->wValue == 0) || (is_as && req->wValue <= 1))) {
            app_log("UAC", "SET_INTERFACE interface=%u alternate=%u",
                    (unsigned)req->wIndex, (unsigned)req->wValue);
            if (is_as) {
                s_uac_streaming.store(false, std::memory_order_relaxed);
                s_uac_alt = 0;
                if (req->wValue == 1) {
                    if (!s_endpoint_open) {
                        if (!usbd_edpt_open(rhport, &s_endpoint_descriptor)) {
                            app_log("UAC", "Opening audio endpoint failed");
                            return false;
                        }
                        s_endpoint_open = true;
                    }
                    s_uac_alt = 1;
                    s_uac_streaming.store(true, std::memory_order_relaxed);
                    if (!uac_queue_packet(rhport)) {
                        s_uac_streaming.store(false, std::memory_order_relaxed);
                        s_uac_alt = 0;
                        return false;
                    }
                } else if (s_endpoint_open) {
                    usbd_edpt_close(rhport, endpoint);
                    s_endpoint_open = false;
                }
            }
            return tud_control_status(rhport, req);
        }
        if ((is_ac || is_as) && req->bRequest == TUSB_REQ_GET_INTERFACE &&
            direction == TUSB_DIR_IN && req->wValue == 0 && req->wLength == 1) {
            static uint8_t ac_alt = 0;
            return tud_control_xfer(rhport, req, is_ac ? &ac_alt : &s_uac_alt, 1);
        }
    }

    // 固定频率端点接受采样频率控制，并始终报告真实的 16 kHz。
    if (type == TUSB_REQ_TYPE_CLASS && recipient == TUSB_REQ_RCPT_ENDPOINT &&
        req->wIndex == endpoint && req->wValue == 0x0100 && req->wLength == 3) {
        if (direction == TUSB_DIR_IN && req->bRequest == 0x81) {
            return tud_control_xfer(rhport, req, s_sample_frequency, sizeof(s_sample_frequency));
        }
        if (direction == TUSB_DIR_OUT && req->bRequest == 0x01) {
            return tud_control_xfer(rhport, req, s_requested_sample_frequency,
                                    sizeof(s_requested_sample_frequency));
        }
    }

    // Feature Unit 2：主通道具有静音控制，单声道通道 1 具有音量控制。
    if (type == TUSB_REQ_TYPE_CLASS && recipient == TUSB_REQ_RCPT_INTERFACE &&
        req->wIndex == (uint16_t)((0x02 << 8) | s_uac_itf_ac)) {
        if (req->wValue == 0x0100 && req->wLength == 1) {
            if ((direction == TUSB_DIR_IN && req->bRequest == 0x81) ||
                (direction == TUSB_DIR_OUT && req->bRequest == 0x01)) {
                return tud_control_xfer(rhport, req, &s_mic_mute, 1);
            }
        }
        if (req->wValue == 0x0201 && req->wLength == 2) {
            if ((direction == TUSB_DIR_IN && req->bRequest == 0x81) ||
                (direction == TUSB_DIR_OUT && req->bRequest == 0x01)) {
                return tud_control_xfer(rhport, req, &s_mic_volume, 2);
            }
            static int16_t vol_min = -32768, vol_max = 0, vol_res = 256;
            if (direction == TUSB_DIR_IN && req->bRequest == 0x82) {
                return tud_control_xfer(rhport, req, &vol_min, 2);
            }
            if (direction == TUSB_DIR_IN && req->bRequest == 0x83) {
                return tud_control_xfer(rhport, req, &vol_max, 2);
            }
            if (direction == TUSB_DIR_IN && req->bRequest == 0x84) {
                return tud_control_xfer(rhport, req, &vol_res, 2);
            }
        }
    }
    app_log("UAC", "Control request rejected: bm=0x%02X request=0x%02X value=0x%04X index=0x%04X length=%u",
            req->bmRequestType, req->bRequest, req->wValue, req->wIndex, (unsigned)req->wLength);
    return false;
}

static bool uac_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr,
                                 xfer_result_t result, uint32_t xferred_bytes) {
    if (ep_addr == (uint8_t)(s_uac_ep_in | 0x80)) {
        if (!s_endpoint_open || s_uac_alt != 1 ||
            !s_uac_streaming.load(std::memory_order_relaxed)) return true;
        if (result != XFER_RESULT_SUCCESS) {
            const uint32_t failures = s_xfer_failed_count.fetch_add(1, std::memory_order_relaxed) + 1;
            if (failures == 1 || failures % 1000 == 0) {
                app_log("UAC", "Audio transfer failed: result=%u bytes=%u total=%lu",
                        (unsigned)result, (unsigned)xferred_bytes, (unsigned long)failures);
            }
            if (result != XFER_RESULT_FAILED) {
                s_uac_streaming.store(false, std::memory_order_relaxed);
                return true;
            }
        } else {
            s_xfer_cb_count.fetch_add(1, std::memory_order_relaxed);
        }
        // ISO 丢包后继续发送后续音频，失败包单独计数。
        if (s_uac_streaming.load(std::memory_order_relaxed)) {
            if (!uac_queue_packet(rhport)) {
                s_uac_streaming.store(false, std::memory_order_relaxed);
            }
        }
        return true;
    }
    return true;
}

static usbd_class_driver_t const s_uac_driver = {
#if CFG_TUSB_DEBUG >= CFG_TUD_LOG_LEVEL
    .name             = "UAC_MIC",
#endif
    .init             = uac_driver_init,
    .reset            = uac_driver_reset,
    .open             = uac_driver_open,
    .control_xfer_cb  = uac_driver_control_xfer_cb,
    .xfer_cb          = uac_driver_xfer_cb,
    .sof              = NULL
};

usbd_class_driver_t const* usbd_app_driver_get_cb(uint8_t* driver_count) {
    *driver_count = 1;
    return &s_uac_driver;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
extern "C" {

bool uac_microphone_init(void) {
    if (s_uac_initialized) return true;
    audio_pipeline_init(&g_audio_pipeline);
    
    if (s_uac_ep_in == 0) {
        s_uac_ep_in = tinyusb_get_free_in_endpoint();
    }
    
    // Spawn recovery watchdog
    xTaskCreatePinnedToCore(uac_watchdog_task, "uac_wdg", 4096, NULL, 5, NULL, 1);

    esp_err_t err = tinyusb_enable_interface(USB_INTERFACE_CUSTOM, UAC_DESC_TOTAL_LEN, uac_load_descriptor);
    if (err != ESP_OK) {
        app_log("UAC", "Failed to enable UAC interface: %d", err);
        return false;
    }
    s_uac_initialized = true;
    app_log("UAC", "UAC 1.0 Microphone ready (EP %d IN, Interrupt + Watchdog)", s_uac_ep_in);
    return true;
}

void uac_microphone_task(void) {
    // Nothing — entirely driven by USB hardware xfer_cb
}

bool uac_microphone_is_streaming(void) {
    return s_uac_streaming.load(std::memory_order_relaxed);
}

uint32_t uac_microphone_transfer_count(void) {
    return s_xfer_cb_count.load(std::memory_order_relaxed);
}

uint32_t uac_microphone_failure_count(void) {
    return s_xfer_failed_count.load(std::memory_order_relaxed);
}

} // extern "C"
