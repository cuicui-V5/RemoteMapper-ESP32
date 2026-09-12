#include "led_indicator.h"
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#ifndef RGB_BUILTIN
#define RGB_BUILTIN 48 // Default for most ESP32-S3 boards if not defined
#endif

static led_state_t s_current_base_state = LED_STATE_WAIT_CONNECTION;
static uint32_t s_flash_expire_time = 0;
static led_state_t s_flash_state = LED_STATE_WAIT_CONNECTION;
static bool s_is_flashing = false;

static uint32_t s_layer_color = 0x00FF00; // Default green for Layer 0
static bool s_layer_flash = false;
static bool s_low_battery = false;

static void update_hardware_led(led_state_t state) {
    if (s_layer_flash && s_is_flashing) {
        uint8_t r = (uint8_t)(((s_layer_color >> 16) & 0xFF) * 36 / 255);
        uint8_t g = (uint8_t)(((s_layer_color >> 8) & 0xFF) * 36 / 255);
        uint8_t b = (uint8_t)((s_layer_color & 0xFF) * 36 / 255);
        neopixelWrite(RGB_BUILTIN, r, g, b);
        return;
    }

    // Low battery white double-pulse alert when connected
    if (s_low_battery && state == LED_STATE_CONNECTED) {
        uint32_t phase = millis() % 2000;
        if ((phase < 120) || (phase >= 220 && phase < 340)) {
            neopixelWrite(RGB_BUILTIN, 32, 32, 32); // Crisp White flash
            return;
        }
    }

    switch (state) {
        case LED_STATE_WAIT_CONNECTION:
            neopixelWrite(RGB_BUILTIN, 24, 0, 0); // Red
            break;
        case LED_STATE_CONNECTED: {
            uint8_t r = (uint8_t)(((s_layer_color >> 16) & 0xFF) * 20 / 255);
            uint8_t g = (uint8_t)(((s_layer_color >> 8) & 0xFF) * 20 / 255);
            uint8_t b = (uint8_t)((s_layer_color & 0xFF) * 20 / 255);
            neopixelWrite(RGB_BUILTIN, r, g, b);
            break;
        }
        case LED_STATE_MIC_STREAMING:
            neopixelWrite(RGB_BUILTIN, 0, 0, 24); // Blue
            break;
        case LED_STATE_HID_KEY_PRESS:
            neopixelWrite(RGB_BUILTIN, 24, 24, 0); // Yellow
            break;
        case LED_STATE_MIC_KEY_PRESS:
            neopixelWrite(RGB_BUILTIN, 24, 0, 0); // Red flash
            break;
        default:
            neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off
            break;
    }
}

static void led_task(void *arg) {
    while (1) {
        if (s_is_flashing) {
            if (millis() > s_flash_expire_time) {
                s_is_flashing = false;
                update_hardware_led(s_current_base_state);
            } else {
                update_hardware_led(s_flash_state);
            }
        } else {
            update_hardware_led(s_current_base_state);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void led_indicator_init(void) {
    update_hardware_led(LED_STATE_WAIT_CONNECTION);
    xTaskCreatePinnedToCore(led_task, "led_task", 2048, NULL, 1, NULL, 1);
}

void led_indicator_set(led_state_t state) {
    if (state == LED_STATE_HID_KEY_PRESS || state == LED_STATE_MIC_KEY_PRESS) return; // Use trigger for flashes
    s_is_flashing = false;
    s_current_base_state = state;
    update_hardware_led(state);
}

void led_indicator_trigger_key(bool is_voice_key) {
    s_layer_flash = false;
    s_flash_state = is_voice_key ? LED_STATE_MIC_KEY_PRESS : LED_STATE_HID_KEY_PRESS;
    s_flash_expire_time = millis() + 100; // Flash for 100ms
    s_is_flashing = true;
}

void led_indicator_set_layer_color(uint32_t rgb_color) {
    s_layer_color = (rgb_color == 0) ? 0x00FF00 : rgb_color;
    s_layer_flash = true;
    s_flash_expire_time = millis() + 200; // 200ms flash on layer change
    s_is_flashing = true;
}

void led_indicator_set_low_battery(bool is_low) {
    s_low_battery = is_low;
}

