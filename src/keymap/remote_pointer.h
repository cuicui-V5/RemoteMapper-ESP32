#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t buttons;
    int8_t x, y, wheel, pan;
} remote_pointer_report_t;

typedef struct {
    uint8_t direction, buttons;
    bool dragging, scrolling, tv_held;
    uint32_t started, last_move, tv_down;
} remote_pointer_t;

void remote_pointer_init(remote_pointer_t *state);
bool remote_pointer_handles(uint8_t key);
bool remote_pointer_key(remote_pointer_t *state, uint8_t key, bool pressed,
                        uint32_t now, remote_pointer_report_t *report);
bool remote_pointer_tick(remote_pointer_t *state, uint32_t now,
                         remote_pointer_report_t *report);
bool remote_pointer_reset(remote_pointer_t *state, remote_pointer_report_t *report);

#ifdef __cplusplus
}
#endif
