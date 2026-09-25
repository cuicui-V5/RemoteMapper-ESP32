#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CONFIG_NAMESPACE       "app_conf"
#define CONFIG_KEY_VERSION     "config_version"
#define CONFIG_VERSION_CURRENT 1

// Read the currently stored config schema version (0 if absent / legacy)
uint32_t config_manager_get_version(void);

// Write the config schema version
bool config_manager_set_version(uint32_t version);

// Migrate stored config from its current version up to CONFIG_VERSION_CURRENT.
// Migrations only ADD defaults for newly introduced keys and never overwrite
// existing user settings, never wipe anything.
void config_manager_init(void);

#ifdef __cplusplus
}
#endif