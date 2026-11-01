#define _POSIX_C_SOURCE 200809L
#include "keymap/remote_pointer.h"
#include "keymap/key_definitions.h"
#include "class/hid/hid_device.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <glob.h>
#include <libevdev/libevdev.h>
#include <linux/uhid.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int transport;
static struct libevdev *input;
static remote_pointer_t pointer;
static uint8_t previous_buttons;
static unsigned reports;
static const char name[] = "RemoteMapper virtual mouse verification";
static const uint8_t descriptor[] = {
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(2))
};

static void write_event(const struct uhid_event *event) {
    assert(write(transport, event, sizeof(*event)) == sizeof(*event));
}

static void open_input(void) {
    struct timespec pause = {.tv_nsec = 10000000};
    for (unsigned attempt = 0; attempt < 500; ++attempt) {
        glob_t matches;
        int result = glob("/dev/input/event*", 0, NULL, &matches);
        assert(result == 0 || result == GLOB_NOMATCH);
        for (size_t index = 0; index < matches.gl_pathc; ++index) {
            int fd = open(matches.gl_pathv[index], O_RDONLY | O_NONBLOCK);
            assert(fd >= 0);
            struct libevdev *device;
            assert(libevdev_new_from_fd(fd, &device) == 0);
            if (strcmp(libevdev_get_name(device), name) == 0) {
                input = device;
                globfree(&matches);
                assert(libevdev_has_event_code(input, EV_REL, REL_X));
                assert(libevdev_has_event_code(input, EV_REL, REL_WHEEL));
                assert(libevdev_has_event_code(input, EV_REL, REL_HWHEEL));
                return;
            }
            libevdev_free(device);
            close(fd);
        }
        globfree(&matches);
        nanosleep(&pause, NULL);
    }
    assert(!"Linux did not create the virtual input device within five seconds");
}

static void create_device(void) {
    transport = open("/dev/uhid", O_RDWR | O_NONBLOCK);
    assert(transport >= 0);
    struct uhid_event event = {.type = UHID_CREATE2};
    memcpy(event.u.create2.name, name, sizeof(name));
    event.u.create2.bus = BUS_USB;
    event.u.create2.vendor = 0x303a;
    event.u.create2.product = 0x4001;
    event.u.create2.rd_size = sizeof(descriptor);
    memcpy(event.u.create2.rd_data, descriptor, sizeof(descriptor));
    write_event(&event);
    struct pollfd ready = {.fd = transport, .events = POLLIN};
    assert(poll(&ready, 1, 5000) == 1);
    assert(read(transport, &event, sizeof(event)) > 0);
    assert(event.type == UHID_START);
    assert(event.u.start.dev_flags & UHID_DEV_NUMBERED_INPUT_REPORTS);
    open_input();
}

static void verify_report(const remote_pointer_report_t *report) {
    hid_mouse_report_t payload = {
        report->buttons, report->x, report->y, report->wheel, report->pan
    };
    struct uhid_event event = {.type = UHID_INPUT2};
    event.u.input2.size = 1 + sizeof(payload);
    event.u.input2.data[0] = 2;
    memcpy(event.u.input2.data + 1, &payload, sizeof(payload));
    write_event(&event);
    int x = 0, y = 0, wheel = 0, pan = 0;
    bool left = false, right = false;
    bool changed = previous_buttons != report->buttons || report->x || report->y || report->wheel || report->pan;
    if (changed) {
        struct pollfd ready = {.fd = libevdev_get_fd(input), .events = POLLIN};
        assert(poll(&ready, 1, 2000) == 1);
        for (;;) {
            struct input_event received;
            assert(libevdev_next_event(input, LIBEVDEV_READ_FLAG_NORMAL, &received) == LIBEVDEV_READ_STATUS_SUCCESS);
            if (received.type == EV_REL) {
                if (received.code == REL_X) x += received.value;
                else if (received.code == REL_Y) y += received.value;
                else if (received.code == REL_WHEEL) wheel += received.value;
                else if (received.code == REL_HWHEEL) pan += received.value;
                else assert(received.code == REL_WHEEL_HI_RES || received.code == REL_HWHEEL_HI_RES);
            } else if (received.type == EV_KEY) {
                if (received.code == BTN_LEFT) {
                    assert(received.value == !!(report->buttons & 1));
                    left = true;
                } else if (received.code == BTN_RIGHT) {
                    assert(received.value == !!(report->buttons & 2));
                    right = true;
                } else assert(!"Unexpected mouse button");
            } else if (received.type == EV_SYN) {
                assert(received.code == SYN_REPORT);
                break;
            } else assert(received.type == EV_MSC);
        }
    }
    assert(x == report->x && y == report->y);
    assert(wheel == report->wheel && pan == report->pan);
    assert(left == !!((previous_buttons ^ report->buttons) & 1));
    assert(right == !!((previous_buttons ^ report->buttons) & 2));
    assert(libevdev_get_event_value(input, EV_KEY, BTN_LEFT) == !!(report->buttons & 1));
    assert(libevdev_get_event_value(input, EV_KEY, BTN_RIGHT) == !!(report->buttons & 2));
    previous_buttons = report->buttons;
    reports++;
}

