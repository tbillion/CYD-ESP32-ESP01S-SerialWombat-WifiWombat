#include "statusbar.h"

#if DISPLAY_SUPPORT_ENABLED

#include "../../config/defaults.h"
#include "../../core/messages/message_center.h"
#include "../screens/messages_screen.h"

lv_obj_t* g_status_bar = nullptr;
lv_obj_t* g_lbl_time = nullptr;
lv_obj_t* g_lbl_rssi = nullptr;
lv_obj_t* g_lbl_i2c  = nullptr;
lv_obj_t* g_lbl_batt = nullptr;
lv_obj_t* g_lbl_messages = nullptr;

static void onMessageBadgeClicked(lv_event_t* e) {
  (void)e;
  showMessagesScreen();
}

void buildStatusBar() {
  g_status_bar = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_status_bar, lv_pct(100), 24);
  lv_obj_align(g_status_bar, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_clear_flag(g_status_bar, LV_OBJ_FLAG_SCROLLABLE);

  g_lbl_time = lv_label_create(g_status_bar);
  lv_label_set_text(g_lbl_time, "--:--");
  lv_obj_align(g_lbl_time, LV_ALIGN_LEFT_MID, 6, 0);

  g_lbl_rssi = lv_label_create(g_status_bar);
  lv_label_set_text(g_lbl_rssi, "WiFi: --");
  lv_obj_align(g_lbl_rssi, LV_ALIGN_CENTER, 0, 0);

  g_lbl_i2c = lv_label_create(g_status_bar);
  lv_label_set_text(g_lbl_i2c, "I2C: 0/0");
  lv_obj_align(g_lbl_i2c, LV_ALIGN_RIGHT_MID, -6, 0);

  if (BATTERY_ADC_PIN >= 0) {
    g_lbl_batt = lv_label_create(g_status_bar);
    lv_label_set_text(g_lbl_batt, "Bat: --%");
    lv_obj_align(g_lbl_batt, LV_ALIGN_RIGHT_MID, -120, 0);
  }

  g_lbl_messages = lv_label_create(g_status_bar);
  lv_label_set_text(g_lbl_messages, "");
  lv_obj_align(g_lbl_messages, LV_ALIGN_LEFT_MID, 60, 0);
  lv_obj_add_flag(g_lbl_messages, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_lbl_messages, onMessageBadgeClicked, LV_EVENT_CLICKED, nullptr);
}

void updateMessageBadge() {
  if (!g_lbl_messages) return;

  MessageSummary summary = MessageCenter::getInstance().getSummary();
  
  if (summary.active_count == 0) {
    lv_label_set_text(g_lbl_messages, "");
    return;
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "🔔 %u", summary.active_count);
  lv_label_set_text(g_lbl_messages, buf);

  lv_color_t color;
  switch (summary.highest_active_severity) {
    case MessageSeverity::ERROR:
      color = lv_color_hex(0xFF0000);
      break;
    case MessageSeverity::WARN:
      color = lv_color_hex(0xFF8800);
      break;
    default:
      color = lv_color_hex(0x0088FF);
      break;
  }
  lv_obj_set_style_text_color(g_lbl_messages, color, 0);
}
#endif // DISPLAY_SUPPORT_ENABLED
