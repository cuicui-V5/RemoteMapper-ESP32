#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ==========================================
// 1. Audio & DSP Parameters
// ==========================================
#define AUDIO_SAMPLE_RATE         16000     // 16kHz sample rate
#define AUDIO_BITS_PER_SAMPLE     16        // 16-bit PCM
#define AUDIO_CHANNELS            1         // Mono
#define AUDIO_DEFAULT_FRAME_BYTES 120       // Default 120 bytes ADPCM per BLE frame
#define AUDIO_DEFAULT_FRAME_SAMPS 240       // 120 * 2 = 240 PCM samples per frame
#define AUDIO_RING_BUFFER_SIZE    8192      // Ring buffer capacity (in samples, ~512ms buffer)

// AGC & Filter parameters
#define AGC_TARGET_LEVEL          28000.0f
#define AGC_DECAY_RATE            0.9997f
#define AGC_MAX_GAIN              30.0f
#define AGC_NOISE_FLOOR           200.0f
#define AGC_INITIAL_PEAK          28000.0f  // Soft-start: initial gain = 1.0x (0dB) to avoid 28x burst
#define AUDIO_LEAD_MUTE_SAMPLES   2400      // 150ms silence zone (2400 samples @ 16kHz) to eliminate button click & pops
#define AUDIO_FADE_IN_SAMPLES     160       // 10ms micro fade-in (160 samples @ 16kHz) - only for transient smoothing at boundary
#define DECLIP_THRESHOLD          1000

// ==========================================
// 2. BLE Central & ATVV Parameters
// ==========================================
#define BLE_REMOTE_NAME_PREFIX    "MI RC"
#define BLE_REMOTE_MAC_PREFIX     "c0:5d:39"
#define BLE_SCAN_INTERVAL_MS      100
#define BLE_SCAN_WINDOW_MS        80
#define BLE_KEEP_ALIVE_INTERVAL   2000      // MIC_EXTEND interval during active voice (ms)

// ATVV GATT UUIDs (128-bit)
#define ATVV_SVC_UUID             "ab5e0001-5a21-4f05-bc7d-af01f617b664"
#define ATVV_CHAR_CMD_UUID        "ab5e0002-5a21-4f05-bc7d-af01f617b664"
#define ATVV_CHAR_AUD_UUID        "ab5e0003-5a21-4f05-bc7d-af01f617b664"
#define ATVV_CHAR_CTL_UUID        "ab5e0004-5a21-4f05-bc7d-af01f617b664"

// HOGP UUIDs (Standard Bluetooth SIG 16-bit)
#define HOGP_SVC_UUID             0x1812
#define HOGP_REPORT_CHAR_UUID     0x2A4D

// ==========================================
// 3. FreeRTOS Task & Multi-Core Pinning
// ==========================================
#define TASK_CORE_BLE             0         // Core 0: BLE & Audio decoding
#define TASK_CORE_USB             1         // Core 1: TinyUSB & HID/Audio push

#define PRIO_TASK_USB             6         // Real-time USB Isochronous & HID
#define PRIO_TASK_AUDIO_DSP       5         // High-priority audio stream decoding
#define PRIO_TASK_BLE             4         // NimBLE client processing
#define PRIO_TASK_KEYMAP_CLI      3         // Key mapping state machine & CLI

// ==========================================
// 4. Default Keymap & Hotkey Codes
// ==========================================
// Default Voice Input Hotkey: Right Alt + Comma (WeChat Voice IME)
// HID Keyboard Modifiers: 0x01=LCTRL, 0x02=LSHIFT, 0x04=LALT, 0x08=LGUI, 0x10=RCTRL, 0x20=RSHIFT, 0x40=RALT, 0x80=RGUI
#define DEFAULT_VOICE_MODIFIER    0x40      // KEY_MOD_RALT
#define DEFAULT_VOICE_KEY         0x36      // HID Usage for Comma ','

#ifdef __cplusplus
}
#endif
