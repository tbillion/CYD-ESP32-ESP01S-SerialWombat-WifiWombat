#pragma once

#include "../../config/system_config.h"

#if DISPLAY_SUPPORT_ENABLED

#  include <lvgl.h>

// LVGL initialization state
extern bool g_lvgl_ready;

// First boot state
extern bool g_firstboot_active;
extern bool g_firstboot_interacted;
extern uint32_t g_firstboot_t0;

// I2C traffic counters (referenced from elsewhere in main .ino)
extern volatile uint32_t g_i2c_tx_count;
extern volatile uint32_t g_i2c_rx_count;

// LVGL init and update
bool lvglInitIfEnabled();
void lvglTickAndUpdate();

#endif  // DISPLAY_SUPPORT_ENABLED
