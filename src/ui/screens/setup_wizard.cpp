#include "setup_wizard.h"

#if DISPLAY_SUPPORT_ENABLED

#include "../lvgl_wrapper.h"
#include "../components/statusbar.h"
#include "../../config/system_config.h"
#include <LittleFS.h>
#include <ESP.h>

#if SD_SUPPORT_ENABLED
#define CYD_USE_SDFAT 1
#include <SdFat.h>
#endif

// Forward declarations for functions still in main .ino
extern CydModel strToModel(const String& s);
extern void applyModelPreset(SystemConfig &cfg);
extern bool saveConfig(const SystemConfig &cfg);

#if SD_SUPPORT_ENABLED
// SD abstraction types and functions (defined in main .ino)
typedef FsFile SDFile;
extern bool sdMount();
extern SDFile sdOpen(const char* path, oflag_t flags);
extern bool sdOpenNext(SDFile &dir, SDFile &out);
extern bool sdIsDir(const char* path);
extern bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path);
#endif

// Setup wizard UI objects
lv_obj_t* g_dd_model = nullptr;
lv_obj_t* g_btn_next = nullptr;
lv_obj_t* g_file_list = nullptr;
String g_sd_cwd = "/";

// Forward declarations
static void onModelEvent(lv_event_t * e);
static void onNextEvent(lv_event_t * e);
#if SD_SUPPORT_ENABLED
static void onFileListEvent(lv_event_t * e);
static bool sdListDirToLV(const String& dir);
#endif

static void onModelEvent(lv_event_t * e) {
  (void)e;
  g_firstboot_interacted = true;
}

static void onNextEvent(lv_event_t * e) {
  (void)e;
  g_firstboot_interacted = true;
  if (!g_dd_model) return;
  char buf[32] = {0};
  lv_dropdown_get_selected_str(g_dd_model, buf, sizeof(buf));
  g_cfg.model = strToModel(String(buf));
  applyModelPreset(g_cfg);
  // Proceed to SD splash picker if SD is enabled, otherwise just commit and reboot.
#if SD_SUPPORT_ENABLED
  firstBootShowSplashPicker();
#else
  g_cfg.configured = true;
  saveConfig(g_cfg);
  ESP.restart();
#endif
}

void firstBootShowModelSelect() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "First Boot: Select CYD Model");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

  g_dd_model = lv_dropdown_create(lv_scr_act());
  lv_dropdown_set_options(g_dd_model,
    "2432S028R\n2432S028C\n2432S022C\n2432S032\n3248S035\n4827S043\n8048S050\n8048S070\nS3_GENERIC");
  lv_obj_set_width(g_dd_model, lv_pct(80));
  lv_obj_align(g_dd_model, LV_ALIGN_CENTER, 0, -10);
  lv_obj_add_event_cb(g_dd_model, onModelEvent, LV_EVENT_VALUE_CHANGED, nullptr);

  g_btn_next = lv_btn_create(lv_scr_act());
  lv_obj_set_width(g_btn_next, 160);
  lv_obj_align(g_btn_next, LV_ALIGN_CENTER, 0, 60);
  lv_obj_add_event_cb(g_btn_next, onNextEvent, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* lbl = lv_label_create(g_btn_next);
  lv_label_set_text(lbl, "Next");
  lv_obj_center(lbl);

  g_firstboot_t0 = millis();
  g_firstboot_active = true;
  g_firstboot_interacted = false;

  buildStatusBar();
}

#if SD_SUPPORT_ENABLED
static void onFileListEvent(lv_event_t * e) {
  g_firstboot_interacted = true;
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  if (!g_file_list) return;
  uint16_t idx = lv_dropdown_get_selected(g_file_list);
  char name[128] = {0};
  lv_dropdown_get_selected_str(g_file_list, name, sizeof(name));
  String sel = String(name);
  if (sel == "..") {
    if (g_sd_cwd != "/") {
      int last = g_sd_cwd.lastIndexOf('/');
      if (last <= 0) g_sd_cwd = "/";
      else g_sd_cwd = g_sd_cwd.substring(0, last);
      sdListDirToLV(g_sd_cwd);
    }
    return;
  }
  String full = (g_sd_cwd == "/") ? ("/" + sel) : (g_sd_cwd + "/" + sel);
  if (sdIsDir(full.c_str())) {
    g_sd_cwd = full;
    sdListDirToLV(g_sd_cwd);
    return;
  }
  // Copy selected file into /assets and reboot
  LittleFS.mkdir("/assets");
  String dest = "/assets/splash";
  int dot = sel.lastIndexOf('.');
  if (dot > 0) dest += sel.substring(dot);
  if (sdCopyToLittleFS(full.c_str(), dest.c_str())) {
    g_cfg.splash_path = dest;
    g_cfg.configured = true;
    saveConfig(g_cfg);
    ESP.restart();
  }
}

static bool sdListDirToLV(const String& dir) {
  if (!sdMount()) return false;

  SDFile d = sdOpen(dir.c_str(), O_RDONLY);
  if (!d) return false;

#if CYD_USE_SDFAT
  if (!d.isDir()) { d.close(); return false; }
#else
  if (!d.isDirectory()) { d.close(); return false; }
#endif

  String opts = "..";
  SDFile e;
  while (sdOpenNext(d, e)) {
    String name;
#if CYD_USE_SDFAT
    char n[96] = {0};
    e.getName(n, sizeof(n));
    name = String(n);
#else
    name = String(e.name());
#endif
    if (name.length() && name != "." && name != "..") {
      opts += "\n" + name;
    }
    e.close();
    delay(0);
  }
  d.close();

  lv_dropdown_set_options(g_file_list, opts.c_str());
  return true;
}

void firstBootShowSplashPicker() {
  lv_obj_clean(lv_scr_act());
  lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);

  lv_obj_t* title = lv_label_create(lv_scr_act());
  lv_label_set_text(title, "Select Splash Image from SD");
  lv_obj_set_style_text_color(title, lv_color_white(), 0);
  lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

  g_file_list = lv_dropdown_create(lv_scr_act());
  lv_obj_set_width(g_file_list, lv_pct(90));
  lv_obj_align(g_file_list, LV_ALIGN_CENTER, 0, 10);
  lv_obj_add_event_cb(g_file_list, onFileListEvent, LV_EVENT_VALUE_CHANGED, nullptr);

  g_sd_cwd = "/";
  sdListDirToLV(g_sd_cwd);

  buildStatusBar();
}
#endif
#endif // DISPLAY_SUPPORT_ENABLED
