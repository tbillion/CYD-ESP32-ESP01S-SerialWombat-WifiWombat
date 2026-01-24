/*
 * CYD ESP32 Serial Wombat WiFi Bridge - Security Hardened
 * 
 * SECURITY NOTICE:
 * ===============
 * This firmware includes HTTP Basic Authentication on all sensitive endpoints.
 * 
 * DEFAULT CREDENTIALS (MUST BE CHANGED):
 *   Username: admin
 *   Password: CHANGE_ME_NOW
 * 
 * To change credentials, modify AUTH_USERNAME and AUTH_PASSWORD below.
 * 
 * Security Features:
 * - HTTP Basic Authentication on all sensitive endpoints
 * - Input validation on I2C addresses, pin numbers, paths
 * - Path traversal protection for file operations
 * - Security headers (CSP, X-Frame-Options, etc.)
 * - Request size limits (5MB uploads, 8KB JSON)
 * - Rate limiting on authentication failures
 * - OTA password protection
 * - Sanitized error messages
 * 
 * Public Endpoints (no auth):
 * - / (root dashboard)
 * - /api/health (health check)
 * 
 * Protected Endpoints (require auth):
 * - All /api/* endpoints (except /api/health)
 * - All file operations
 * - All configuration changes
 * - OTA updates
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoOTA.h>
#include <Wire.h>
#include <WiFiManager.h>
#include <FS.h>
#include "cyd_predecls.h" // must be in the include list (Arduino auto-prototype safety)

// --- Forward declarations for helpers (Arduino auto-prototypes can be unreliable with typedefs) ---
static bool sdEnsureMounted();
static bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path);
static bool sdGetUsage(uint64_t &total, uint64_t &used);
static uint64_t sd_total_bytes();
static uint64_t sd_used_bytes();

#include <LittleFS.h>
#include <ArduinoJson.h>
#include "src/services/web_server/html_templates.h"
#include "src/services/i2c_manager/i2c_manager.h"
#include "src/services/serialwombat/serialwombat_manager.h"
#include "src/services/web_server/api_handlers.h"

// ===================================================================================
// --- SECURITY CONFIGURATION ---
// ===================================================================================
// CRITICAL: Change these credentials before deployment!
// Default credentials are for initial setup only.
#define SECURITY_ENABLED 1
#define AUTH_USERNAME "admin"
#define AUTH_PASSWORD "CHANGE_ME_NOW"  // MUST be changed!

// Build-time check for default password (compile warning)
#if defined(SECURITY_ENABLED) && SECURITY_ENABLED == 1
  #if !defined(AUTH_PASSWORD) || (defined(AUTH_PASSWORD) && strcmp(AUTH_PASSWORD, "CHANGE_ME_NOW") == 0)
    #warning "*** SECURITY WARNING: Default password detected! Change AUTH_PASSWORD before deployment ***"
  #endif
#endif

#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)  // 5MB max upload
#define MAX_JSON_SIZE 8192  // 8KB max JSON payload

// CORS Configuration
// WARNING: Default allows all origins (*). For production, change to specific domain:
// #define CORS_ALLOW_ORIGIN "https://yourdomain.com"
#define CORS_ALLOW_ORIGIN "*"  // CHANGE IN PRODUCTION!

// Rate limiting (simple time-based)
static unsigned long g_last_auth_fail = 0;
static uint8_t g_auth_fail_count = 0;
static const uint16_t AUTH_LOCKOUT_MS = 5000;  // 5 second lockout after failed auth

// ===================================================================================
// --- Pre-Compilation Configuration (Global Scope) ---
// ===================================================================================
// SD_SUPPORT_ENABLED: set to 1 to enable the SD feature set, or 0 to compile it out.
#define SD_SUPPORT_ENABLED 1

// Pin mapping for SPI-mode SD cards (adjust to your wiring).
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18

// ESP-IDF host enum compatibility (Arduino-ESP32 v3.x)
#ifndef VSPI_HOST
  #define VSPI_HOST SPI3_HOST
#endif
#ifndef HSPI_HOST
  #define HSPI_HOST SPI2_HOST
#endif

// ===================================================================================
// --- Display / Touch / LVGL Configuration (Global Scope) ---
// ===================================================================================
// Compile-time master enable for local display stack. Web UI remains always enabled.
#define DISPLAY_SUPPORT_ENABLED 1

// Default enable states (can be overridden by config.json). These are *boot defaults*.
#define DEFAULT_DISPLAY_ENABLE 1
#define DEFAULT_TOUCH_ENABLE   1
#define DEFAULT_LVGL_ENABLE    1

// Headless auto-config timeout on first boot (no config.json)
#define FIRST_BOOT_HEADLESS_TIMEOUT_MS 30000UL

// Optional: battery ADC pin (set to -1 to disable in LVGL status bar)
#define BATTERY_ADC_PIN -1


// I2C traffic counters are always available (used by both Web UI and LVGL)
volatile uint32_t g_i2c_tx_count = 0;
volatile uint32_t g_i2c_rx_count = 0;
volatile bool g_i2c_tx_blink = false;
volatile bool g_i2c_rx_blink = false;

#if DISPLAY_SUPPORT_ENABLED
  #define LGFX_USE_V1
  #include <LovyanGFX.hpp>
  #include <lvgl.h>

  // Panel drivers (conditionally used by presets)
  #include <lgfx/v1/panel/Panel_ILI9341.hpp>
  #include <lgfx/v1/panel/Panel_ST7789.hpp>
  #include <lgfx/v1/panel/Panel_ST7796.hpp>
  #include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

  // Bus drivers
  #include <lgfx/v1/platforms/esp32/Bus_SPI.hpp>
  #include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

  // Touch drivers
  #include <lgfx/v1/touch/Touch_XPT2046.hpp>
  #include <lgfx/v1/touch/Touch_GT911.hpp>
  // Note: Capacitive touch ICs vary widely on small CYD variants.
  // We keep touch abstraction in config; presets may enable/disable touch.
#endif


// Global runtime toggle used by the UI and handlers.
// (When SD_SUPPORT_ENABLED==0 this still exists but stays false.)
bool isSDEnabled = (SD_SUPPORT_ENABLED != 0);

#if SD_SUPPORT_ENABLED

  // Shared SD state
  static bool   g_sdMounted = false;
  static String g_sdMountMsg = "";
  static int    g_sd_cs   = SD_CS;
  static int    g_sd_mosi = SD_MOSI;
  static int    g_sd_miso = SD_MISO;
  static int    g_sd_sck  = SD_SCK;

  // Upload state
  static bool   g_sdUploadOk = false;
  static String g_sdUploadMsg;
  #include <SPI.h>

  // SdFat library selection (explicit to avoid SD.h conflicts).
  #define CYD_USE_SDFAT 1
  #include <SdFat.h>

// ===================================================================================
// SD Abstraction Layer (SdFat)
// - Keeps the rest of the sketch stable regardless of calling site.
// ===================================================================================
  // ---- SdFat backend ----
  static SdFat sd;
  typedef FsFile SDFile;
  static bool sdMount() {
    if (g_sdMounted) return true;
    const uint8_t SD_CS_PIN = (uint8_t)g_sd_cs;
    SPI.begin(g_sd_sck, g_sd_miso, g_sd_mosi, SD_CS_PIN);
    SdSpiConfig config(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16));
    if (!sd.begin(config)) {
      g_sdMounted = false;
      g_sdMountMsg = "SD mount failed";
      return false;
    }
    g_sdMounted = true;
    g_sdMountMsg = "OK";
    return true;
  }

  static void sdUnmount() {
    // SdFat doesn't have a universal "end" across all configs; treat as logical unmount.
    g_sdMounted = false;
  }

  static bool sdExists(const char* path) { if (!g_sdMounted && !sdMount()) return false; return sd.exists(path); }
  static bool sdMkdir(const char* path)  { if (!g_sdMounted && !sdMount()) return false; return sd.mkdir(path); }
  static bool sdRemove(const char* path) { if (!g_sdMounted && !sdMount()) return false; return sd.remove(path); }
  static bool sdRmdir(const char* path)  { if (!g_sdMounted && !sdMount()) return false; return sd.rmdir(path); }
  static bool sdRename(const char* f, const char* t) { if (!g_sdMounted && !sdMount()) return false; return sd.rename(f, t); }

  static SDFile sdOpen(const char* path, oflag_t flags) {
    if (!g_sdMounted && !sdMount()) return SDFile();
    return sd.open(path, flags);
  }

  static bool sdIsDir(const char* path) {
    SDFile f = sdOpen(path, O_RDONLY);
    if (!f) return false;
    bool isDir = f.isDir();
    f.close();
    return isDir;
  }

  static bool sdGetStats(uint64_t &totalBytes, uint64_t &usedBytes) {
    totalBytes = 0; usedBytes = 0;
    if (!g_sdMounted && !sdMount()) return false;
    if (!sd.vol()) return false;
    uint32_t cCount = sd.vol()->clusterCount();
    uint32_t spc = sd.vol()->blocksPerCluster();
    uint32_t bps = 512;
    totalBytes = (uint64_t)cCount * (uint64_t)spc * (uint64_t)bps;
    uint32_t freeClusters = sd.vol()->freeClusterCount();
    uint64_t freeBytes = (uint64_t)freeClusters * (uint64_t)spc * (uint64_t)bps;
    usedBytes = totalBytes - freeBytes;
    return true;
  }

// ---- Unified helpers (work with SdFat or SD.h) ----
static bool sdFileIsDir(SDFile &f) {
  return f.isDir();
}

static String sdFileName(SDFile &f) {
  char nm[96];
  nm[0]=0;
  f.getName(nm, sizeof(nm));
  return String(nm);
}

static bool sdOpenNext(SDFile &dir, SDFile &out) {
  SDFile tmp;
  if (!tmp.openNext(&dir, O_RDONLY)) return false;
  out = tmp;
  return (bool)out;
}


#endif


// ===================================================================================
// CYD Framework Runtime Config (LittleFS: /config.json)
// ===================================================================================
// This config is intentionally compact to avoid heap churn. All fields have
// safe defaults so the web UI always comes up even if display is disabled.

static const char* CFG_PATH = "/config.json";

enum CydModel : uint8_t {
  CYD_UNKNOWN = 0,
  CYD_2432S028R,
  CYD_2432S028C,
  CYD_2432S022C,
  CYD_2432S032,
  CYD_3248S035,
  CYD_4827S043,
  CYD_8048S050,
  CYD_8048S070,
  CYD_S3_GENERIC
};

enum PanelKind : uint8_t {
  PANEL_NONE = 0,
  PANEL_SPI_ILI9341,
  PANEL_SPI_ST7789,
  PANEL_SPI_ST7796,
  PANEL_RGB_800x480
};

enum TouchKind : uint8_t {
  TOUCH_NONE = 0,
  TOUCH_XPT2046,
  TOUCH_GT911,
  TOUCH_I2C_GENERIC
};

struct SystemConfig {
  bool configured = false;
  bool headless = false;

  // runtime enables
  bool display_enable = (DEFAULT_DISPLAY_ENABLE != 0);
  bool touch_enable   = (DEFAULT_TOUCH_ENABLE != 0);
  bool lvgl_enable    = (DEFAULT_LVGL_ENABLE != 0);

  CydModel model = CYD_UNKNOWN;
  PanelKind panel = PANEL_NONE;
  TouchKind touch = TOUCH_NONE;

  // I2C pins (also used for I2C touch, when applicable)
  int i2c_sda = 21;
  int i2c_scl = 22;

  // SPI panel pins (ESP32-WROOM CYD family defaults)
  int tft_sck  = 14;
  int tft_mosi = 13;
  int tft_miso = 12;
  int tft_cs   = 15;
  int tft_dc   = 2;
  int tft_rst  = -1;
  int tft_bl   = 21;
  int tft_freq = 40000000;

  // Touch SPI pins (XPT2046-style; some CYDs use a separate SPI bus)
  int tp_sck  = 25;
  int tp_mosi = 32;
  int tp_miso = 39;
  int tp_cs   = 33;
  int tp_irq  = 36;

  // SD SPI pins (shared with display SPI on most CYD variants)
  int sd_sck  = SD_SCK;
  int sd_mosi = SD_MOSI;
  int sd_miso = SD_MISO;
  int sd_cs   = SD_CS;

  // RGB/GT911 pins (Sunton S3 7" style, from common community configs)
  // These are used only for PANEL_RGB_800x480.
  int rgb_pins[16] = {15,7,6,5,4, 9,46,3,8,16,1, 14,21,47,48,45};
  int rgb_hen = 41;
  int rgb_vsync = 40;
  int rgb_hsync = 39;
  int rgb_pclk  = 42;
  int rgb_freq_write = 12000000;

  // Splash asset stored in LittleFS (/assets/...) after first boot selection.
  String splash_path = "/assets/splash";
};

static SystemConfig g_cfg;

#if SD_SUPPORT_ENABLED
static void syncSdRuntimePins(const SystemConfig &cfg) {
  g_sd_sck = cfg.sd_sck;
  g_sd_mosi = cfg.sd_mosi;
  g_sd_miso = cfg.sd_miso;
  g_sd_cs = cfg.sd_cs;
}
#else
static void syncSdRuntimePins(const SystemConfig &) {}
#endif

static const char* modelToStr(CydModel m) {
  switch (m) {
    case CYD_2432S028R: return "2432S028R";
    case CYD_2432S028C: return "2432S028C";
    case CYD_2432S022C: return "2432S022C";
    case CYD_2432S032:  return "2432S032";
    case CYD_3248S035:  return "3248S035";
    case CYD_4827S043:  return "4827S043";
    case CYD_8048S050:  return "8048S050";
    case CYD_8048S070:  return "8048S070";
    case CYD_S3_GENERIC:return "S3_GENERIC";
    default: return "UNKNOWN";
  }
}

static CydModel strToModel(const String& s) {
  if (s == "2432S028R") return CYD_2432S028R;
  if (s == "2432S028C") return CYD_2432S028C;
  if (s == "2432S022C") return CYD_2432S022C;
  if (s == "2432S032")  return CYD_2432S032;
  if (s == "3248S035")  return CYD_3248S035;
  if (s == "4827S043")  return CYD_4827S043;
  if (s == "8048S050")  return CYD_8048S050;
  if (s == "8048S070")  return CYD_8048S070;
  if (s == "S3_GENERIC")return CYD_S3_GENERIC;
  return CYD_UNKNOWN;
}

static bool cfgExists() {
  return LittleFS.exists(CFG_PATH);
}

static void applyModelPreset(SystemConfig &cfg) {
  // Base defaults
  cfg.display_enable = (DEFAULT_DISPLAY_ENABLE != 0);
  cfg.touch_enable   = (DEFAULT_TOUCH_ENABLE != 0);
  cfg.lvgl_enable    = (DEFAULT_LVGL_ENABLE != 0);
  cfg.headless = false;

  // I2C defaults from the user's mapping table
  if (cfg.model == CYD_8048S050 || cfg.model == CYD_8048S070) {
    cfg.i2c_sda = 19;
    cfg.i2c_scl = 20;
  } else if (cfg.model == CYD_S3_GENERIC) {
    cfg.i2c_sda = 4;
    cfg.i2c_scl = 5;
  } else if (cfg.model == CYD_4827S043) {
    cfg.i2c_sda = 17;
    cfg.i2c_scl = 18;
  } else {
    cfg.i2c_sda = 21;
    cfg.i2c_scl = 22;
  }

  // SD SPI defaults from the mapping table (shared across CYD variants unless overridden)
  switch (cfg.model) {
    case CYD_8048S050:
    case CYD_8048S070:
    case CYD_4827S043:
    case CYD_S3_GENERIC:
    case CYD_2432S028R:
    case CYD_2432S028C:
    case CYD_2432S022C:
    case CYD_2432S032:
    case CYD_3248S035:
    default:
      cfg.sd_sck  = SD_SCK;
      cfg.sd_mosi = SD_MOSI;
      cfg.sd_miso = SD_MISO;
      cfg.sd_cs   = SD_CS;
      break;
  }

  switch (cfg.model) {
    case CYD_2432S028R:
    case CYD_2432S028C:
    case CYD_2432S032:
      cfg.panel = PANEL_SPI_ILI9341;
      cfg.touch = (cfg.model == CYD_2432S028R) ? TOUCH_XPT2046 : TOUCH_I2C_GENERIC;
      break;
    case CYD_2432S022C:
      cfg.panel = PANEL_SPI_ST7789;
      cfg.touch = TOUCH_I2C_GENERIC;
      break;
    case CYD_3248S035:
      cfg.panel = PANEL_SPI_ST7796;
      cfg.touch = TOUCH_I2C_GENERIC;
      break;
    case CYD_8048S050:
    case CYD_8048S070:
      cfg.panel = PANEL_RGB_800x480;
      cfg.touch = TOUCH_GT911;
      break;
    case CYD_4827S043:
      // NV3047 variants vary in bus wiring; we keep panel disabled unless user edits pins.
      cfg.panel = PANEL_NONE;
      cfg.touch = TOUCH_I2C_GENERIC;
      cfg.display_enable = false;
      cfg.lvgl_enable = false;
      break;
    case CYD_S3_GENERIC:
      cfg.panel = PANEL_NONE;
      cfg.touch = TOUCH_NONE;
      cfg.display_enable = false;
      cfg.lvgl_enable = false;
      break;
    default:
      cfg.panel = PANEL_NONE;
      cfg.touch = TOUCH_NONE;
      cfg.display_enable = false;
      cfg.lvgl_enable = false;
      break;
  }

  syncSdRuntimePins(cfg);
}

// Establish a known-safe baseline when no config.json exists.
static void setConfigDefaults(SystemConfig &cfg) {
  cfg = SystemConfig();
  cfg.configured = false;
  cfg.headless = false;
  // Start unconfigured: no panel/touch until user selects a model (or headless triggers).
  cfg.model = CYD_UNKNOWN;
  applyModelPreset(cfg);
}

static bool loadConfig(SystemConfig &cfg) {
  if (!cfgExists()) return false;
  File f = LittleFS.open(CFG_PATH, "r");
  if (!f) return false;

  StaticJsonDocument<1536> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  cfg.configured = doc["configured"] | false;
  cfg.headless   = doc["headless"] | false;

  cfg.display_enable = doc["display_enable"] | (DEFAULT_DISPLAY_ENABLE != 0);
  cfg.touch_enable   = doc["touch_enable"]   | (DEFAULT_TOUCH_ENABLE != 0);
  cfg.lvgl_enable    = doc["lvgl_enable"]    | (DEFAULT_LVGL_ENABLE != 0);

  cfg.model = strToModel(String((const char*)(doc["model"] | "UNKNOWN")));
  cfg.panel = (PanelKind)(doc["panel"] | (int)PANEL_NONE);
  cfg.touch = (TouchKind)(doc["touch"] | (int)TOUCH_NONE);

  cfg.i2c_sda = doc["i2c_sda"] | cfg.i2c_sda;
  cfg.i2c_scl = doc["i2c_scl"] | cfg.i2c_scl;

  cfg.tft_sck  = doc["tft_sck"]  | cfg.tft_sck;
  cfg.tft_mosi = doc["tft_mosi"] | cfg.tft_mosi;
  cfg.tft_miso = doc["tft_miso"] | cfg.tft_miso;
  cfg.tft_cs   = doc["tft_cs"]   | cfg.tft_cs;
  cfg.tft_dc   = doc["tft_dc"]   | cfg.tft_dc;
  cfg.tft_rst  = doc["tft_rst"]  | cfg.tft_rst;
  cfg.tft_bl   = doc["tft_bl"]   | cfg.tft_bl;
  cfg.tft_freq = doc["tft_freq"] | cfg.tft_freq;

  cfg.tp_sck  = doc["tp_sck"]  | cfg.tp_sck;
  cfg.tp_mosi = doc["tp_mosi"] | cfg.tp_mosi;
  cfg.tp_miso = doc["tp_miso"] | cfg.tp_miso;
  cfg.tp_cs   = doc["tp_cs"]   | cfg.tp_cs;
  cfg.tp_irq  = doc["tp_irq"]  | cfg.tp_irq;

  cfg.sd_sck  = doc["sd_sck"]  | cfg.sd_sck;
  cfg.sd_mosi = doc["sd_mosi"] | cfg.sd_mosi;
  cfg.sd_miso = doc["sd_miso"] | cfg.sd_miso;
  cfg.sd_cs   = doc["sd_cs"]   | cfg.sd_cs;

  cfg.splash_path = String((const char*)(doc["splash"] | cfg.splash_path.c_str()));

  // If the config says headless, forcibly disable local stack.
  if (cfg.headless) {
    cfg.display_enable = false;
    cfg.touch_enable = false;
    cfg.lvgl_enable = false;
    cfg.panel = PANEL_NONE;
    cfg.touch = TOUCH_NONE;
  }

  syncSdRuntimePins(cfg);
  return true;
}

static bool saveConfig(const SystemConfig &cfg) {
  StaticJsonDocument<1536> doc;
  doc["configured"] = cfg.configured;
  doc["headless"] = cfg.headless;
  doc["display_enable"] = cfg.display_enable;
  doc["touch_enable"] = cfg.touch_enable;
  doc["lvgl_enable"] = cfg.lvgl_enable;
  doc["model"] = modelToStr(cfg.model);
  doc["panel"] = (int)cfg.panel;
  doc["touch"] = (int)cfg.touch;
  doc["i2c_sda"] = cfg.i2c_sda;
  doc["i2c_scl"] = cfg.i2c_scl;
  doc["tft_sck"] = cfg.tft_sck;
  doc["tft_mosi"] = cfg.tft_mosi;
  doc["tft_miso"] = cfg.tft_miso;
  doc["tft_cs"] = cfg.tft_cs;
  doc["tft_dc"] = cfg.tft_dc;
  doc["tft_rst"] = cfg.tft_rst;
  doc["tft_bl"] = cfg.tft_bl;
  doc["tft_freq"] = cfg.tft_freq;
  doc["tp_sck"] = cfg.tp_sck;
  doc["tp_mosi"] = cfg.tp_mosi;
  doc["tp_miso"] = cfg.tp_miso;
  doc["tp_cs"] = cfg.tp_cs;
  doc["tp_irq"] = cfg.tp_irq;
  doc["sd_sck"] = cfg.sd_sck;
  doc["sd_mosi"] = cfg.sd_mosi;
  doc["sd_miso"] = cfg.sd_miso;
  doc["sd_cs"] = cfg.sd_cs;
  doc["splash"] = cfg.splash_path;

  File f = LittleFS.open(CFG_PATH, "w");
  if (!f) return false;
  bool ok = (serializeJson(doc, f) > 0);
  f.close();
  return ok;
}

// ===================================================================================
// LovyanGFX + LVGL glue
// ===================================================================================
#if DISPLAY_SUPPORT_ENABLED
class LGFX : public lgfx::LGFX_Device {
public:
  lgfx::Bus_SPI _bus_spi;
  lgfx::Panel_ILI9341 _panel_ili;
  lgfx::Panel_ST7789  _panel_7789;
  lgfx::Panel_ST7796  _panel_7796;

  lgfx::Bus_RGB _bus_rgb;
  lgfx::Panel_RGB _panel_rgb;

  lgfx::Light_PWM _light_pwm;

  lgfx::Touch_XPT2046 _touch_xpt;
  lgfx::Touch_GT911   _touch_gt;

  bool beginFromConfig(const SystemConfig &cfg) {
    if (!cfg.display_enable || cfg.panel == PANEL_NONE) return false;

    if (cfg.panel == PANEL_RGB_800x480) {
      using namespace lgfx;
      {
        auto pcfg = _panel_rgb.config();
        pcfg.memory_width = 800;
        pcfg.memory_height = 480;
        pcfg.panel_width = 800;
        pcfg.panel_height = 480;
        pcfg.offset_x = 0;
        pcfg.offset_y = 0;
        _panel_rgb.config(pcfg);
      }
      {
        auto dc = _panel_rgb.config_detail();
        dc.use_psram = 1;
        _panel_rgb.config_detail(dc);
      }
      {
        auto bcfg = _bus_rgb.config();
        bcfg.panel = &_panel_rgb;
        bcfg.pin_d0  = (gpio_num_t)cfg.rgb_pins[0];
        bcfg.pin_d1  = (gpio_num_t)cfg.rgb_pins[1];
        bcfg.pin_d2  = (gpio_num_t)cfg.rgb_pins[2];
        bcfg.pin_d3  = (gpio_num_t)cfg.rgb_pins[3];
        bcfg.pin_d4  = (gpio_num_t)cfg.rgb_pins[4];
        bcfg.pin_d5  = (gpio_num_t)cfg.rgb_pins[5];
        bcfg.pin_d6  = (gpio_num_t)cfg.rgb_pins[6];
        bcfg.pin_d7  = (gpio_num_t)cfg.rgb_pins[7];
        bcfg.pin_d8  = (gpio_num_t)cfg.rgb_pins[8];
        bcfg.pin_d9  = (gpio_num_t)cfg.rgb_pins[9];
        bcfg.pin_d10 = (gpio_num_t)cfg.rgb_pins[10];
        bcfg.pin_d11 = (gpio_num_t)cfg.rgb_pins[11];
        bcfg.pin_d12 = (gpio_num_t)cfg.rgb_pins[12];
        bcfg.pin_d13 = (gpio_num_t)cfg.rgb_pins[13];
        bcfg.pin_d14 = (gpio_num_t)cfg.rgb_pins[14];
        bcfg.pin_d15 = (gpio_num_t)cfg.rgb_pins[15];
        bcfg.pin_henable = (gpio_num_t)cfg.rgb_hen;
        bcfg.pin_vsync   = (gpio_num_t)cfg.rgb_vsync;
        bcfg.pin_hsync   = (gpio_num_t)cfg.rgb_hsync;
        bcfg.pin_pclk    = (gpio_num_t)cfg.rgb_pclk;
        bcfg.freq_write  = cfg.rgb_freq_write;
        bcfg.hsync_polarity = 0;
        bcfg.hsync_front_porch = 8;
        bcfg.hsync_pulse_width = 2;
        bcfg.hsync_back_porch = 43;
        bcfg.vsync_polarity = 0;
        bcfg.vsync_front_porch = 8;
        bcfg.vsync_pulse_width = 2;
        bcfg.vsync_back_porch = 12;
        bcfg.pclk_idle_high = 1;
        _bus_rgb.config(bcfg);
      }
      _panel_rgb.setBus(&_bus_rgb);

      // Backlight
      {
        auto lcfg = _light_pwm.config();
        lcfg.pin_bl = -1; // many RGB boards use external control; keep disabled
        lcfg.invert = false;
        lcfg.freq = 44100;
        lcfg.pwm_channel = 7;
        _light_pwm.config(lcfg);
      }
      _panel_rgb.setLight(&_light_pwm);

      // Touch
      if (cfg.touch_enable && cfg.touch == TOUCH_GT911) {
        auto tcfg = _touch_gt.config();
        tcfg.x_min = 0; tcfg.y_min = 0;
        tcfg.x_max = 799; tcfg.y_max = 479;
        tcfg.pin_sda = (gpio_num_t)cfg.i2c_sda;
        tcfg.pin_scl = (gpio_num_t)cfg.i2c_scl;
        tcfg.i2c_port = I2C_NUM_0;
        tcfg.i2c_addr = 0x5D;
        tcfg.freq = 400000;
        tcfg.bus_shared = false;
        _touch_gt.config(tcfg);
        _panel_rgb.setTouch(&_touch_gt);
      }

      setPanel(&_panel_rgb);
      return init();
    }

    // SPI Panels
    lgfx::Panel_Device* panel = nullptr;
    if (cfg.panel == PANEL_SPI_ILI9341) panel = &_panel_ili;
    else if (cfg.panel == PANEL_SPI_ST7789) panel = &_panel_7789;
    else if (cfg.panel == PANEL_SPI_ST7796) panel = &_panel_7796;
    else return false;

    {
      auto bcfg = _bus_spi.config();
      bcfg.spi_host = VSPI_HOST;
      bcfg.spi_mode = 0;
      bcfg.freq_write = cfg.tft_freq;
      bcfg.freq_read  = 16000000;
      bcfg.pin_sclk = cfg.tft_sck;
      bcfg.pin_mosi = cfg.tft_mosi;
      bcfg.pin_miso = cfg.tft_miso;
      bcfg.pin_dc   = cfg.tft_dc;
      _bus_spi.config(bcfg);
      panel->setBus(&_bus_spi);
    }

    {
      auto pcfg = panel->config();
      pcfg.pin_cs = cfg.tft_cs;
      pcfg.pin_rst = cfg.tft_rst;
      pcfg.pin_busy = -1;
      pcfg.panel_width = 320;
      pcfg.panel_height = 240;
      pcfg.offset_x = 0;
      pcfg.offset_y = 0;
      pcfg.readable = false;
      pcfg.invert = false;
      pcfg.rgb_order = false;
      pcfg.dlen_16bit = false;
      pcfg.bus_shared = true;
      panel->config(pcfg);
    }

    // Backlight
    {
      auto lcfg = _light_pwm.config();
      lcfg.pin_bl = cfg.tft_bl;
      lcfg.invert = false;
      lcfg.freq = 44100;
      lcfg.pwm_channel = 7;
      _light_pwm.config(lcfg);
      panel->setLight(&_light_pwm);
    }

    // Touch (XPT2046)
    if (cfg.touch_enable && cfg.touch == TOUCH_XPT2046) {
      auto tcfg = _touch_xpt.config();
      tcfg.spi_host = HSPI_HOST; // CYD touch is often on HSPI
      tcfg.freq = 2000000;
      tcfg.pin_sclk = cfg.tp_sck;
      tcfg.pin_mosi = cfg.tp_mosi;
      tcfg.pin_miso = cfg.tp_miso;
      tcfg.pin_cs   = cfg.tp_cs;
      tcfg.pin_int  = cfg.tp_irq;
      tcfg.bus_shared = false;
      tcfg.x_min = 200; tcfg.x_max = 3900;
      tcfg.y_min = 200; tcfg.y_max = 3900;
      tcfg.offset_rotation = 0;
      _touch_xpt.config(tcfg);
      panel->setTouch(&_touch_xpt);
    }

    setPanel(panel);
    return init();
  }
};

static LGFX lcd;


#if LVGL_VERSION_MAJOR >= 9
// ===================== LVGL v9 =====================
static lv_color_t* g_lv_buf1 = nullptr;
static lv_color_t* g_lv_buf2 = nullptr;
static lv_display_t* g_lv_disp = nullptr;
static lv_indev_t* g_lv_indev = nullptr;

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
static lv_disp_draw_buf_t g_lv_drawbuf;
static lv_color_t *g_lv_buf1 = nullptr;
static lv_color_t *g_lv_buf2 = nullptr;
static lv_disp_drv_t g_lv_disp_drv;
static lv_indev_drv_t g_lv_indev_drv;
static lv_disp_t* g_lv_disp = nullptr;
static lv_indev_t* g_lv_indev = nullptr;

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
// Basic LVGL objects for first boot / status bar
static bool g_lvgl_ready = false;
static bool g_firstboot_active = false;
static bool g_firstboot_interacted = false;
static uint32_t g_firstboot_t0 = 0;

static lv_obj_t* g_status_bar = nullptr;
static lv_obj_t* g_lbl_time = nullptr;
static lv_obj_t* g_lbl_rssi = nullptr;
static lv_obj_t* g_lbl_i2c  = nullptr;
static lv_obj_t* g_lbl_batt = nullptr;

static lv_obj_t* g_dd_model = nullptr;
static lv_obj_t* g_btn_next = nullptr;
static lv_obj_t* g_file_list = nullptr;
static String g_sd_cwd = "/";

static void buildStatusBar() {
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
}

static void firstBootShowModelSelect();
static void firstBootShowSplashPicker();

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

static void firstBootShowModelSelect() {
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
static bool sdListDirToLV(const String& dir);

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

static void firstBootShowSplashPicker() {
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

static bool lvglInitIfEnabled() {
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

static void lvglTickAndUpdate() {
  if (!g_lvgl_ready) return;
  lv_timer_handler();

  static uint32_t last = 0;
  uint32_t now = millis();
  if (now - last < 500) return;
  last = now;

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

// ===================================================================================
// IntelHexSW8B - embedded library (single-file integration)
// Source: IntelHexSW8B (zip provided)
// ===================================================================================
class IntelHexSW8B {
public:
  IntelHexSW8B();

  // Initialize with a filesystem (LittleFS or SD). Creates cacheDir if needed.
  // Cache is flash-backed so it works with or without PSRAM.
  bool begin(fs::FS& fs, const char* cacheDir = "/hexcache");

  // Clear cache files (data + validity) and warnings/bounds.
  void clearCache();

  // Load and parse an Intel HEX file from the filesystem.
  // Supports record types: 00 (data), 01 (EOF), 04 (extended linear address).
  // If enforceChecksum is true, lines with invalid checksum are ignored (like the original C# code).
  bool loadHexFile(const char* hexPath, bool enforceChecksum = false);

  // Bounds observed during parse (highest/lowest written absolute byte address).
  bool hasBounds() const { return _boundsSet; }
  uint32_t minAddress() const { return _minAddr; }
  uint32_t maxAddress() const { return _maxAddr; }

  // Warnings/errors collected (duplicate addresses, out-of-range writes, missing bytes in strict export, etc.)
  const String& warnings() const { return _warnings; }

  // Strict export for CH32V003 16KB firmware window.
  // - Fixed range: [0x00000000, 0x00004000)
  // - Little-endian uint16 packing: word = b0 | (b1<<8)
  // - Output: 0xXXXX comma-separated, no whitespace/comments.
  // - STRICT: If any byte in the 16KB window is missing, export fails and warnings() will report the first missing address.
  bool exportFW_CH32V003_16K_Strict(const char* outPath,
                                   bool trailingComma = true,
                                   bool newlineAtEnd = false);

  // Optional: CRC16-CCITT over a byte range, treating missing bytes as an error when strict=true.
  // This mirrors the original algorithm (poly 0x1021, init 0xFFFF).
  // If strict is true and any byte is missing, returns 0 and appends an error to warnings().
  uint16_t crc16ccitt(uint32_t start, uint32_t exclusiveEnd, bool strict = true, uint8_t fillValue = 0xFF);

private:
  fs::FS* _fs = nullptr;
  String _cacheDir;
  String _dataPath;
  String _validPath;

  bool _boundsSet = false;
  uint32_t _minAddr = 0;
  uint32_t _maxAddr = 0;

  String _warnings;

  static constexpr uint32_t PAGE_SIZE = 256;
  static constexpr uint32_t VALID_BYTES = 32; // 256 bits => 32 bytes

  bool ensureCacheFiles_();

  bool readPageValid_(uint32_t pageIndex, uint8_t* valid32);
  bool writePageValid_(uint32_t pageIndex, const uint8_t* valid32);

  bool readPageData_(uint32_t pageIndex, uint8_t* data256);
  bool writePageData_(uint32_t pageIndex, const uint8_t* data256);

  bool setByte_(uint32_t addr, uint8_t value);
  bool getByte_(uint32_t addr, uint8_t& value, bool& isValid);

  // HEX parsing helpers
  static inline uint8_t hexNibble_(char c);
  static bool parseHexByte_(const char* s, uint8_t& out);
  static bool parseHexU16_(const char* s, uint16_t& out);

  bool parseLine_(const String& raw, bool enforceChecksum, uint32_t& extHigh);
};


IntelHexSW8B::IntelHexSW8B() {}

static bool fileWriteAt_(fs::FS& fs, const char* path, uint32_t offset, const uint8_t* buf, uint32_t len) {
  File f = fs.open(path, "r+");
  if (!f) f = fs.open(path, "w+");
  if (!f) return false;
  if (!f.seek(offset)) { f.close(); return false; }
  size_t w = f.write(buf, len);
  f.close();
  return (w == len);
}

bool IntelHexSW8B::begin(fs::FS& fs, const char* cacheDir) {
  _fs = &fs;
  _cacheDir = cacheDir;
  if (!_cacheDir.startsWith("/")) _cacheDir = "/" + _cacheDir;

  _dataPath  = _cacheDir + "/data.bin";
  _validPath = _cacheDir + "/valid.bin";

  _warnings = "";
  _boundsSet = false;
  _minAddr = 0;
  _maxAddr = 0;

  _fs->mkdir(_cacheDir.c_str());
  return ensureCacheFiles_();
}

bool IntelHexSW8B::ensureCacheFiles_() {
  if (!_fs) return false;
  if (!_fs->exists(_dataPath.c_str())) {
    File f = _fs->open(_dataPath.c_str(), "w");
    if (!f) return false;
    f.close();
  }
  if (!_fs->exists(_validPath.c_str())) {
    File f = _fs->open(_validPath.c_str(), "w");
    if (!f) return false;
    f.close();
  }
  return true;
}

void IntelHexSW8B::clearCache() {
  if (!_fs) return;
  if (_fs->exists(_dataPath.c_str()))  _fs->remove(_dataPath.c_str());
  if (_fs->exists(_validPath.c_str())) _fs->remove(_validPath.c_str());
  ensureCacheFiles_();
  _warnings = "";
  _boundsSet = false;
}

bool IntelHexSW8B::readPageValid_(uint32_t pageIndex, uint8_t* valid32) {
  memset(valid32, 0, VALID_BYTES);
  uint32_t off = pageIndex * VALID_BYTES;

  File f = _fs->open(_validPath.c_str(), "r");
  if (!f) return false;
  uint32_t sz = (uint32_t)f.size();
  if (off >= sz) { f.close(); return true; }

  f.seek(off);
  int r = f.read(valid32, VALID_BYTES);
  f.close();
  if (r < 0) return false;
  if (r < (int)VALID_BYTES) memset(valid32 + r, 0, VALID_BYTES - r);
  return true;
}

bool IntelHexSW8B::writePageValid_(uint32_t pageIndex, const uint8_t* valid32) {
  uint32_t off = pageIndex * VALID_BYTES;
  return fileWriteAt_(*_fs, _validPath.c_str(), off, valid32, VALID_BYTES);
}

bool IntelHexSW8B::readPageData_(uint32_t pageIndex, uint8_t* data256) {
  // default to 0xFF for readability; validity bitmap determines "present".
  memset(data256, 0xFF, PAGE_SIZE);
  uint32_t off = pageIndex * PAGE_SIZE;

  File f = _fs->open(_dataPath.c_str(), "r");
  if (!f) return false;
  uint32_t sz = (uint32_t)f.size();
  if (off >= sz) { f.close(); return true; }

  f.seek(off);
  int r = f.read(data256, PAGE_SIZE);
  f.close();
  if (r < 0) return false;
  if (r < (int)PAGE_SIZE) memset(data256 + r, 0xFF, PAGE_SIZE - r);
  return true;
}

bool IntelHexSW8B::writePageData_(uint32_t pageIndex, const uint8_t* data256) {
  uint32_t off = pageIndex * PAGE_SIZE;
  return fileWriteAt_(*_fs, _dataPath.c_str(), off, data256, PAGE_SIZE);
}

bool IntelHexSW8B::setByte_(uint32_t addr, uint8_t value) {
  uint32_t page = addr >> 8;
  uint32_t off  = addr & 0xFF;

  uint8_t valid[VALID_BYTES];
  uint8_t data[PAGE_SIZE];

  if (!readPageValid_(page, valid)) return false;
  if (!readPageData_(page, data)) return false;

  uint8_t bit = 1u << (off & 7);
  uint32_t idx = off >> 3;
  if (valid[idx] & bit) {
    _warnings += "Warning: Address 0x";
    _warnings += String(addr, HEX);
    _warnings += " is defined multiple times\n";
  }

  data[off] = value;
  valid[idx] |= bit;

  if (!writePageData_(page, data)) return false;
  if (!writePageValid_(page, valid)) return false;

  if (!_boundsSet) {
    _boundsSet = true;
    _minAddr = addr;
    _maxAddr = addr;
  } else {
    if (addr < _minAddr) _minAddr = addr;
    if (addr > _maxAddr) _maxAddr = addr;
  }
  return true;
}

bool IntelHexSW8B::getByte_(uint32_t addr, uint8_t& value, bool& isValid) {
  uint32_t page = addr >> 8;
  uint32_t off  = addr & 0xFF;

  uint8_t valid[VALID_BYTES];
  uint8_t data[PAGE_SIZE];

  if (!readPageValid_(page, valid)) return false;
  if (!readPageData_(page, data)) return false;

  uint8_t bit = 1u << (off & 7);
  uint32_t idx = off >> 3;
  isValid = (valid[idx] & bit) != 0;
  value = data[off];
  return true;
}

uint8_t IntelHexSW8B::hexNibble_(char c) {
  if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
  if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
  return 0xFF;
}

bool IntelHexSW8B::parseHexByte_(const char* s, uint8_t& out) {
  uint8_t hi = hexNibble_(s[0]);
  uint8_t lo = hexNibble_(s[1]);
  if (hi == 0xFF || lo == 0xFF) return false;
  out = (uint8_t)((hi << 4) | lo);
  return true;
}

bool IntelHexSW8B::parseHexU16_(const char* s, uint16_t& out) {
  uint8_t b0, b1;
  if (!parseHexByte_(s, b0)) return false;
  if (!parseHexByte_(s + 2, b1)) return false;
  out = (uint16_t)((b0 << 8) | b1);
  return true;
}

bool IntelHexSW8B::parseLine_(const String& raw, bool enforceChecksum, uint32_t& extHigh) {
  String line = raw;
  line.trim();
  if (line.endsWith("\r")) line.remove(line.length() - 1);

  // Remove whitespace inside line (like Regex.Replace("\\s*", ""))
  {
    String out; out.reserve(line.length());
    for (size_t i = 0; i < line.length(); i++) {
      char c = line[i];
      if (!isspace((unsigned char)c)) out += c;
    }
    line = out;
  }

  if (line.length() < 11) return true;
  if (line[0] != ':') return true;

  // Reject illegal chars
  for (size_t i = 1; i < line.length(); i++) {
    if (!isxdigit((unsigned char)line[i])) return true;
  }

  uint8_t len;
  if (!parseHexByte_(line.c_str() + 1, len)) return true;
  if ((uint32_t)line.length() != (uint32_t)(11 + (uint32_t)len * 2)) return true;

  uint16_t addr16;
  if (!parseHexU16_(line.c_str() + 3, addr16)) return true;

  uint8_t rectype;
  {
    uint8_t rt;
    if (!parseHexByte_(line.c_str() + 7, rt)) return true;
    rectype = rt;
  }

  uint8_t indicated;
  if (!parseHexByte_(line.c_str() + line.length() - 2, indicated)) return true;

  uint32_t sum = 0;
  for (size_t i = 1; i < line.length() - 2; i += 2) {
    uint8_t b;
    if (!parseHexByte_(line.c_str() + i, b)) return true;
    sum += b;
  }
  uint8_t calc = (uint8_t)((0x100 - (sum & 0xFF)) & 0xFF);

  if (enforceChecksum && (calc != indicated)) return true;

  if (rectype == 0x00) {
    const char* p = line.c_str() + 9;
    for (uint8_t i = 0; i < len; i++) {
      uint8_t b;
      if (!parseHexByte_(p + i * 2, b)) return true;
      uint32_t absAddr = (extHigh << 16) + (uint32_t)addr16 + i;

      // If targeting CH32V003 16KB strictly, we can warn on out-of-range writes
      // but still store them in cache. The strict exporter will refuse missing bytes,
      // and you can choose to fail early if desired.
      if (absAddr >= 0x00004000) {
        _warnings += "Warning: Write beyond 16KB window at 0x";
        _warnings += String(absAddr, HEX);
        _warnings += "\n";
      }

      if (!setByte_(absAddr, b)) return false;
    }
  } else if (rectype == 0x04) {
    if (len != 2) return true;
    uint16_t high;
    if (!parseHexU16_(line.c_str() + 9, high)) return true;
    extHigh = high;
  } else if (rectype == 0x01) {
    // EOF
  } else {
    // Ignore unsupported types (matches original approach)
    _warnings += "Warning: Unsupported record type 0x";
    _warnings += String(rectype, HEX);
    _warnings += " ignored\n";
  }

  return true;
}

bool IntelHexSW8B::loadHexFile(const char* hexPath, bool enforceChecksum) {
  if (!_fs) return false;
  if (!ensureCacheFiles_()) return false;

  File f = _fs->open(hexPath, "r");
  if (!f) return false;

  _warnings = "";
  _boundsSet = false;

  uint32_t extHigh = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (!parseLine_(line, enforceChecksum, extHigh)) {
      f.close();
      return false;
    }
  }
  f.close();
  return true;
}

bool IntelHexSW8B::exportFW_CH32V003_16K_Strict(const char* outPath,
                                               bool trailingComma,
                                               bool newlineAtEnd) {
  if (!_fs) return false;

  const uint32_t start = 0x00000000;
  const uint32_t end   = 0x00004000; // 16KB

  File out = _fs->open(outPath, "w");
  if (!out) return false;

  bool first = true;

  for (uint32_t a = start; a < end; a += 2) {
    uint8_t b0, b1;
    bool v0, v1;

    if (!getByte_(a,   b0, v0)) { out.close(); return false; }
    if (!getByte_(a+1, b1, v1)) { out.close(); return false; }

    if (!v0 || !v1) {
      out.close();
      uint32_t miss = !v0 ? a : (a + 1);
      _warnings += "ERROR: Missing byte at 0x";
      _warnings += String(miss, HEX);
      _warnings += " within required 16KB window\n";
      return false;
    }

    uint16_t w = (uint16_t)(b0 | ((uint16_t)b1 << 8)); // little-endian word packing

    char buf[8];
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned)w);

    if (!first) out.print(",");
    first = false;
    out.print(buf);
  }

  if (trailingComma) out.print(",");
  if (newlineAtEnd) out.print("\n");
  out.close();
  return true;
}

uint16_t IntelHexSW8B::crc16ccitt(uint32_t start, uint32_t exclusiveEnd, bool strict, uint8_t fillValue) {
  uint16_t crc = 0xFFFF;
  for (uint32_t a = start; a < exclusiveEnd; a++) {
    uint8_t b;
    bool valid;
    if (!getByte_(a, b, valid)) {
      _warnings += "ERROR: Read failed at 0x";
      _warnings += String(a, HEX);
      _warnings += "\n";
      return 0;
    }
    if (!valid) {
      if (strict) {
        _warnings += "ERROR: Missing byte at 0x";
        _warnings += String(a, HEX);
        _warnings += " during CRC (strict)\n";
        return 0;
      }
      b = fillValue;
    }

    crc ^= (uint16_t)b << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000) crc = (uint16_t)((crc << 1) ^ 0x1021);
      else crc <<= 1;
    }
  }
  return crc;
}


// --- Serial Wombat Library & Sub-Modules (Library Manager v2.2.2) ---
#include <SerialWombat.h>
#include <SerialWombatPWM.h>
#include <SerialWombatServo.h>
#include <SerialWombatAnalogInput.h>
#include <SerialWombatPulseTimer.h>
#include <SerialWombatUltrasonicDistanceSensor.h>
#include <SerialWombatDebouncedInput.h>
#include <SerialWombatQuadEnc.h>
#include <SerialWombatTM1637.h>
#include <SerialWombatHBridge.h>

// ===================================================================================
// CONFIGURATION (ESP32-S3)
// ===================================================================================
// Hardcoded per requirement
#define SDA_PIN 4
#define SCL_PIN 5
#define TCP_PORT 3000

WebServer server(80);
WiFiServer tcpServer(TCP_PORT);
WiFiClient tcpClient;

SerialWombat sw;
uint8_t currentWombatAddress = 0x6C;

// ===================================================================================
// --- SECURITY FUNCTIONS ---
// ===================================================================================

/**
 * Add security headers to HTTP response
 * Implements: CORS, CSP, X-Frame-Options, X-Content-Type-Options
 * 
 * Notes:
 * - HSTS is included but only effective over HTTPS (not yet implemented)
 * - CSP uses 'unsafe-inline' due to embedded HTML with inline scripts
 * - CORS uses wildcard by default - CHANGE CORS_ALLOW_ORIGIN for production
 */
