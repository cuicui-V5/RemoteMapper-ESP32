#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool wol_manager_parse_mac(const char* mac_str, uint8_t out_mac[6]);
void wol_manager_mac_to_str(const uint8_t mac[6], char* out_str, size_t out_size);
bool wol_manager_send(const uint8_t mac[6], uint16_t port);
bool wol_manager_send_str(const char* mac_str, uint16_t port);

#ifdef __cplusplus
}
#endif
