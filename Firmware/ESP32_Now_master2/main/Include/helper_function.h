#ifndef HELPER_FUNCTION_H
#define HELPER_FUNCTION_H

#include "esp_now.h"

// Function prototypes
void mac_to_string(const uint8_t *mac, char *str);
bool is_slave_discovered(const uint8_t *mac);
void remove_slave(const uint8_t *mac);
void add_new_slave(const uint8_t *mac, const char *name);
void master_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status);

#endif // HELPER_FUNCTION_H
