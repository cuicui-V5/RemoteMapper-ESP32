#include "keymap/remote_pointer.h"
#include "keymap/key_definitions.h"
#include <assert.h>
#include <stdio.h>

int main(void) {
    remote_pointer_t state;
    remote_pointer_report_t report;
    remote_pointer_init(&state);
    assert(remote_pointer_handles(MI_KEY_RIGHT));
    assert(!remote_pointer_handles(MI_KEY_VOICE));
    assert(!remote_pointer_handles(MI_KEY_VOL_UP));

    assert(remote_pointer_key(&state, MI_KEY_RIGHT, true, 100, &report));
    assert(report.x == 1 && report.y == 0 && report.buttons == 0);
    assert(!remote_pointer_tick(&state, 119, &report));
    assert(remote_pointer_tick(&state, 120, &report));
    assert(report.x == 1);
    assert(remote_pointer_tick(&state, 1100, &report));
    assert(report.x == 16);
    assert(remote_pointer_tick(&state, 5100, &report));
    assert(report.x == 16);
    assert(!remote_pointer_key(&state, MI_KEY_RIGHT, false, 5101, &report));
    assert(!remote_pointer_tick(&state, 5200, &report));

    assert(remote_pointer_key(&state, MI_KEY_OK, true, 5300, &report));
    assert(report.buttons == 1);
    assert(remote_pointer_key(&state, MI_KEY_OK, false, 5350, &report));
    assert(report.buttons == 0);
    assert(remote_pointer_key(&state, MI_KEY_MENU, true, 5400, &report));
    assert(report.buttons == 2);
    assert(remote_pointer_key(&state, MI_KEY_MENU, false, 5450, &report));
    assert(report.buttons == 0);

    assert(remote_pointer_key(&state, MI_KEY_HOME, true, 5500, &report));
    assert(report.buttons == 1 && state.dragging);
    assert(!remote_pointer_key(&state, MI_KEY_HOME, false, 5550, &report));
    assert(remote_pointer_key(&state, MI_KEY_DOWN, true, 5600, &report));
    assert(report.buttons == 1 && report.y == 1);
    assert(!remote_pointer_key(&state, MI_KEY_DOWN, false, 5650, &report));
    assert(remote_pointer_key(&state, MI_KEY_HOME, true, 5700, &report));
    assert(report.buttons == 0 && !state.dragging);

    assert(!remote_pointer_key(&state, MI_KEY_TV, true, 5800, &report));
    assert(!remote_pointer_key(&state, MI_KEY_TV, false, 5900, &report));
    assert(state.scrolling);
    assert(remote_pointer_key(&state, MI_KEY_UP, true, 6000, &report));
    assert(report.x == 0 && report.y == 0 && report.wheel == 1);
    assert(!remote_pointer_tick(&state, 6119, &report));
    assert(remote_pointer_tick(&state, 6120, &report));
    assert(report.wheel == 1);
    assert(remote_pointer_tick(&state, 7000, &report));
    assert(!remote_pointer_tick(&state, 7039, &report));
    assert(remote_pointer_tick(&state, 7040, &report));
    assert(report.wheel == 1);
    assert(!remote_pointer_key(&state, MI_KEY_UP, false, 7050, &report));
    assert(remote_pointer_key(&state, MI_KEY_LEFT, true, 7100, &report));
    assert(report.pan == -1 && report.wheel == 0);
    assert(!remote_pointer_key(&state, MI_KEY_LEFT, false, 7150, &report));

    assert(!remote_pointer_key(&state, MI_KEY_TV, true, 7200, &report));
    assert(!remote_pointer_key(&state, MI_KEY_TV, false, 7900, &report));
    assert(state.scrolling);
    assert(remote_pointer_key(&state, MI_KEY_HOME, true, 8000, &report));
    assert(state.dragging && !state.scrolling);
    assert(remote_pointer_reset(&state, &report));
    assert(report.buttons == 0 && !state.dragging && !state.scrolling);
    assert(!remote_pointer_tick(&state, 8200, &report));

    assert(remote_pointer_key(&state, MI_KEY_LEFT, true, UINT32_MAX - 10, &report));
    assert(!remote_pointer_tick(&state, 8, &report));
    assert(remote_pointer_tick(&state, 9, &report));
    assert(report.x == -1);
    puts("remote_pointer: movement, acceleration, release, buttons, drag, scroll and clock wrap passed");
    return 0;
}
