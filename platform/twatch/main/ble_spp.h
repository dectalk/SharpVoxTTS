#pragma once

#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*ble_spp_recv_cb_t)(const char *line);

esp_err_t ble_spp_init(ble_spp_recv_cb_t recv_cb);

#ifdef __cplusplus
}
#endif
