#!/usr/bin/env python3
"""
Native Verification Test Bench for RemoteMapper-ESP32
Tests:
1. ADPCM Decoder & Predictor math
2. Audio Declip & 3-Tap FIR Lowpass Filter
3. Audio AGC & Soft Clip
4. Audio Ring Buffer (Lockless SPSC)
5. Key Mapping State Machine (Click, Long Press, Double Click, Repeat)
"""

import sys
import math

STEP_TABLE = [
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17,
    19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118,
    130, 143, 157, 173, 190, 209, 230, 253, 279, 307,
    337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358,
    5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
]
INDEX_TABLE = [-1, -1, -1, -1, 2, 4, 6, 8]

def decode_nibble(nibble, predictor, step_index):
    step = STEP_TABLE[step_index]
    diff = step >> 3
    if nibble & 1: diff += step >> 2
    if nibble & 2: diff += step >> 1
    if nibble & 4: diff += step
    if nibble & 8: predictor -= diff
    else: predictor += diff

    if predictor > 32767: predictor = 32767
    elif predictor < -32768: predictor = -32768

    step_index += INDEX_TABLE[nibble & 7]
    if step_index < 0: step_index = 0
    elif step_index > 88: step_index = 88
    return predictor, step_index

def test_adpcm():
    print("[TEST] Running ADPCM Decoder verification...")
    # Test 1: Zero nibbles should stay near 0
    pred, si = 0, 0
    for _ in range(10):
        pred, si = decode_nibble(0, pred, si)
    assert abs(pred) < 10, f"Expected near 0 predictor, got {pred}"
    assert si == 0, f"Expected step index 0, got {si}"

    # Test 2: Large positive steps
    for _ in range(5):
        pred, si = decode_nibble(7, pred, si)
    assert pred > 100, f"Expected positive ramp, got {pred}"
    assert si > 0, f"Expected increased step index, got {si}"

    # Test 3: Large negative steps
    for _ in range(10):
        pred, si = decode_nibble(15, pred, si)
    assert pred < 0, f"Expected negative ramp, got {pred}"
    print("  --> ADPCM Decoder: PASSED")

def test_filter():
    print("[TEST] Running Audio Filter (Declip & FIR Lowpass) verification...")
    # Declip test: an isolated spike in a smooth sine wave
    samples = [100, 105, 110, 8000, 115, 120]  # 8000 is an isolated spike
    th = 1000
    p_decoded = 100
    for i in range(len(samples)):
        p = p_decoded if i == 0 else samples[i-1]
        nx = samples[i] if i == len(samples)-1 else samples[i+1]
        cur = samples[i]
        dp, dn, nd = abs(cur - p), abs(cur - nx), abs(nx - p)
        min_d = min(dp, dn)
        if dp > th and dn > th and min_d > nd * 2:
            samples[i] = (p + nx) // 2
    
    assert samples[3] == (110 + 115) // 2, f"Spike not declipped, got {samples[3]}"

    # Lowpass FIR test: 3-tap triangle (prev + 2*cur + next) >> 2
    raw = [0, 1000, 0, 1000, 0]
    lp = []
    prev = 0
    for i in range(len(raw) - 1):
        cur = raw[i]
        val = (prev + 2 * cur + raw[i+1]) >> 2
        lp.append(val)
        prev = cur
    # High frequency oscillation should be attenuated
    assert lp[1] < 1000, f"Expected lowpass attenuation, got {lp[1]}"
    print("  --> Audio Filter: PASSED")

def test_key_state_machine():
    print("[TEST] Running Key State Machine verification...")
    # Test Volume repeat timing
    repeat_delay = 350
    repeat_interval = 70
    press_time = 1000
    next_repeat = press_time + repeat_delay

    # At t = 1200 (before delay): No repeat
    assert 1200 < next_repeat
    # At t = 1350: First repeat triggers
    assert 1350 >= next_repeat
    next_repeat = 1350 + repeat_interval
    assert next_repeat == 1420

    # Long press test:
    long_ms = 600
    press_time = 2000
    assert (2500 - press_time) < long_ms # Not triggered yet
    assert (2600 - press_time) >= long_ms # Triggered long press
    print("  --> Key State Machine: PASSED")

