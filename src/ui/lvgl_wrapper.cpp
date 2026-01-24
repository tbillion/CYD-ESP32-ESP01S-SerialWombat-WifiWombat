#include "lvgl_wrapper.h"

#if DISPLAY_SUPPORT_ENABLED

#include "../../hal/display/lgfx_display.h"
#include "components/statusbar.h"
#include <WiFi.h>
#include <time.h>
#include <ESP.h>

// Forward declarations for functions still in main .ino
extern bool saveConfig(const SystemConfig &cfg);

// LVGL v8/v9 compatibility layer
#if LVGL_VERSION_MAJOR >= 9
// ===================== LVGL v9 =====================
lv_color_t* g_lv_buf1 = nullptr;
lv_color_t* g_lv_buf2 = nullptr;
lv_display_t* g_lv_disp = nullptr;
lv_indev_t* g_lv_indev = nullptr;

static void lv_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.writePixels((lgfx::rgb565_t*)px_map, w * h, true);
  lcd.endWrite();
  lv_display_flush_ready(disp);
}

static void lv_touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
  (void)indev;
  uint16_t x, y;
  if (lcd.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#else
// ===================== LVGL v8 =====================
lv_disp_draw_buf_t g_lv_drawbuf;
lv_color_t *g_lv_buf1 = nullptr;
lv_color_t *g_lv_buf2 = nullptr;
lv_disp_drv_t g_lv_disp_drv;
lv_indev_drv_t g_lv_indev_drv;
lv_disp_t* g_lv_disp = nullptr;
lv_indev_t* g_lv_indev = nullptr;

static void lv_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);
  lcd.startWrite();
  lcd.setAddrWindow(area->x1, area->y1, w, h);
  lcd.writePixels((lgfx::rgb565_t*)&color_p->full, w*h, true);
  lcd.endWrite();
  lv_disp_flush_ready(disp);
}

static void lv_touch_read_cb(lv_indev_drv_t * indev_driver, lv_indev_data_t * data) {
  (void)indev_driver;
  uint16_t x, y;
  if (lcd.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = x;
    data->point.y = y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

#endif
// LVGL initialization and first boot state
bool g_lvgl_ready = false;
bool g_firstboot_active = false;
bool g_firstboot_interacted = false;
uint32_t g_firstboot_t0 = 0;

bool lvglInitIfEnabled() {
  if (!g_cfg.display_enable || !g_cfg.lvgl_enable) return false;
  if (!lcd.beginFromConfig(g_cfg)) return false;

  lv_init();

  uint16_t w = lcd.width();
  uint16_t h = lcd.height();
  size_t buf_px = (size_t)w * 40; // 40 lines

#if defined(BOARD_HAS_PSRAM)
  g_lv_buf1 = (lv_color_t*)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  g_lv_buf2 = (lv_color_t*)heap_caps_malloc(buf_px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
#endif
  if (!g_lv_buf1) g_lv_buf1 = (lv_color_t*)malloc(buf_px * sizeof(lv_color_t));
  if (!g_lv_buf2) g_lv_buf2 = (lv_color_t*)malloc(buf_px * sizeof(lv_color_t));
  if (!g_lv_buf1 || !g_lv_buf2) return false;

#if LVGL_VERSION_MAJOR >= 9
  // LVGL v9 display + input creation
  // Render a partial buffer (40 lines) for speed/ram
  g_lv_disp = lv_display_create(w, h);
  lv_display_set_flush_cb(g_lv_disp, lv_flush_cb);
  lv_display_set_buffers(g_lv_disp, g_lv_buf1, g_lv_buf2, buf_px * sizeof(lv_color_t), LV_DISPLAY_RENDER_MODE_PARTIAL);

  g_lv_indev = lv_indev_create();
  lv_indev_set_type(g_lv_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(g_lv_indev, lv_touch_read_cb);

#else
  // LVGL v8 driver registration
  lv_disp_draw_buf_init(&g_lv_drawbuf, g_lv_buf1, g_lv_buf2, buf_px);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = w;
  disp_drv.ver_res = h;
  disp_drv.flush_cb = lv_flush_cb;
  disp_drv.draw_buf = &g_lv_drawbuf;
  g_lv_disp = lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = lv_touch_read_cb;
  g_lv_indev = lv_indev_drv_register(&indev_drv);
#endif

g_lvgl_ready = true;
  return true;
}

void lvglTickAndUpdate() {
  if (!g_lvgl_ready) return;
  lv_timer_handler();

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 500) return;
  last = now;

  // Update message badge
  updateMessageBadge();

  // Time (best effort)
  if (g_lbl_time) {
    time_t t = time(nullptr);
    struct tm tm;
    if (localtime_r(&t, &tm)) {
      char buf[16];
      snprintf(buf, sizeof(buf), "%02d:%02d", tm.tm_hour, tm.tm_min);
      lv_label_set_text(g_lbl_time, buf);
    }
  }

  // RSSI
  if (g_lbl_rssi) {
    int r = WiFi.isConnected() ? WiFi.RSSI() : 0;
    char buf[24];
    snprintf(buf, sizeof(buf), "WiFi: %d", r);
    lv_label_set_text(g_lbl_rssi, buf);
  }

  // I2C traffic
  if (g_lbl_i2c) {
    char buf[32];
    snprintf(buf, sizeof(buf), "I2C: %lu/%lu", (unsigned long)g_i2c_tx_count, (unsigned long)g_i2c_rx_count);
    lv_label_set_text(g_lbl_i2c, buf);
  }

  if (g_firstboot_active && !g_firstboot_interacted) {
    if (now - g_firstboot_t0 > FIRST_BOOT_HEADLESS_TIMEOUT_MS) {
      // Auto headless
      g_cfg.headless = true;
      g_cfg.display_enable = false;
      g_cfg.touch_enable = false;
      g_cfg.lvgl_enable = false;
      g_cfg.panel = PANEL_NONE;
      g_cfg.touch = TOUCH_NONE;
      g_cfg.configured = true;
      saveConfig(g_cfg);
      ESP.restart();
    }
  }
}
#endif // DISPLAY_SUPPORT_ENABLED
