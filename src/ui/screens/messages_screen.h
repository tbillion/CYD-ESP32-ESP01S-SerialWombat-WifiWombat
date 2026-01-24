#pragma once

#include "../../config/system_config.h"

#if DISPLAY_SUPPORT_ENABLED

#include <lvgl.h>

// Show the messages screen
void showMessagesScreen();

// Show message detail popup
void showMessageDetail(uint32_t msg_id);

#endif // DISPLAY_SUPPORT_ENABLED
