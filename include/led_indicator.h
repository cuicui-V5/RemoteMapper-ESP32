#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {

#endif

typedef enum {
    LED_STATE_WAIT_CONNECTION = 0, // Red solid
    LED_STATE_CONNECTED,           // Green solid
    LED_STATE_MIC_STREAMING,       // Blue solid
    LED_STATE_HID_KEY_PRESS,       // Yellow flash
    LED_STATE_MIC_KEY_PRESS        // Red flash
} led_state_t;

void led_indicator_init(void);
void led_indicator_set(led_state_t state);
void led_indicator_trigger_key(bool is_voice_key);
void led_indicator_set_layer_color(uint32_t rgb_color);
void led_indicator_set_low_battery(bool is_low);

#ifdef __cplusplus
}
#endif
