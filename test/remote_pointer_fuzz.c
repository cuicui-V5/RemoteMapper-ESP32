#include "keymap/remote_pointer.h"
#include "keymap/key_definitions.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    remote_pointer_t pointer;
    remote_pointer_init(&pointer);
    remote_pointer_report_t report;
    uint32_t now = UINT32_MAX - 1000;
    uint8_t held = 0;
    const uint8_t keys[] = {MI_KEY_UP, MI_KEY_DOWN, MI_KEY_LEFT, MI_KEY_RIGHT,
                           MI_KEY_OK, MI_KEY_MENU, MI_KEY_HOME, MI_KEY_TV};
    for (size_t index = 0; index + 3 < size; index += 4) {
        now += data[index + 1] + ((uint32_t)data[index + 2] << 8);
        bool emitted = false;
        switch (data[index] % 4) {
            case 0:
                if (held) {
                    emitted = remote_pointer_key(&pointer, held, false, now, &report);
                    if (emitted) assert(report.x == 0 && report.y == 0);
                }
                held = keys[data[index + 3] % 8];
                emitted = remote_pointer_key(&pointer, held, true, now, &report);
                break;
            case 1:
                emitted = remote_pointer_key(&pointer, held, false, now, &report);
                held = 0;
                break;
            case 2:
                emitted = remote_pointer_tick(&pointer, now, &report);
                break;
            case 3:
                emitted = remote_pointer_reset(&pointer, &report);
                held = 0;
                assert(!pointer.direction && !pointer.buttons && !pointer.dragging && !pointer.scrolling);
                break;
        }
        if (emitted) {
            assert((report.buttons & ~3) == 0);
            assert(abs(report.x) <= 16 && abs(report.y) <= 16);
            assert(abs(report.wheel) <= 1 && abs(report.pan) <= 1);
            assert(!((report.x || report.y) && (report.wheel || report.pan)));
        }
        if (!pointer.direction) assert(!remote_pointer_tick(&pointer, now + 1000, &report));
        assert(!(pointer.scrolling && pointer.dragging));
    }
    remote_pointer_reset(&pointer, &report);
    assert(report.buttons == 0);
    assert(!remote_pointer_tick(&pointer, now + 2000, &report));
    return 0;
}
