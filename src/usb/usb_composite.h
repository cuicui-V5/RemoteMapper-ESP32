#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "app_config.h"
#include "keymap/key_state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize TinyUSB stack with UAC 1.0 Microphone, HID Keyboard & Consumer, and CDC Serial
 */
void usb_composite_init(void);

/**
 * @brief Process periodic TinyUSB device tasks
 */
void usb_composite_task(void);

/**
 * @brief Send USB HID Keyboard Key Down (with modifier)
 */
bool usb_hid_keyboard_press(uint8_t modifier, uint8_t keycode);

/**
 * @brief Send USB HID Keyboard Key Up (release all keys)
 */
bool usb_hid_keyboard_release(void);

/**
 * @brief Send USB HID Keyboard Tap (Press then Release)
 */
bool usb_hid_keyboard_tap(uint8_t modifier, uint8_t keycode);
uint32_t usb_hid_keyboard_sent(void);
uint32_t usb_hid_keyboard_failed(void);

/**
 * @brief Send USB HID Consumer Control Code (e.g. Volume Up/Down, AC Back)
 */
bool usb_hid_consumer_press(uint16_t usage_code);

/**
 * @brief Release USB HID Consumer Control
 */
bool usb_hid_consumer_release(void);

/**
 * @brief Send USB HID Consumer Tap
 */
bool usb_hid_consumer_tap(uint16_t usage_code);
uint32_t usb_hid_consumer_sent(void);
uint32_t usb_hid_consumer_failed(void);

/**
 * @brief Dispatch high-level key action to USB HID
 */
void usb_hid_dispatch_action(const key_action_t *action);

/**
 * @brief Push 1ms frame of PCM audio to USB UAC Isochronous IN endpoint (32 bytes = 16 samples @ 16kHz)
 */
void usb_audio_task(void);

#ifdef __cplusplus
}
#endif