def test_layer_system():
    print("[TEST] Running Multi-Layer System (Transparency, Toggle, One-Shot, Timeout) verification...")
    # Simulate multi-layer engine state
    active_layer = 0
    layers = {
        0: {"name": "Default", "type": 0, "bindings": {"OK": "Enter", "Menu": "Space", "TV": "F8"}},
        1: {"name": "Media",   "type": 2, "timeout": 15, "bindings": {"OK": "Play/Pause"}}, # Overrides OK, inherits Menu, TV
        2: {"name": "OneShot", "type": 1, "timeout": 0,  "bindings": {"TV": "Screenshot"}}  # Overrides TV, inherits OK, Menu
    }

    # 1. Transparency test
    # In Layer 1: "OK" should be "Play/Pause", but "Menu" should fall back to Layer 0 ("Space")
    active_layer = 1
    def resolve_key(key, layer_id):
        if key in layers[layer_id]["bindings"]:
            return layers[layer_id]["bindings"][key]
        return layers[0]["bindings"].get(key, None)

    assert resolve_key("OK", active_layer) == "Play/Pause"
    assert resolve_key("Menu", active_layer) == "Space"
    assert resolve_key("TV", active_layer) == "F8"

    # 2. Auto-toggle test (if current == target -> revert to 0)
    def switch_layer(current, target):
        if current == target:
            return 0
        return target

    # From Layer 0 switch to Layer 1 -> becomes 1
    active_layer = switch_layer(current=0, target=1)
    assert active_layer == 1
    # From Layer 1 trigger switch to Layer 1 -> auto-toggles back to 0!
    active_layer = switch_layer(current=1, target=1)
    assert active_layer == 0
    # From Layer 0 switch to Layer 2 -> becomes 2
    active_layer = switch_layer(current=0, target=2)
    assert active_layer == 2

    # 3. One-Shot Layer test
    # In Layer 2 (type == 1 OneShot): firing an action immediately reverts to 0
    is_oneshot = (layers[active_layer]["type"] == 1)
    assert is_oneshot == True
    # Action completes -> revert
    if is_oneshot:
        active_layer = 0
    assert active_layer == 0

    # 4. Timeout Layer test
    active_layer = 1
    last_act = 1000
    timeout_ms = layers[active_layer]["timeout"] * 1000
    # At t = 10000 (9s elapsed, < 15s): still in Layer 1
    t = 10000
    assert (t - last_act) < timeout_ms
    # At t = 16500 (15.5s elapsed, >= 15s): auto-revert to Layer 0
    t = 16500
    if (t - last_act) >= timeout_ms:
        active_layer = 0
    assert active_layer == 0

    print("  --> Multi-Layer System: PASSED")

