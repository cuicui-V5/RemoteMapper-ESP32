#include "remote_pointer.h"
#include "key_definitions.h"
#include <string.h>

void remote_pointer_init(remote_pointer_t *state) {
    memset(state, 0, sizeof(*state));
}

static bool direction_key(uint8_t key) {
    return key == MI_KEY_UP || key == MI_KEY_DOWN || key == MI_KEY_LEFT || key == MI_KEY_RIGHT;
}

bool remote_pointer_handles(uint8_t key) {
    return direction_key(key) || key == MI_KEY_OK || key == MI_KEY_MENU ||
           key == MI_KEY_HOME || key == MI_KEY_TV;
}

static uint8_t buttons(const remote_pointer_t *state) {
    return state->buttons | (state->dragging ? 1 : 0);
}

static void movement(remote_pointer_t *state, uint32_t now, remote_pointer_report_t *report) {
    *report = (remote_pointer_report_t){.buttons = buttons(state)};
    uint32_t elapsed = now - state->started;
    // 短按精细移动，持续按住后在一秒内增至每次 16 个单位。
    int8_t distance = elapsed >= 1000 ? 16 : 1 + elapsed * 15 / 1000;
    if (state->scrolling) {
        if (state->direction == MI_KEY_UP) report->wheel = 1;
        if (state->direction == MI_KEY_DOWN) report->wheel = -1;
        if (state->direction == MI_KEY_LEFT) report->pan = -1;
        if (state->direction == MI_KEY_RIGHT) report->pan = 1;
    } else {
        if (state->direction == MI_KEY_UP) report->y = -distance;
        if (state->direction == MI_KEY_DOWN) report->y = distance;
        if (state->direction == MI_KEY_LEFT) report->x = -distance;
        if (state->direction == MI_KEY_RIGHT) report->x = distance;
    }
    state->last_move = now;
}

bool remote_pointer_key(remote_pointer_t *state, uint8_t key, bool pressed,
                        uint32_t now, remote_pointer_report_t *report) {
    if (direction_key(key)) {
        if (pressed) {
            state->direction = key;
            state->started = now;
            movement(state, now, report);
            return true;
        }
        if (state->direction == key) state->direction = 0;
        return false;
    }
    uint8_t previous = buttons(state);
    if (key == MI_KEY_OK || key == MI_KEY_MENU) {
        uint8_t button = key == MI_KEY_OK ? 1 : 2;
        if (pressed) state->buttons |= button;
        else state->buttons &= (uint8_t)~button;
    } else if (key == MI_KEY_HOME && pressed) {
        state->scrolling = false;
        state->dragging = !state->dragging;
    } else if (key == MI_KEY_TV) {
        if (pressed) {
            state->tv_held = true;
            state->tv_down = now;
        } else {
            if (state->tv_held && (uint32_t)(now - state->tv_down) < 700) {
                state->scrolling = !state->scrolling;
                state->direction = 0;
                state->dragging = false;
            }
            state->tv_held = false;
        }
    }
    *report = (remote_pointer_report_t){.buttons = buttons(state)};
    return previous != report->buttons;
}

bool remote_pointer_tick(remote_pointer_t *state, uint32_t now,
                         remote_pointer_report_t *report) {
    uint32_t held = now - state->started;
    uint32_t interval = state->scrolling ? (held >= 1000 ? 40 : 120) : 20;
    if (!state->direction || (uint32_t)(now - state->last_move) < interval) return false;
    movement(state, now, report);
    return true;
}

bool remote_pointer_reset(remote_pointer_t *state, remote_pointer_report_t *report) {
    bool release = buttons(state) != 0;
    remote_pointer_init(state);
    *report = (remote_pointer_report_t){0};
    return release;
}