static void addSecurityHeaders() {
  server.sendHeader("X-Content-Type-Options", "nosniff");
  server.sendHeader("X-Frame-Options", "DENY");
  server.sendHeader("X-XSS-Protection", "1; mode=block");
  // CSP: 'unsafe-inline' required for embedded HTML, consider external JS in future
  server.sendHeader("Content-Security-Policy", "default-src 'self' 'unsafe-inline'; img-src 'self' data:;");
  // HSTS: Only effective over HTTPS, included for future HTTPS implementation
  server.sendHeader("Strict-Transport-Security", "max-age=31536000; includeSubDomains");
  // CORS - PRODUCTION WARNING: Change CORS_ALLOW_ORIGIN to specific domain
  server.sendHeader("Access-Control-Allow-Origin", CORS_ALLOW_ORIGIN);
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
}

/**
 * Check HTTP Basic Authentication
 * Returns true if authenticated or security disabled, false otherwise
 */
static bool checkAuth() {
#if !SECURITY_ENABLED
  return true;  // Security disabled at compile time
#endif

  // Rate limiting: simple lockout after repeated failures
  if (g_auth_fail_count >= 3) {
    unsigned long now = millis();
    if (now - g_last_auth_fail < AUTH_LOCKOUT_MS) {
      server.send(429, "text/plain", "Too many failed attempts. Try again later.");
      return false;
    } else {
      // Reset after lockout period
      g_auth_fail_count = 0;
    }
  }

  if (!server.authenticate(AUTH_USERNAME, AUTH_PASSWORD)) {
    g_auth_fail_count++;
    g_last_auth_fail = millis();
    server.requestAuthentication(BASIC_AUTH, "Wombat Manager", "Authentication required");
    return false;
  }

  // Success - reset fail counter
  g_auth_fail_count = 0;
  return true;
}

