#include "usb/uac_microphone.h"
#include "esp32-hal-tinyusb.h"
#include "led_indicator.h"
#include "audio/audio_pipeline.h"
#include "log/app_log.h"
#include "tusb.h"
#include "device/usbd_pvt.h"

#define UAC_DESC_TOTAL_LEN  108

static uint8_t s_uac_ep_in   = 0;
static uint8_t s_uac_itf_ac  = 0;
static uint8_t s_uac_itf_as  = 0;
static uint8_t s_uac_str_idx = 0;
static uint8_t s_uac_alt     = 0;
static volatile bool s_uac_streaming   = false;
static bool          s_uac_initialized = false;

// Controls
static uint8_t  s_mic_mute   = 0;
static int16_t  s_mic_volume = 0x0000;

static DRAM_ATTR int16_t s_tx_buf[32]; // 32 samples (64 bytes) for 2ms batches

// We cannot use xfer_cb because queuing inside xfer_cb misses alternating frames (500Hz).
// The solution: A dedicated 500Hz FreeRTOS task (every 2ms). It reads 32 samples
// (64 bytes) and sends them. Because 2ms > 1ms, the USB hardware is guaranteed to
// be free. The average rate is exactly 16000 samples/sec, which Windows UAC perfectly
// absorbs despite the 1ms endpoint polling rate.
// ---------------------------------------------------------------------------
static volatile uint32_t s_xfer_cb_count = 0;

static void uac_soft_reopen_endpoint(uint8_t rhport) {
    uint8_t ep_addr = (uint8_t)(s_uac_ep_in | 0x80);
    usbd_edpt_close(rhport, ep_addr);

    tusb_desc_endpoint_t ep;
    ep.bLength          = sizeof(tusb_desc_endpoint_t);
    ep.bDescriptorType  = TUSB_DESC_ENDPOINT;
    ep.bEndpointAddress = ep_addr;
    ep.bmAttributes.xfer = TUSB_XFER_ISOCHRONOUS;
    ep.bmAttributes.sync = 1;
    ep.wMaxPacketSize   = 64;
    ep.bInterval        = 2;
    usbd_edpt_open(rhport, &ep);

    memset(s_tx_buf, 0, 64);
    usbd_edpt_xfer(rhport, ep_addr, (uint8_t*)s_tx_buf, 64);
}

static void uac_watchdog_task(void* arg) {
    uint32_t last_count = 0;
    uint32_t stuck_ticks = 0;
    while(1) {
        vTaskDelay(pdMS_TO_TICKS(100));
        
        // ONLY monitor if USB is fully mounted, active, NOT suspended by host, and streaming
        if (tud_mounted() && !tud_suspended() && s_uac_streaming) {
            if (s_xfer_cb_count == last_count) {
                stuck_ticks += 100;
                if (stuck_ticks >= 3000) { // Relaxed to 3.0 seconds
                    app_log("UAC", "Watchdog: Isochronous IN idle/stalled for 3s. Performing clean soft reset...");
                    uac_soft_reopen_endpoint(0);
                    last_count = s_xfer_cb_count;
                    stuck_ticks = 0;
                }
            } else {
                last_count = s_xfer_cb_count;
                stuck_ticks = 0;
            }
        } else {
            stuck_ticks = 0;
            last_count = s_xfer_cb_count;
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
        // 11. Isochronous Endpoint — 9 bytes (wMaxPacketSize=64, bInterval=2)
        0x09, 0x05, (uint8_t)(s_uac_ep_in | 0x80), 0x05, 0x40, 0x00, 0x02, 0x00, 0x00,
        // 12. CS Endpoint General — 7 bytes
        0x07, 0x25, 0x01, 0x00, 0x00, 0x00, 0x00
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
    s_uac_streaming = false;
    s_uac_alt = 0;
    s_xfer_cb_count = 0;
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
            usbd_edpt_open(rhport, (tusb_desc_endpoint_t const *)p);
        }
        len += p[0]; p += p[0];
    }
    return len;
}

