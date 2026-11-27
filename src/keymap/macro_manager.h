#pragma once

#include <Arduino.h>
#include <stdint.h>

void macro_clear_scripts(void);
bool macro_set_script(uint8_t slot, const String& script);
String macro_get_script(uint8_t slot);
