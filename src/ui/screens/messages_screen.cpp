#include "messages_screen.h"

#if DISPLAY_SUPPORT_ENABLED

#include "../lvgl_wrapper.h"
#include "../components/statusbar.h"
#include "../../core/messages/message_center.h"

// UI objects
static lv_obj_t* g_tabview = nullptr;
static lv_obj_t* g_tab_active = nullptr;
static lv_obj_t* g_tab_history = nullptr;
static lv_obj_t* g_list_active = nullptr;
static lv_obj_t* g_list_history = nullptr;
static lv_obj_t* g_btn_ack_all = nullptr;
static lv_obj_t* g_btn_clear_history = nullptr;
static lv_obj_t* g_detail_popup = nullptr;
static uint32_t g_current_detail_msg_id = 0;

// Forward declarations
static void populateActiveList();
static void populateHistoryList();
static void onMessageClicked(lv_event_t* e);
static void onAckAllClicked(lv_event_t* e);
static void onClearHistoryClicked(lv_event_t* e);
static void onDetailAckClicked(lv_event_t* e);
static void onDetailCloseClicked(lv_event_t* e);
static const char* severityToIcon(MessageSeverity sev);
static const char* severityToStr(MessageSeverity sev);

static const char* severityToIcon(MessageSeverity sev) {
  switch (sev) {
    case MessageSeverity::ERROR: return "❌";
    case MessageSeverity::WARN:  return "⚠️";
    default:                      return "ℹ️";
  }
}

static const char* severityToStr(MessageSeverity sev) {
  switch (sev) {
    case MessageSeverity::ERROR: return "ERROR";
    case MessageSeverity::WARN:  return "WARN";
    default:                      return "INFO";
  }
}

static void populateActiveList() {
  if (!g_list_active) return;
  
  lv_obj_clean(g_list_active);
  
  const auto& messages = MessageCenter::getInstance().getActiveMessages();
  
  if (messages.empty()) {
    lv_obj_t* lbl = lv_label_create(g_list_active);
    lv_label_set_text(lbl, "No active messages");
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_GREY), 0);
    return;
  }
  
  for (const auto& msg : messages) {
    lv_obj_t* btn = lv_btn_create(g_list_active);
    lv_obj_set_width(btn, lv_pct(95));
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn, onMessageClicked, LV_EVENT_CLICKED, (void*)(uintptr_t)msg.id);
    
    lv_obj_t* lbl = lv_label_create(btn);
    char buf[128];
    uint32_t sec = msg.timestamp / 1000;
    snprintf(buf, sizeof(buf), "%s [%02lu:%02lu:%02lu] %s", 
             severityToIcon(msg.severity),
             (sec / 3600), ((sec / 60) % 60), (sec % 60),
             msg.title.c_str());
    lv_label_set_text(lbl, buf);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl, lv_pct(95));
  }
}

static void populateHistoryList() {
  if (!g_list_history) return;
  
  lv_obj_clean(g_list_history);
  
  const auto& messages = MessageCenter::getInstance().getHistoryMessages();
  
  if (messages.empty()) {
    lv_obj_t* lbl = lv_label_create(g_list_history);
    lv_label_set_text(lbl, "No message history");
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_GREY), 0);
    return;
  }
  
  for (const auto& msg : messages) {
    lv_obj_t* btn = lv_btn_create(g_list_history);
    lv_obj_set_width(btn, lv_pct(95));
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn, onMessageClicked, LV_EVENT_CLICKED, (void*)(uintptr_t)msg.id);
    
    lv_obj_t* lbl = lv_label_create(btn);
    char buf[128];
    uint32_t sec = msg.timestamp / 1000;
    snprintf(buf, sizeof(buf), "%s [%02lu:%02lu:%02lu] %s", 
             severityToIcon(msg.severity),
             (sec / 3600), ((sec / 60) % 60), (sec % 60),
             msg.title.c_str());
    lv_label_set_text(lbl, buf);
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl, lv_pct(95));
  }
}

static void onMessageClicked(lv_event_t* e) {
  uint32_t msg_id = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
  showMessageDetail(msg_id);
}

static void onAckAllClicked(lv_event_t* e) {
  (void)e;
  MessageCenter::getInstance().acknowledgeAll();
  populateActiveList();
  populateHistoryList();
  updateMessageBadge();
}

static void onClearHistoryClicked(lv_event_t* e) {
  (void)e;
  MessageCenter::getInstance().clearHistory();
  populateHistoryList();
}

static void onDetailAckClicked(lv_event_t* e) {
  (void)e;
  MessageCenter::getInstance().acknowledge(g_current_detail_msg_id);
  
  if (g_detail_popup) {
    lv_obj_del(g_detail_popup);
    g_detail_popup = nullptr;
  }
  
  populateActiveList();
  populateHistoryList();
  updateMessageBadge();
}

static void onDetailCloseClicked(lv_event_t* e) {
  (void)e;
  if (g_detail_popup) {
    lv_obj_del(g_detail_popup);
    g_detail_popup = nullptr;
  }
}

