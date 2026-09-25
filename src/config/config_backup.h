#pragma once

#include <Arduino.h>

// Export current user configuration as a JSON document.
//   full=false -> passwords are redacted ("********") -> safe to share
//   full=true  -> includes Wi-Fi/AP passwords in plaintext (use with care)
String config_backup_export(bool full);

// Restore configuration from a previously exported JSON document.
// Returns an empty string on success, or a human-readable error message.
String config_backup_import(const String& json);