/**
 * Validate I2C address (7-bit address range)
 */
static bool isValidI2CAddress(uint8_t addr) {
  return (addr >= 0x08 && addr <= 0x77);
}

/**
 * Validate pin number for ESP32
 */
static bool isValidPinNumber(int pin) {
  // ESP32 valid GPIO pins (exclude reserved/strapping pins)
  // Allow common GPIO pins, exclude flash pins (6-11)
  if (pin < 0 || pin > 39) return false;
  if (pin >= 6 && pin <= 11) return false;  // Flash pins
  return true;
}

/**
 * Validate integer is within range
 */
static bool isValidRange(int value, int min_val, int max_val) {
  return (value >= min_val && value <= max_val);
}

/**
 * Enhanced path traversal protection
 * Prevents: .., absolute path escape, null bytes, control chars
 */
static bool isPathSafe(const String& path) {
  // Check for null bytes
  if (path.indexOf('\0') >= 0) return false;
  
  // Check for control characters
  for (size_t i = 0; i < path.length(); i++) {
    char c = path[i];
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return false;
  }
  
  // Check for parent directory references
  if (path.indexOf("..") >= 0) return false;
  
  // Must start with / (absolute within filesystem)
  if (path.length() > 0 && path[0] != '/') return false;
  
  return true;
}

