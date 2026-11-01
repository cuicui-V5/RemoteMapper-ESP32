#pragma once

#include "agent/event_journal.h"
#include <Arduino.h>

void mac_pointer_init();
bool mac_pointer_event(uint8_t key, bool pressed, remote_event_source_t source, bool changed);
void mac_pointer_tick();
void mac_pointer_reset();
uint32_t mac_pointer_color();
String mac_pointer_status();