static bool uac_driver_control_xfer_cb(uint8_t rhport, uint8_t stage,
                                         tusb_control_request_t const *req) {
    if (stage != CONTROL_STAGE_SETUP) return true;

    // 1. Standard USB Requests
    if (req->bmRequestType_bit.type == TUSB_REQ_TYPE_STANDARD) {
        if (req->bRequest == TUSB_REQ_SET_INTERFACE) {
            uint8_t itf = (uint8_t)req->wIndex;
            if (itf != s_uac_itf_as && itf != s_uac_itf_ac) {
                return false; // Crucial: Let other class drivers (HID, CDC) handle their own interfaces!
            }
            uint8_t alt = (uint8_t)req->wValue;
            s_uac_alt       = alt;
            s_uac_streaming = (alt == 1);
            
            if (alt == 1) {
                uac_soft_reopen_endpoint(rhport);
            } else {
                usbd_edpt_close(rhport, (uint8_t)(s_uac_ep_in | 0x80));
            }
            return tud_control_status(rhport, req);
        } else if (req->bRequest == TUSB_REQ_GET_INTERFACE) {
            uint8_t itf = (uint8_t)req->wIndex;
            if (itf != s_uac_itf_as && itf != s_uac_itf_ac) {
                return false; // Crucial: Let other class drivers handle their own interfaces!
            }
            return tud_control_xfer(rhport, req, &s_uac_alt, 1);
        }
        return false;
    }

    // 2. Class-Specific USB Requests
    if (req->bmRequestType_bit.type == TUSB_REQ_TYPE_CLASS) {
        uint8_t target_itf = (uint8_t)req->wIndex;
        if (target_itf != s_uac_itf_ac && target_itf != s_uac_itf_as && target_itf != 0x02) {
            return false; // Crucial: Let HID / CDC class drivers handle their own class requests!
        }
        uint8_t r  = req->bRequest;
        uint8_t cs = (uint8_t)(req->wValue >> 8);
        if (r == 0x81) {   // GET_CUR
            if (cs == 0x01) return tud_control_xfer(rhport, req, &s_mic_mute,   1);
            else             return tud_control_xfer(rhport, req, &s_mic_volume, 2);
        }
        if (r == 0x01) {   // SET_CUR
            if (cs == 0x01) return tud_control_xfer(rhport, req, &s_mic_mute,   1);
            else             return tud_control_xfer(rhport, req, &s_mic_volume, 2);
        }
        static int16_t vol_min = -32768, vol_max = 0, vol_res = 256;
        if (r == 0x82) return tud_control_xfer(rhport, req, &vol_min, 2);
        if (r == 0x83) return tud_control_xfer(rhport, req, &vol_max, 2);
        if (r == 0x84) return tud_control_xfer(rhport, req, &vol_res, 2);
        return false;
    }
    return false;
}

static bool uac_driver_xfer_cb(uint8_t rhport, uint8_t ep_addr,
                                 xfer_result_t result, uint32_t xferred_bytes) {
    if (ep_addr == (uint8_t)(s_uac_ep_in | 0x80)) {
        if (s_uac_streaming) {
            memset(s_tx_buf, 0, 64);
            if (!s_mic_mute) {
                audio_pipeline_read_for_usb(&g_audio_pipeline, s_tx_buf, 32);
            }
            usbd_edpt_xfer(rhport, ep_addr, (uint8_t*)s_tx_buf, 64);
            
            static uint32_t loop_counter = 0;
            loop_counter++;
            s_xfer_cb_count++;
            if (loop_counter % 1000 == 0) {
                app_log("UAC", "USB TX alive: streaming true, ringbuf avail: %d", audio_ring_buffer_available_read(&g_audio_pipeline.ring_buf));
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
    return s_uac_streaming;
}

} // extern "C"