/**
 * Validate filename for filesystem safety
 * Only allows alphanumeric, underscore, dash, dot
 */
static bool isFilenameSafe(const String& filename) {
  if (filename.length() == 0 || filename.length() > 255) return false;
  
  // Check for null bytes
  if (filename.indexOf('\0') >= 0) return false;
  
  // Must not start with dot (hidden files)
  if (filename[0] == '.') return false;
  
  // Check each character
  for (size_t i = 0; i < filename.length(); i++) {
    char c = filename[i];
    bool valid = (c >= 'a' && c <= 'z') || 
                 (c >= 'A' && c <= 'Z') || 
                 (c >= '0' && c <= '9') ||
                 c == '_' || c == '-' || c == '.';
    if (!valid) return false;
  }
  
  return true;
}

/**
 * Sanitize error messages to prevent info disclosure
 */
static String sanitizeError(const String& error) {
  // Remove filesystem paths
  String safe = error;
  safe.replace("/littlefs/", "[FS]/");
  safe.replace("/sd/", "[SD]/");
  safe.replace("/temp/", "[TEMP]/");
  safe.replace("/fw/", "[FW]/");
  safe.replace("/config/", "[CFG]/");
  
  // Limit length
  if (safe.length() > 128) {
    safe = safe.substring(0, 125) + "...";
  }
  
  return safe;
}

