#pragma once

#include "../../config/system_config.h"

#if DISPLAY_SUPPORT_ENABLED

#include <lvgl.h>
#include <Arduino.h>

// Setup wizard UI objects
extern lv_obj_t* g_dd_model;
extern lv_obj_t* g_btn_next;
extern lv_obj_t* g_file_list;
extern String g_sd_cwd;

// Setup wizard screens
void firstBootShowModelSelect();

#if SD_SUPPORT_ENABLED
void firstBootShowSplashPicker();
#endif

#endif // DISPLAY_SUPPORT_ENABLED
