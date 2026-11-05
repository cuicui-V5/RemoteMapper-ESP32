#pragma once
#include <Arduino.h>
bool ha_configure(const String& url, const String& token);
String ha_get_config_json();
bool ha_test_connection(String* message = nullptr);
void ha_clear_entity_lists();
bool ha_set_entity_list(uint8_t slot, const String& entities);
String ha_get_entity_list(uint8_t slot);