/**
 * Validate JSON size before parsing
 */
static bool isJsonSizeSafe(const String& json) {
  return (json.length() <= MAX_JSON_SIZE);
}

/**
 * Check if upload size is within limits
 */
static bool isUploadSizeSafe(size_t size) {
  return (size <= MAX_UPLOAD_SIZE);
}

// ===================================================================================
// HELPERS
// ===================================================================================

// --- Storage layout (ESP32 LittleFS) ---
static const char* FW_DIR = "/fw";
static const char* CFG_DIR = "/config";

// --- Upload status (ESP32 WebServer requires single response in POST handler) ---
static bool g_hexUploadOk = false;
static String g_hexUploadMsg = "";
static String g_hexUploadPath = "";
static File g_hexUploadFile;
static IntelHexSW8B g_hexConv;
static bool g_fwUploadOk = false;
static String g_fwUploadMsg = "";
static String g_fwUploadPath = "";
static File g_fwUploadFile;

// Backward-compat aliases used by some SD handlers
#if SD_SUPPORT_ENABLED
static inline SDFile sd_open(const char* path, oflag_t flags) { return sdOpen(path, flags); }
static inline SDFile sd_open(const String& path, oflag_t flags) { return sdOpen(path.c_str(), flags); }
static inline bool sd_rename(const char* from, const char* to) { return sdRename(from, to); }
static inline bool sd_rename(const String& from, const String& to) { return sdRename(from.c_str(), to.c_str()); }
static inline uint64_t sd_filesize(SDFile& f) {
  // SdFat file type exposes fileSize(); fs::File exposes size().
#if CYD_USE_SDFAT
  return (uint64_t)f.fileSize();
#else
  return (uint64_t)f.size();
#endif
}
#endif