static void key(uint8_t code, bool pressed, uint32_t now) {
    remote_pointer_report_t report;
    if (remote_pointer_key(&pointer, code, pressed, now, &report)) verify_report(&report);
}

static void tick(uint32_t now) {
    remote_pointer_report_t report;
    if (remote_pointer_tick(&pointer, now, &report)) verify_report(&report);
}

static void disconnect_pointer(void) {
    remote_pointer_report_t report;
    if (remote_pointer_reset(&pointer, &report)) verify_report(&report);
    assert(!libevdev_get_event_value(input, EV_KEY, BTN_LEFT));
    assert(!libevdev_get_event_value(input, EV_KEY, BTN_RIGHT));
}

int main(void) {
    create_device();
    remote_pointer_init(&pointer);
    uint32_t now = UINT32_MAX - 500;
    for (unsigned round = 0; round < 200; ++round) {
        const uint8_t directions[] = {MI_KEY_UP, MI_KEY_DOWN, MI_KEY_LEFT, MI_KEY_RIGHT};
        for (unsigned direction = 0; direction < 4; ++direction) {
            key(directions[direction], true, now);
            for (unsigned elapsed = 20; elapsed <= 1500; elapsed += 20) tick(now + elapsed);
            now += 1501;
            key(directions[direction], false, now);
            tick(now + 20);
        }
        for (unsigned click = 0; click < 2; ++click) {
            key(MI_KEY_OK, true, now += 50);
            key(MI_KEY_OK, false, now += 30);
        }
        key(MI_KEY_MENU, true, now += 50);
        key(MI_KEY_MENU, false, now += 30);
        key(MI_KEY_HOME, true, now += 30);
        key(MI_KEY_HOME, false, now += 30);
        key(MI_KEY_RIGHT, true, now += 30);
        tick(now += 500);
        key(MI_KEY_RIGHT, false, now += 1);
        disconnect_pointer();
        tick(now += 100);
        key(MI_KEY_TV, true, now += 30);
        key(MI_KEY_TV, false, now += 100);
        for (unsigned direction = 0; direction < 4; ++direction) {
            key(directions[direction], true, now);
            for (unsigned elapsed = 20; elapsed <= 1500; elapsed += 20) tick(now + elapsed);
            now += 1501;
            key(directions[direction], false, now);
        }
        key(MI_KEY_HOME, true, now += 30);
        key(MI_KEY_HOME, false, now += 30);
        key(MI_KEY_LEFT, true, now += 30);
        key(MI_KEY_LEFT, false, now += 30);
        key(MI_KEY_HOME, true, now += 30);
        key(MI_KEY_HOME, false, now += 30);
        disconnect_pointer();
    }
    struct uhid_event end = {.type = UHID_DESTROY};
    write_event(&end);
    close(libevdev_get_fd(input));
    libevdev_free(input);
    close(transport);
    printf("Linux UHID verified %u reports: movement, acceleration, double-click sequences, right click, drag release, both scroll axes and clock wrap\n", reports);
    return 0;
}
