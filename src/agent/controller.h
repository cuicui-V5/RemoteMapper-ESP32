#pragma once

#include <Arduino.h>
#include "event_journal.h"

void agent_controller_init();
void agent_controller_tick();
bool agent_controller_mac_mode();
bool agent_controller_event(uint8_t key, bool pressed, remote_event_source_t source, bool changed);
String agent_controller_command(const String &arguments);
