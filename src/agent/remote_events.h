#pragma once

#include "event_journal.h"
#include <Arduino.h>

void remote_events_init(void);
bool remote_events_record(uint8_t key, bool pressed, remote_event_source_t source);
String remote_events_command(const String &arguments);