void showMessagesScreen() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
  
  buildStatusBar();
  
  g_tabview = lv_tabview_create(lv_scr_act());
  lv_obj_set_size(g_tabview, lv_pct(100), lv_pct(90));
  lv_obj_align(g_tabview, LV_ALIGN_BOTTOM_MID, 0, 0);
  
  g_tab_active = lv_tabview_add_tab(g_tabview, "Active");
  g_tab_history = lv_tabview_add_tab(g_tabview, "History");
  
  // Active tab
  g_list_active = lv_obj_create(g_tab_active);
  lv_obj_set_size(g_list_active, lv_pct(100), lv_pct(80));
  lv_obj_align(g_list_active, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(g_list_active, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_list_active, 5, 0);
  
  g_btn_ack_all = lv_btn_create(g_tab_active);
  lv_obj_set_size(g_btn_ack_all, lv_pct(90), 40);
  lv_obj_align(g_btn_ack_all, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(g_btn_ack_all, onAckAllClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_ack = lv_label_create(g_btn_ack_all);
  lv_label_set_text(lbl_ack, "Acknowledge All");
  lv_obj_center(lbl_ack);
  
  // History tab
  g_list_history = lv_obj_create(g_tab_history);
  lv_obj_set_size(g_list_history, lv_pct(100), lv_pct(80));
  lv_obj_align(g_list_history, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_set_flex_flow(g_list_history, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(g_list_history, 5, 0);
  
  g_btn_clear_history = lv_btn_create(g_tab_history);
  lv_obj_set_size(g_btn_clear_history, lv_pct(90), 40);
  lv_obj_align(g_btn_clear_history, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_add_event_cb(g_btn_clear_history, onClearHistoryClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_clear = lv_label_create(g_btn_clear_history);
  lv_label_set_text(lbl_clear, "Clear History");
  lv_obj_center(lbl_clear);
  
  populateActiveList();
  populateHistoryList();
}

void showMessageDetail(uint32_t msg_id) {
  Message* msg = MessageCenter::getInstance().findMessageById(msg_id);
  if (!msg) return;
  
  g_current_detail_msg_id = msg_id;
  
  if (g_detail_popup) {
    lv_obj_del(g_detail_popup);
  }
  
  g_detail_popup = lv_obj_create(lv_scr_act());
  lv_obj_set_size(g_detail_popup, lv_pct(90), lv_pct(80));
  lv_obj_center(g_detail_popup);
  lv_obj_set_style_bg_color(g_detail_popup, lv_color_hex(0x202020), 0);
  lv_obj_set_style_border_color(g_detail_popup, lv_color_hex(0x606060), 0);
  lv_obj_set_style_border_width(g_detail_popup, 2, 0);
  
  lv_obj_t* container = lv_obj_create(g_detail_popup);
  lv_obj_set_size(container, lv_pct(95), lv_pct(75));
  lv_obj_align(container, LV_ALIGN_TOP_MID, 0, 5);
  lv_obj_set_flex_flow(container, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(container, 8, 0);
  
  char buf[512];
  uint32_t sec = msg->timestamp / 1000;
  snprintf(buf, sizeof(buf),
           "Severity: %s %s\n"
           "Source: %s\n"
           "Code: %s\n"
           "Time: %02lu:%02lu:%02lu\n"
           "Count: %u\n"
           "Title: %s\n"
           "Details: %s",
           severityToIcon(msg->severity),
           severityToStr(msg->severity),
           msg->source.c_str(),
           msg->code.c_str(),
           (sec / 3600), ((sec / 60) % 60), (sec % 60),
           msg->count,
           msg->title.c_str(),
           msg->details.c_str());
  
  lv_obj_t* lbl_detail = lv_label_create(container);
  lv_label_set_text(lbl_detail, buf);
  lv_label_set_long_mode(lbl_detail, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(lbl_detail, lv_pct(95));
  
  lv_obj_t* btn_container = lv_obj_create(g_detail_popup);
  lv_obj_set_size(btn_container, lv_pct(95), 50);
  lv_obj_align(btn_container, LV_ALIGN_BOTTOM_MID, 0, -5);
  lv_obj_set_flex_flow(btn_container, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(btn_container, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(btn_container, LV_OBJ_FLAG_SCROLLABLE);
  
  if (!msg->acknowledged) {
    lv_obj_t* btn_ack = lv_btn_create(btn_container);
    lv_obj_set_size(btn_ack, 120, 40);
    lv_obj_add_event_cb(btn_ack, onDetailAckClicked, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lbl_ack = lv_label_create(btn_ack);
    lv_label_set_text(lbl_ack, "Acknowledge");
    lv_obj_center(lbl_ack);
  }
  
  lv_obj_t* btn_close = lv_btn_create(btn_container);
  lv_obj_set_size(btn_close, 100, 40);
  lv_obj_add_event_cb(btn_close, onDetailCloseClicked, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl_close = lv_label_create(btn_close);
  lv_label_set_text(lbl_close, "Close");
  lv_obj_center(lbl_close);
}

#endif // DISPLAY_SUPPORT_ENABLED