#if SD_SUPPORT_ENABLED
#if 1
// ===================================================================================
// SD Card Runtime State + Helpers (disabled: legacy FS/SD.h implementation)
// ===================================================================================
// Use default SPI instance for SD (portable across ESP32 variants)
// (g_sdMountMsg declared once)

// Upload state for SD uploads
static bool g_sdUploadOk = false;
static String g_sdUploadMsg = "";
static String g_sdUploadPath = "";
// (SdFat migration) g_sdUploadFile is SDFile (see top of SdFat layer)

static bool sdEnsureMounted();

static bool sdPathIsSafe(const String& pth) {
  // Require absolute, forbid traversal
  if (!pth.startsWith("/")) return false;
  if (pth.indexOf("..") >= 0) return false;
  return true;
}

static bool sdEnsureMounted() {
  if (!isSDEnabled) {
    g_sdMounted = false;
    g_sdMountMsg = "SD disabled";
    return false;
  }
  if (g_sdMounted) return true;

  g_sdMountMsg = "Mounting...";
  bool ok = sdMount();
  yield();

  g_sdMounted = ok;
  g_sdMountMsg = ok ? "Mounted" : "No card / mount failed";
  return ok;
}

// Copy an SD file to LittleFS (used by LVGL file browser + fw conversion pipeline)
static bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path) {
  if (!sdEnsureMounted()) return false;
  SDFile in = sdOpen(sd_path, O_RDONLY);
  if (!in) return false;
  File out = LittleFS.open(lfs_path, "w");
  if (!out) { in.close(); return false; }

  uint8_t buf[4096];
  while (true) {
    int r = in.read(buf, sizeof(buf));
    if (r <= 0) break;
    if ((int)out.write(buf, (size_t)r) != r) { in.close(); out.close(); return false; }
    delay(0);
  }
  in.close();
  out.close();
  return true;
}

static uint64_t sd_total_bytes() {
  uint64_t total = 0, used = 0;
  if (!sdEnsureMounted()) return 0;
  if (!sdGetUsage(total, used)) return 0;
  return total;
}

static uint64_t sd_used_bytes() {
  uint64_t total = 0, used = 0;
  if (!sdEnsureMounted()) return 0;
  if (!sdGetUsage(total, used)) return 0;
  return used;
}


static bool sdRemoveRecursive(const String& path) {
  if (!sdMount()) return false;

  // Open to detect file/dir
  SDFile f = sdOpen(path.c_str(), O_RDONLY);
  if (!f) {
    // If it doesn't exist, treat as ok
    return true;
  }
  bool isDir = sdFileIsDir(f);
  f.close();

  if (!isDir) {
    return sdRemove(path.c_str());
  }

  SDFile d = sdOpen(path.c_str(), O_RDONLY);
  if (!d) return false;

  SDFile e;
  while (sdOpenNext(d, e)) {
    String name = sdFileName(e);
    bool childIsDir = sdFileIsDir(e);
    e.close();

    if (!name.length() || name == "." || name == "..") continue;
    String child = path + "/" + name;

    bool ok = childIsDir ? sdRemoveRecursive(child) : sdRemove(child.c_str());
    if (!ok) { d.close(); return false; }
    delay(0);
  }
  d.close();

  // remove directory itself (SdFat: rmdir, SD.h: rmdir)
  return sdRmdir(path.c_str());
}
#endif