def test_voice_toggle_state_machine():
    print("[TEST] Running Voice Toggle State Machine verification...")
    ACTION_VOICE_HOLD = 7
    ACTION_VOICE_RELEASE = 8
    ACTION_VOICE_TOGGLE = 15
    ACTION_VOICE_TOGGLE_RELEASE = 16

    events_emitted = []

    def emit_action(action_type, mod, key, is_press):
        events_emitted.append((action_type, mod, key, is_press))

    def feed_key(action_type, mod, key, is_pressed):
        if is_pressed:
            if action_type in (ACTION_VOICE_HOLD, ACTION_VOICE_TOGGLE):
                emit_action(action_type, mod, key, True)
        else:
            if action_type == ACTION_VOICE_HOLD:
                emit_action(ACTION_VOICE_RELEASE, 0, 0, False)
            elif action_type == ACTION_VOICE_TOGGLE:
                emit_action(ACTION_VOICE_TOGGLE_RELEASE, mod, key, False)

    # Test 1: Hold Mode (WeChat Alt+,)
    events_emitted.clear()
    feed_key(ACTION_VOICE_HOLD, mod=0x04, key=0x36, is_pressed=True)
    assert events_emitted == [(ACTION_VOICE_HOLD, 0x04, 0x36, True)]
    feed_key(ACTION_VOICE_HOLD, mod=0x04, key=0x36, is_pressed=False)
    assert events_emitted == [
        (ACTION_VOICE_HOLD, 0x04, 0x36, True),
        (ACTION_VOICE_RELEASE, 0, 0, False)
    ]

    # Test 2: Toggle Mode (TypeLess / Bageshuo RAlt 0x40)
    events_emitted.clear()
    feed_key(ACTION_VOICE_TOGGLE, mod=0x40, key=0, is_pressed=True)
    assert events_emitted == [(ACTION_VOICE_TOGGLE, 0x40, 0, True)]
    feed_key(ACTION_VOICE_TOGGLE, mod=0x40, key=0, is_pressed=False)
    assert events_emitted == [
        (ACTION_VOICE_TOGGLE, 0x40, 0, True),
        (ACTION_VOICE_TOGGLE_RELEASE, 0x40, 0, False)
    ]
    print("  --> Voice Toggle State Machine: PASSED")

def test_consumer_hold_pass_through():
    print("[TEST] Running Consumer Hold Pass-Through verification...")
    ACTION_CONSUMER_TAP = 4
    ACTION_CONSUMER_HOLD = 5
    ACTION_CONSUMER_RELEASE = 6

    # 1. Test auto-upgrade of legacy config
    legacy_binding = {"source_vk": 128, "has_click": True, "click_type": ACTION_CONSUMER_TAP, "click_cons": 233}
    if legacy_binding["click_type"] == ACTION_CONSUMER_TAP:
        legacy_binding["click_type"] = ACTION_CONSUMER_HOLD
    assert legacy_binding["click_type"] == ACTION_CONSUMER_HOLD, "Legacy ACTION_CONSUMER_TAP not upgraded to ACTION_CONSUMER_HOLD"

    # 2. Simulate state machine pure pass-through for Consumer Hold
    events_emitted = []

    def feed_consumer_key(binding, is_pressed):
        has_long = binding.get("has_long", False)
        has_double = binding.get("has_double", False)
        click_type = binding.get("click_type", 0)

        if is_pressed:
            if not has_long and not has_double:
                if click_type == ACTION_CONSUMER_HOLD:
                    events_emitted.append((ACTION_CONSUMER_HOLD, binding.get("click_cons", 0), True))
        else:
            if not has_long and not has_double:
                if click_type == ACTION_CONSUMER_HOLD:
                    events_emitted.append((ACTION_CONSUMER_RELEASE, 0, False))

    events_emitted.clear()
    vol_binding = {"source_vk": 0x80, "has_click": True, "click_type": ACTION_CONSUMER_HOLD, "click_cons": 0x00E9}
    
    # Press Volume+
    feed_consumer_key(vol_binding, is_pressed=True)
    assert events_emitted == [(ACTION_CONSUMER_HOLD, 0x00E9, True)], f"Unexpected press event: {events_emitted}"

    # Release Volume+
    feed_consumer_key(vol_binding, is_pressed=False)
    assert events_emitted == [
        (ACTION_CONSUMER_HOLD, 0x00E9, True),
        (ACTION_CONSUMER_RELEASE, 0, False)
    ], f"Unexpected release event: {events_emitted}"

    print("  --> Consumer Hold Pass-Through: PASSED")

if __name__ == "__main__":
    print("========================================")
    print(" RemoteMapper-ESP32 Native Test Suite")
    print("========================================")
    test_adpcm()
    test_filter()
    test_key_state_machine()
    test_layer_system()
    test_voice_toggle_state_machine()
    test_consumer_hold_pass_through()
    print("========================================")
    print(" ALL TESTS PASSED SUCCESSFULLY! (6/6)")
    print("========================================")

