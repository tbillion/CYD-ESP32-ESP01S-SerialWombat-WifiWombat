#pragma once

#include "../../config/system_config.h"

#if DISPLAY_SUPPORT_ENABLED

#include <lvgl.h>

// Status bar UI objects
extern lv_obj_t* g_status_bar;
extern lv_obj_t* g_lbl_time;
extern lv_obj_t* g_lbl_rssi;
extern lv_obj_t* g_lbl_i2c;
extern lv_obj_t* g_lbl_batt;
extern lv_obj_t* g_lbl_messages;

// Build the status bar UI
void buildStatusBar();

// Update message badge
void updateMessageBadge();

#endif // DISPLAY_SUPPORT_ENABLED