static String sanitizeBasename(const String& name) {
  // Keep only a safe filename (no slashes, no traversal)
  String out;
  out.reserve(name.length());
  for (size_t i = 0; i < name.length(); i++) {
    char c = name[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.') {
      out += c;
    }
  }
  if (out.length() == 0) out = "file.bin";
  return out;
}

// Minimal JSON string escaper for web API responses
static String jsonEscape(const String& s) {
  String out;
  out.reserve(s.length() + 8);
  for (size_t i = 0; i < s.length(); i++) {
    const char c = s[i];
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"':  out += "\\\""; break;
      case '\b': out += "\\b"; break;
      case '\f': out += "\\f"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if ((uint8_t)c < 0x20) {
          char buf[7];
          snprintf(buf, sizeof(buf), "\\u%04X", (unsigned)(uint8_t)c);
          out += buf;
        } else {
          out += c;
        }
        break;
    }
  }
  return out;
}

static String joinPath(const String& dir, const String& base) {
  String d = dir;
  if (!d.startsWith("/")) d = "/" + d;
  if (d.endsWith("/")) d.remove(d.length()-1);
  String b = base;
  if (b.startsWith("/")) b.remove(0, 1);
  return d + "/" + b;
}

static String normalizePath(String p) {
  // Preserve legacy behavior: ensure leading slash
  if (p.length() == 0) return String("/");
  if (!p.startsWith("/")) p = "/" + p;
  return p;
}

static void fsListFilesBySuffixInDir(const char* dirPath, const char* suffix, String& outOptionsHtml, bool& foundAny) {
  File dir = LittleFS.open(dirPath);
  if (!dir || !dir.isDirectory()) return;
  File file = dir.openNextFile();
  while (file) {
    String n = String(file.name());
    if (n.endsWith(suffix)) {
      outOptionsHtml += "<option value='" + n + "'>" + n + "</option>";
      foundAny = true;
    }
    file.close();
    file = dir.openNextFile();
  }
  dir.close();
}

static void fsListFilesBySuffix(const char* suffix, String& outOptionsHtml, bool& foundAny) {
  foundAny = false;

  // Prefer /fw for firmware blobs
  fsListFilesBySuffixInDir(FW_DIR, suffix, outOptionsHtml, foundAny);

  // Back-compat: also include legacy root-stored files
  File root = LittleFS.open("/");
  if (!root || !root.isDirectory()) return;
  File file = root.openNextFile();
  while (file) {
    String n = String(file.name());
    if (n.endsWith(suffix)) {
      outOptionsHtml += "<option value='" + n + "'>" + n + "</option>";
      foundAny = true;
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

// ===================================================================================
// ROUTE HANDLERS (Dashboard + Tools)
// ===================================================================================
// ===================================================================================
// Firmware Flasher (v06 - row-by-row writing) PRESERVED
// ===================================================================================
static bool ensureDir(const char* path) {
  if (LittleFS.exists(path)) return true;
  return LittleFS.mkdir(path);
}

static bool parseHexWordTokenToU16(const char* tok, uint16_t& out) {
  // Accept formats like 0x1234 or 1234
  const char* p = tok;
  while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
  if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) p += 2;
  uint32_t v = 0;
  int digits = 0;
  while (*p) {
    char c = *p++;
    if (c == ',' || c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
    uint8_t n;
    if (c >= '0' && c <= '9') n = c - '0';
    else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
    else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
    else return false;
    v = (v << 4) | n;
    digits++;
    if (digits > 4) return false;
  }
  if (digits == 0) return false;
  out = (uint16_t)v;
  return true;
}

static bool convertFwTxtToBin(const char* fwTxtPath, const char* outBinPath, String& err) {
  File in = LittleFS.open(fwTxtPath, "r");
  if (!in) { err = String("Open failed: ") + fwTxtPath; return false; }

  // Create output
  File out = LittleFS.open(outBinPath, "w");
  if (!out) { in.close(); err = String("Open failed: ") + outBinPath; return false; }

  // Stream parse tokens to keep heap stable
  char tok[16];
  int tlen = 0;

  uint8_t outBuf[512];
  size_t outLen = 0;

  auto flushOut = [&]() {
    if (outLen) {
      size_t w = out.write(outBuf, outLen);
      outLen = 0;
      return w > 0;
    }
    return true;
  };

  while (in.available()) {
    int c = in.read();
    if (c < 0) break;
    char ch = (char)c;

    // Token separators: comma or whitespace
    if (ch == ',' || ch == '\n' || ch == '\r' || ch == ' ' || ch == '\t') {
      if (tlen > 0) {
        tok[tlen] = 0;
        uint16_t w;
        if (!parseHexWordTokenToU16(tok, w)) {
          in.close();
          out.close();
          LittleFS.remove(outBinPath);
          err = String("Parse error near token: ") + tok;
          return false;
        }
        // little-endian bytes
        outBuf[outLen++] = (uint8_t)(w & 0xFF);
        outBuf[outLen++] = (uint8_t)((w >> 8) & 0xFF);

        if (outLen >= sizeof(outBuf) - 2) {
          if (!flushOut()) {
            in.close(); out.close();
            LittleFS.remove(outBinPath);
            err = "Write failed";
            return false;
          }
        }
        tlen = 0;
      }
      continue;
    }

    // accumulate token chars
    if (tlen < (int)sizeof(tok) - 1) {
      tok[tlen++] = ch;
    } else {
      in.close(); out.close();
      LittleFS.remove(outBinPath);
      err = "Token too long";
      return false;
    }

    // Be cooperative with WiFi/WDT
    if ((in.position() & 0x3FF) == 0) { yield(); ArduinoOTA.handle(); }
  }

  // final token
  if (tlen > 0) {
    tok[tlen] = 0;
    uint16_t w;
    if (!parseHexWordTokenToU16(tok, w)) {
      in.close(); out.close();
      LittleFS.remove(outBinPath);
      err = String("Parse error near token: ") + tok;
      return false;
    }
    outBuf[outLen++] = (uint8_t)(w & 0xFF);
    outBuf[outLen++] = (uint8_t)((w >> 8) & 0xFF);
  }

  if (!flushOut()) {
    in.close(); out.close();
    LittleFS.remove(outBinPath);
    err = "Write failed";
    return false;
  }

  in.close();
  out.close();
  return true;
}

static bool convertHexToFirmwareBin(const String& tempHexPath, const String& outBinPath, String& outWarnOrErr) {
  // Use a deterministic temp output in /temp
  String tempFwTxt = String("/temp/fw_conv.txt");

  if (!g_hexConv.begin(LittleFS, "/hexcache")) {
    outWarnOrErr = "Hex converter init failed";
    return false;
  }

  g_hexConv.clearCache();

  if (!g_hexConv.loadHexFile(tempHexPath.c_str(), false)) {
    outWarnOrErr = "HEX load failed";
    return false;
  }

  // Strict 16KB window export
  if (!g_hexConv.exportFW_CH32V003_16K_Strict(tempFwTxt.c_str(), true, false)) {
    outWarnOrErr = g_hexConv.warnings();
    if (outWarnOrErr.length() == 0) outWarnOrErr = "HEX export failed";
    // try cleanup
    LittleFS.remove(tempFwTxt);
    return false;
  }

  String err;
  bool ok = convertFwTxtToBin(tempFwTxt.c_str(), outBinPath.c_str(), err);

  // Always delete intermediate text
  LittleFS.remove(tempFwTxt);

  if (!ok) {
    outWarnOrErr = err;
    return false;
  }

  // Provide warnings (non-fatal) to UI
  outWarnOrErr = g_hexConv.warnings();
  return true;
}

// ===================================================================================
// Firmware Flasher (v06 - row-by-row writing) PRESERVED
// ===================================================================================
#endif



// ===================================================================================
// Path / Filename Helpers (SD + LittleFS)
// ===================================================================================
static String makeFileSafeName(const String& in) {
  String s; s.reserve(in.length());
  for (size_t i=0;i<in.length();i++) {
    char c=in[i];
    if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.') s+=c;
    else if (c==' ') s+='_';
  }
  if (s.length()==0) s = "file";
  return s;
}

static String sanitizePath(const String& raw) {
  String p = raw;
  p.trim();
  if (p.length()==0) return "/";
  // force leading slash
  if (p[0] != '/') p = "/" + p;
  // collapse //
  while (p.indexOf("//") >= 0) p.replace("//","/");
  // remove .. segments
  while (p.indexOf("..") >= 0) p.replace("..","_");
  // no trailing slash unless root
  if (p.length()>1 && p.endsWith("/")) p.remove(p.length()-1);
  return p;
}

static const char* TEMP_DIR = "/tmp";
static String ensureTempPathForUpload(const char* leafName) {
  String leaf = makeFileSafeName(String(leafName));
  String dir = String(TEMP_DIR);
  if (!LittleFS.exists(dir)) LittleFS.mkdir(dir);
  return dir + String("/") + leaf;
}


// ===================================================================================
// SYSTEM SETTINGS API
// ===================================================================================
static String fwSlotPath(const String& prefix, const String& version) {
  String name = makeFileSafeName(prefix + String("_") + version + String(".bin"));
  return joinPath(FW_DIR, name);
}

static void fsCleanSlot(const String& prefix) {
  // Same logic as handleCleanSlot, but callable from other handlers
  String toDelete[64];
  int delCount = 0;

  auto collectInDir = [&](const char* dirPath) {
    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) return;
    SDFile f;
    while (f) {
      String n = String(f.name());
      String base = n;
      int slash = base.lastIndexOf('/');
      if (slash >= 0) base = base.substring(slash + 1);
      if (base.startsWith(prefix + "_") && base.endsWith(".bin")) {
        if (delCount < 64) toDelete[delCount++] = n;
      }
      f.close();
      f.close();
    // next

    }
    dir.close();
  };

  collectInDir(FW_DIR);
  collectInDir("/");

  for (int i = 0; i < delCount; i++) {
    LittleFS.remove(toDelete[i]);
  }
}

static bool copyFileBuffered(fs::FS& srcFS, const String& srcPath, fs::FS& dstFS, const String& dstPath, String& err) {
  File in = srcFS.open(srcPath, "r");
  if (!in) { err = String("Open failed: ") + srcPath; return false; }

  // Ensure destination parent exists for LittleFS
  if (&dstFS == &LittleFS) {
    int slash = dstPath.lastIndexOf('/');
    if (slash > 0) {
      String parent = dstPath.substring(0, slash);
      if (!LittleFS.exists(parent)) LittleFS.mkdir(parent);
    }
  }

  File out = dstFS.open(dstPath, "w");
  if (!out) { in.close(); err = String("Open failed: ") + dstPath; return false; }

  uint8_t buf[4096];
  while (true) {
    size_t r = in.read(buf, sizeof(buf));
    if (r == 0) break;
    size_t w = out.write(buf, r);
    if (w != r) {
      in.close(); out.close();
      err = "Write failed";
      return false;
    }
    yield();
  }
  in.close();
  out.close();
  return true;
}

static bool copyFileBufferedSdToFS(const String& srcPath, const String& dstPath, String& err) {
#if SD_SUPPORT_ENABLED
  if (!g_sdMounted && !sdMount()) { err = "SD not mounted"; return false; }
  SDFile in = sdOpen(srcPath.c_str(), O_RDONLY);
  if (!in) { err = "Open src failed"; return false; }
  File out = LittleFS.open(dstPath, "w");
  if (!out) { in.close(); err = "Open dst failed"; return false; }
  uint8_t buf[2048];
  int n;
  while ((n = in.read(buf, sizeof(buf))) > 0) {
    if (out.write(buf, n) != (size_t)n) { out.close(); in.close(); err = "Write failed"; return false; }
  }
  out.close();
  in.close();
  return true;
#else
  (void)srcPath; (void)dstPath; err = "SD disabled"; return false;
#endif
}


static bool fwTxtToBin(const String& inPath, const String& outBinPath, String& err, bool fromSD = false) {
  if (!fromSD) {
    return convertFwTxtToBin(inPath.c_str(), outBinPath.c_str(), err);
  }
#if SD_SUPPORT_ENABLED
  // Stream copy SD -> LittleFS temp, then run existing converter on LittleFS
  String tempIn = String("/temp/sd_fw.txt");
  String copyErr;
  if (!sdCopyToLittleFS(inPath.c_str(), tempIn.c_str())) {
    err = copyErr;
    return false;
  }
  bool ok = convertFwTxtToBin(tempIn.c_str(), outBinPath.c_str(), err);
  LittleFS.remove(tempIn);
  return ok;
#else
  err = "SD support disabled";
  return false;
#endif
}
// ===================================================================================
// SETUP / LOOP
// ===================================================================================
void setup() {
  Serial.begin(115200);

  // Keep WiFi responsive during long flash operations
  WiFi.setSleep(false);

  // LittleFS (ESP32) - auto format on first mount failure
  if (!LittleFS.begin(true)) {
    LittleFS.format();
    LittleFS.begin(true);
  }

  // Load (or create) CYD runtime config
  if (!loadConfig(g_cfg)) {
    setConfigDefaults(g_cfg);
    // first boot (not configured yet)
    g_cfg.configured = false;
    saveConfig(g_cfg);
  }

  // Apply dynamic I2C pins from config *before* Wire.begin
  Wire.begin(g_cfg.i2c_sda, g_cfg.i2c_scl);
  Wire.setClock(100000);

  sw.begin(Wire, currentWombatAddress);

  // Ensure storage folders exist
  if (!LittleFS.exists(FW_DIR)) LittleFS.mkdir(FW_DIR);
  if (!LittleFS.exists(CFG_DIR)) LittleFS.mkdir(CFG_DIR);
  if (!LittleFS.exists("/temp")) LittleFS.mkdir("/temp");
  if (!LittleFS.exists("/hexcache")) LittleFS.mkdir("/hexcache");

#if DISPLAY_SUPPORT_ENABLED
  // Initialize local display stack (LovyanGFX + LVGL) if enabled
  if (g_cfg.display_enable && g_cfg.lvgl_enable && !g_cfg.headless) {
    if (lvglInitIfEnabled()) {
      if (!g_cfg.configured) {
        firstBootShowModelSelect();
      }
    }
  }
#endif

  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Wombat-Setup")) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Wombat-Setup");
  }
  
  // Security warning on startup
  #if SECURITY_ENABLED
  Serial.println("\n*** SECURITY WARNING ***");
  Serial.println("Authentication is ENABLED");
  Serial.print("Username: ");
  Serial.println(AUTH_USERNAME);
  if (strcmp(AUTH_PASSWORD, "CHANGE_ME_NOW") == 0) {
    Serial.println("*** DEFAULT PASSWORD DETECTED ***");
    Serial.println("*** CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY ***");
    Serial.println("*** SYSTEM IS NOT SECURE WITH DEFAULT PASSWORD ***");
  }
  Serial.println("************************\n");
  #else
  Serial.println("\n*** WARNING: SECURITY DISABLED ***\n");
  #endif

  // Dashboard + Tools
  server.on("/", []() { handleRoot(server); });
  server.on("/scanner", []() { handleScanner(server); });
  server.on("/scan-data", []() { handleScanData(server); });
  server.on("/deepscan", []() { handleDeepScan(server); });
  server.on("/connect", []() { handleConnect(server); });
  server.on("/setpin", []() { handleSetPin(server); });
  server.on("/changeaddr", []() { handleChangeAddr(server); });
  server.on("/resetwifi", []() { handleResetWiFi(server); });
  server.on("/flashfw", HTTP_POST, []() { handleFlashFW(server); });
  server.on(
    "/upload_fw",
    HTTP_POST,
    []() {
      // ESP32 WebServer: send exactly ONE response from the POST handler.
      if (g_fwUploadOk) {
        server.send(200, "text/plain", "Saved.");
      } else {
        String msg = g_fwUploadMsg.length() ? g_fwUploadMsg : String("Upload failed");
        server.send(500, "text/plain", msg);
      }
    },
    []() { handleUploadFW(server); }
  );

  server.on(
    "/upload_hex",
    HTTP_POST,
    []() { handleUploadHexPost(server); },
    []() { handleUploadHex(server); }
  );

  server.on("/clean_slot", []() { handleCleanSlot(server); });
  server.on("/resetwombat", []() { handleResetTarget(server); });
  server.on("/formatfs", []() { handleFormat(server); });

  // Configurator UI
  server.on("/configure", []() { server.send(200, "text/html", FPSTR(CONFIG_HTML)); });

  // System Settings UI
  server.on("/settings", []() { server.send(200, "text/html", FPSTR(SETTINGS_HTML)); });

  // Health check endpoint (public, no auth)
  server.on("/api/health", HTTP_GET, []() { handleApiHealth(server); });
  
  // System Settings API
  server.on("/api/system", HTTP_GET, []() { handleApiSystem(server); });

#if SD_SUPPORT_ENABLED
  // SD Card Manager API (only registered when enabled)
  if (isSDEnabled) {
    server.on("/api/sd/status", HTTP_GET, []() { handleApiSdStatus(server); });
    server.on("/api/sd/list", HTTP_GET, []() { handleApiSdList(server); });
    server.on("/api/sd/delete", HTTP_POST, []() { handleApiSdDelete(server); });
    server.on("/api/sd/rename", HTTP_POST, []() { handleApiSdRename(server); });
    server.on("/api/sd/eject", HTTP_POST, []() { handleApiSdEject(server); });
    server.on("/api/sd/import_fw", HTTP_POST, []() { handleApiSdImportFw(server); });
    server.on("/api/sd/convert_fw", HTTP_POST, []() { handleApiSdImportFw(server); });
    server.on("/sd/download", HTTP_GET, []() { handleSdDownload(server); });
    server.on("/api/sd/upload", HTTP_POST, []() { handleUploadSdPost(server); }, []() { handleUploadSD(server); });
  }
#endif

  // Configurator API
  server.on("/api/variant", HTTP_GET, []() { handleApiVariant(server); });
  server.on("/api/apply", HTTP_POST, []() { handleApiApply(server); });
  server.on("/api/config/save", HTTP_POST, []() { handleConfigSave(server); });
  server.on("/api/config/load", HTTP_GET, []() { handleConfigLoad(server); });
  server.on("/api/config/list", HTTP_GET, []() { handleConfigList(server); });
  server.on("/api/config/exists", HTTP_GET, []() { handleConfigExists(server); });
  server.on("/api/config/delete", HTTP_GET, []() { handleConfigDelete(server); });

  server.begin();
  tcpServer.begin();
  
  // Configure OTA security
  ArduinoOTA.setPassword(AUTH_PASSWORD);
  ArduinoOTA.setHostname("wombat-bridge");
  
  // OTA security callbacks
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.println("OTA Update Start: " + type);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  
  ArduinoOTA.begin();
}

void loop() {
  ArduinoOTA.handle();
  server.handleClient();
  handleTcpBridge();

#if DISPLAY_SUPPORT_ENABLED
  if (g_lvgl_ready) {
    static uint32_t last = millis();
    uint32_t now = millis();
    uint32_t dt = now - last;
    last = now;
#if LVGL_VERSION_MAJOR >= 9
    lv_tick_set(lv_tick_get() + dt);
#else
  #if defined(LVGL_VERSION_MAJOR) && (LVGL_VERSION_MAJOR < 9)
    // LVGL v8 uses a software tick increment.
    lv_tick_inc(dt);
  #endif
#endif
    lvglTickAndUpdate();
  }
#endif
}
