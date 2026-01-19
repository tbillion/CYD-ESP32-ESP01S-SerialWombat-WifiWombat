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
static volatile uint32_t g_i2c_tx_count = 0;
static volatile uint32_t g_i2c_rx_count = 0;
static volatile bool g_i2c_tx_blink = false;
static volatile bool g_i2c_rx_blink = false;
static inline void i2cMarkTx() { g_i2c_tx_count++; g_i2c_tx_blink = true; }
static inline void i2cMarkRx() { g_i2c_rx_count++; g_i2c_rx_blink = true; }

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

  // Upload state
  static bool   g_sdUploadOk = false;
  static String g_sdUploadMsg;
  #include <SPI.h>

	// SD library selection (Arduino IDE friendly):
	// This sketch is intended to compile out-of-the-box on Arduino-ESP32 v3.x
	// using the core-provided SD.h (fs::File). SdFat is OPTIONAL; if you want it,
	// you can re-enable it manually, but the default is SD.h.
	#define CYD_USE_SDFAT 0
	#include <SD.h>

  // If we're on SD.h (not SdFat), define a minimal flag API used elsewhere.
  typedef uint32_t oflag_t;
  #ifndef O_RDONLY
    #define O_RDONLY 0x0001
  #endif
  #ifndef O_WRITE
    #define O_WRITE  0x0002
  #endif
  #ifndef O_RDWR
    #define O_RDWR   (O_RDONLY | O_WRITE)
  #endif
  #ifndef O_CREAT
    #define O_CREAT  0x0004
  #endif
  #ifndef O_TRUNC
    #define O_TRUNC  0x0008
  #endif

// ===================================================================================
// SD Abstraction Layer (SdFat preferred, SD.h fallback)
// - Keeps the rest of the sketch stable regardless of which SD library you have.
// ===================================================================================
#if SD_SUPPORT_ENABLED

#if CYD_USE_SDFAT
  // ---- SdFat backend ----
  static SdFat sd;
  typedef FsFile SDFile;
  static bool sdMount() {
    if (g_sdMounted) return true;
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!sd.begin(SD_CS, SD_SCK_MHZ(12))) {
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

#else
  // ---- SD.h backend ----
  typedef File SDFile;
  static bool sdMount() {
    if (g_sdMounted) return true;
    SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
    if (!SD.begin(SD_CS, SPI, 12000000)) {
      g_sdMounted = false;
      g_sdMountMsg = "SD mount failed";
      return false;
    }
    g_sdMounted = true;
    g_sdMountMsg = "OK";
    return true;
  }

  static void sdUnmount() {
    // SD.h does not reliably support end() on all cores; treat as logical unmount.
    g_sdMounted = false;
  }

  static bool sdExists(const char* path) { if (!g_sdMounted && !sdMount()) return false; return SD.exists(path); }
  static bool sdMkdir(const char* path)  { if (!g_sdMounted && !sdMount()) return false; return SD.mkdir(path); }
  static bool sdRemove(const char* path) { if (!g_sdMounted && !sdMount()) return false; return SD.remove(path); }
  static bool sdRmdir(const char* path)  { if (!g_sdMounted && !sdMount()) return false; return SD.rmdir(path); }
  static bool sdRename(const char* f, const char* t) { if (!g_sdMounted && !sdMount()) return false; return SD.rename(f, t); }

  static SDFile sdOpen(const char* path, oflag_t flags) {
    if (!g_sdMounted && !sdMount()) return SDFile();
    // Map flags to mode string.
    const char* mode = "r";
    if (flags & O_WRITE) {
      if (flags & O_APPEND) mode = "a";
      else mode = "w"; // includes O_CREAT/O_TRUNC semantics
    }
    return SD.open(path, mode);
  }

  static bool sdIsDir(const char* path) {
    SDFile f = sdOpen(path, O_RDONLY);
    if (!f) return false;
    bool isDir = f.isDirectory();
    f.close();
    return isDir;
  }

  static bool sdGetStats(uint64_t &totalBytes, uint64_t &usedBytes) {
    // SD.h doesn't expose reliable capacity info across cores.
    totalBytes = 0; usedBytes = 0;
    return false;
  }

#endif

// ---- Unified helpers (work with SdFat or SD.h) ----
static bool sdFileIsDir(SDFile &f) {
#if CYD_USE_SDFAT
  return f.isDir();
#else
  return f.isDirectory();
#endif
}

static String sdFileName(SDFile &f) {
#if CYD_USE_SDFAT
  char nm[96];
  nm[0]=0;
  f.getName(nm, sizeof(nm));
  return String(nm);
#else
  const char* n = f.name();
  return n ? String(n) : String("");
#endif
}

static bool sdOpenNext(SDFile &dir, SDFile &out) {
#if CYD_USE_SDFAT
  SDFile tmp;
  if (!tmp.openNext(&dir, O_RDONLY)) return false;
  out = tmp;
  return (bool)out;
#else
  out = dir.openNextFile();
  return (bool)out;
#endif
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

  cfg.splash_path = String((const char*)(doc["splash"] | cfg.splash_path.c_str()));

  // If the config says headless, forcibly disable local stack.
  if (cfg.headless) {
    cfg.display_enable = false;
    cfg.touch_enable = false;
    cfg.lvgl_enable = false;
    cfg.panel = PANEL_NONE;
    cfg.touch = TOUCH_NONE;
  }

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

// Pin Mode Strings
const char* const pinModeStrings[] PROGMEM = {
  "DIGITAL_IO", "CONTROLLED", "ANALOGINPUT", "SERVO", "THROUGHPUT_CONSUMER",
  "QUADRATURE_ENC", "HBRIDGE", "WATCHDOG", "PROTECTEDOUTPUT", "COUNTER",
  "DEBOUNCE", "TM1637", "WS2812", "SW_UART", "INPUT_PROCESSOR",
  "MATRIX_KEYPAD", "PWM", "UART0_TXRX", "PULSE_TIMER", "DMA_PULSE_OUTPUT",
  "ANALOG_THROUGHPUT", "FRAME_TIMER", "TOUCH", "UART1_TXRX", "RESISTANCE_INPUT",
  "PULSE_ON_CHANGE", "HF_SERVO", "ULTRASONIC_DISTANCE", "LIQUID_CRYSTAL",
  "HS_CLOCK", "HS_COUNTER", "VGA", "PS2_KEYBOARD", "I2C_CONTROLLER",
  "QUEUED_PULSE_OUTPUT", "MAX7219MATRIX", "FREQUENCY_OUTPUT", "IR_RX",
  "IR_TX", "RC_PPM", "BLINK"
};

// ===================================================================================
// --- HTML: Dashboard (wifiwombat06 - PRESERVED) ---
// ===================================================================================
const char INDEX_HTML_HEAD[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Wombat Manager</title>
  <style>
    body { font-family: 'Segoe UI', sans-serif; text-align: center; background: #222; color: #eee; margin:0; padding:10px; }
    .card { background: #333; padding: 15px; margin: 10px auto; max-width: 450px; border-radius: 8px; border: 1px solid #444; }
    h2 { color: #00d2ff; margin: 0 0 10px 0; }
    h3 { border-bottom: 1px solid #555; padding-bottom: 5px; margin: 10px 0; font-size: 1.1em; color: #ccc;}
    button { background: #007acc; color: white; padding: 12px; border: none; border-radius: 4px; cursor: pointer; margin: 5px; width: 100%; font-size:1rem; }
    button.scan { background: #28a745; }
    button.deep { background: #6f42c1; }
    button.flash { background: #d35400; font-weight:bold; }
    button.danger { background: #c0392b; }
    button.warn { background: #e67e22; color: #fff; }
    select, input[type=text], textarea { padding: 10px; margin: 5px; width: 95%; background: #ddd; border: none; border-radius: 4px; box-sizing: border-box; }
    input[type=radio] { transform: scale(1.5); margin: 10px; }
    .status { font-family: monospace; color: #0f0; }
    .warn-text { color: #e74c3c; font-size: 0.8rem; font-weight: bold; }
    .slot-label { display: block; text-align: left; padding: 5px; background: #444; margin-bottom: 2px; border-radius: 4px; cursor: pointer;}
    .slot-label:hover { background: #555; }
  </style>
</head>
<body>
  <h2>Wombat Wifi Bridge</h2>
  
  <div class="card">
    <div style="text-align:left; font-size:0.9em;">
      <strong>Bridge IP:</strong> <span class="status">%IP%</span> (Port 3000)<br>
      <strong>Target:</strong> <span class="status">0x%ADDR%</span>
    </div>
  </div>

  <div class="card">
    <h3>Scanner Tools</h3>
    <button class="scan" onclick="location.href='/scanner'">Fast I2C Scanner</button>
    <button class="deep" onclick="location.href='/deepscan'">Deep Chip Analysis</button>
  </div>

  <div class="card" style="border:1px solid #00d2ff;">
    <h3>Firmware Manager</h3>
    <button onclick="document.getElementById('uploadUi').style.display='block';">Update a Firmware Slot</button>
    
    <div id="uploadUi" style="display:none; text-align:left; background:#222; padding:10px; margin-top:10px;">
      
      <h4>1. Select Slot to Update:</h4>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Default_FW" checked> Default Firmware</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Brushed_Motor"> Brushed Motor</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Front_Panel"> Front Panel</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Keypad"> Keypad</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Motor_Control"> Motor Control</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="TM1637"> TM1637 Display</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Ultrasonic"> Ultrasonic</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Comms"> Communications</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Custom1"> Custom Slot 1</label>
      <label class="slot-label"><input type="radio" name="fwSlot" value="Custom2"> Custom Slot 2</label>

      <h4>2. Enter Version:</h4>
      <input type="text" id="fwVer" placeholder="e.g. 2.1.2">

      <h4>3. Input Method:</h4>
      <div style="display:flex; gap:18px; align-items:center; flex-wrap:wrap;">
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="blob" checked onchange="setFwInputMode('blob')"> Blob
        </label>
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="hex" onchange="setFwInputMode('hex')"> IntelHEX
        </label>
        %SD_FW_OPTION%
      </div>

      <div id="fwBlobArea" style="margin-top:10px;">
        <h4>4. Paste Array Code:</h4>
        <textarea id="fwContent" rows="5" placeholder="0x306F, 0x37A0, ..."></textarea>
      </div>

      <div id="fwHexArea" style="display:none; margin-top:10px;">
        <h4>4. Upload Intel HEX (.hex):</h4>
        <input type="file" id="fwHexFile" accept=".hex" style="display:none" />
        <button class="scan" type="button" onclick="document.getElementById('fwHexFile').click();">Choose .hex File</button>
        <div id="fwHexName" class="status" style="margin-top:6px; font-size:0.9em; word-break:break-all;"></div>
      </div>

      %SD_FW_AREA%

      <button class="scan" onclick="uploadFW()">Save & Overwrite Slot</button>
      <div id="uploadStatus"></div>
    </div>
    
    <script>
      function setFwInputMode(mode) {
        var blobArea = document.getElementById('fwBlobArea');
        var hexArea  = document.getElementById('fwHexArea');
        var sdArea   = document.getElementById('fwSdArea');

        if (blobArea) blobArea.style.display = 'none';
        if (hexArea)  hexArea.style.display  = 'none';
        if (sdArea)   sdArea.style.display   = 'none';

        if (mode === 'hex') {
          if (hexArea) hexArea.style.display = 'block';
        } else if (mode === 'sd') {
          if (sdArea) sdArea.style.display = 'block';
          try { if (typeof refreshSdFwList === 'function') refreshSdFwList(); } catch(e) {}
        } else {
          if (blobArea) blobArea.style.display = 'block';
        }
      }

      // Keep filename display in HEX mode
      (function(){
        var f = document.getElementById('fwHexFile');
        if (f) {
          f.addEventListener('change', function(){
            var n = (f.files && f.files.length) ? f.files[0].name : '';
            var lbl = document.getElementById('fwHexName');
            if (lbl) lbl.textContent = n ? ('Selected: ' + n) : '';
          });
        }
      })();

      function getSelectedSlot() {
        var slots = document.getElementsByName('fwSlot');
        for (var i=0; i<slots.length; i++) {
          if (slots[i].checked) return slots[i].value;
        }
        return '';
      }

      function getInputMode() {
        var modes = document.getElementsByName('fwInputMode');
        for (var i=0; i<modes.length; i++) {
          if (modes[i].checked) return modes[i].value;
        }
        return 'blob';
      }

      function uploadFW() {
        var mode = getInputMode();
        if (mode === 'hex') return uploadFW_hex();
        if (mode === 'sd') return uploadFW_sd();
        return uploadFW_blob();
      }

      function uploadFW_blob() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var text = document.getElementById('fwContent').value;

        if(!ver || !text) { alert("Please enter Version and Paste Code."); return; }

        var finalName = slotName + "_" + ver;
        var status = document.getElementById('uploadStatus');
        status.innerHTML = "Cleaning old files...";

        // 1. Clean Slot First
        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
            status.innerHTML = "Parsing Hex...";
            // 2. Parse Hex
            var hexValues = text.match(/0x[0-9A-Fa-f]{1,4}/g);
            if(!hexValues) { status.innerHTML = "Error: No hex found."; return; }

            var bytes = new Uint8Array(hexValues.length * 2);
            for(var i=0; i<hexValues.length; i++) {
               var val = parseInt(hexValues[i], 16);
               bytes[i*2] = val & 0xFF;
               bytes[i*2+1] = (val >> 8) & 0xFF;
            }

            status.innerHTML = "Uploading " + bytes.length + " bytes...";

            // 3. Upload
            var blob = new Blob([bytes], {type: "application/octet-stream"});
            var fd = new FormData();
            fd.append("file", blob, finalName + ".bin");

            return fetch('/upload_fw', {method:'POST', body:fd});
        })
        .then(r => r.text())
        .then(() => {
             status.innerHTML = "Success! Reloading...";
             setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = "Error: " + e);
      }

      function uploadFW_hex() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var f = document.getElementById('fwHexFile');
        var file = (f && f.files && f.files.length) ? f.files[0] : null;

        if(!ver) { alert("Please enter Version."); return; }
        if(!file) { alert("Please choose a .hex file."); return; }

        var status = document.getElementById('uploadStatus');
        status.innerHTML = "Cleaning old files...";

        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
          status.innerHTML = "Uploading HEX...";
          var fd = new FormData();
          fd.append('file', file, file.name);
          var url = '/upload_hex?prefix=' + encodeURIComponent(slotName) + '&ver=' + encodeURIComponent(ver);
          return fetch(url, {method:'POST', body:fd});
        })
        .then(r => r.text())
        .then(t => {
          status.innerHTML = t + "<br>Reloading...";
          setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = "Error: " + e);
      }

      function uploadFW_sd() {
        var slotName = getSelectedSlot();
        var ver = document.getElementById('fwVer').value.trim();
        var sel = document.getElementById('fwSdSelect');
        var sdPath = sel ? sel.value : '';

        if (!ver) { alert('Please enter Version.'); return; }
        if (!sdPath) { alert('Please select a file from SD.'); return; }

        var status = document.getElementById('uploadStatus');
        status.innerHTML = 'Cleaning old files...';

        fetch('/clean_slot?prefix=' + encodeURIComponent(slotName))
        .then(() => {
          status.innerHTML = 'Importing from SD...';
          return fetch('/api/sd/import_fw', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ path: sdPath, slot: slotName, ver: ver })
          });
        })
        .then(r => r.text())
        .then(t => {
          status.innerHTML = t + '<br>Reloading...';
          setTimeout(() => location.reload(), 1500);
        })
        .catch(e => status.innerHTML = 'Error: ' + e);
      }
    </script>
  </div>

  <div class="card" style="border:1px solid #d35400;">
    <h3 style="color:#e67e22;">Firmware Flasher</h3>
    <p class="warn-text">WARNING: Do not power off during update.</p>
    <form action="/flashfw" method="POST">
      <label>Select Firmware from Storage:</label>
      <select name="fw_name">
)rawliteral";

const char INDEX_HTML_TAIL[] PROGMEM = R"rawliteral(
      </select>
      <button class="flash" type="submit" onclick="return confirm('Ready to Flash SW8B? This takes about 30 seconds.');">Start Flash Update</button>
    </form>
  </div>

  <div class="card">
    <h3>Settings</h3>
    <form action="/connect" method="GET">
      <input type="text" name="addr" placeholder="Hex (e.g. 0x6C)"><br>
      <button type="submit">Connect to Address</button>
    </form>
    
    <button class="warn" onclick="if(confirm('Reset Wombat at Target Address?')) location.href='/resetwombat'">Reset Target Wombat</button>
    
    <hr style="border-color:#555;">
    <form action="/changeaddr" method="GET">
      <input type="text" name="newaddr" placeholder="Change Hardware Addr"><br>
      <button class="danger" type="submit">Flash New Address</button>
    </form>
    <br>
    <button onclick="location.href='/resetwifi'">Reset WiFi</button>
    <button onclick="if(confirm('Delete ALL firmwares?')) location.href='/formatfs'" style="background:#555;font-size:0.7em;">Format Storage</button>
  </div>

  %SD_TILE%
</body>
</html>
)rawliteral";



#if SD_SUPPORT_ENABLED
// ===================================================================================
// --- SD Card Management UI Fragments (injected conditionally) ---
// ===================================================================================
const char SD_FW_OPTION_HTML[] PROGMEM = R"rawliteral(
        <label style="display:inline-flex; align-items:center; gap:8px;">
          <input type="radio" name="fwInputMode" value="sd" onchange="setFwInputMode('sd')"> SD_Custom
        </label>
)rawliteral";

const char SD_FW_AREA_HTML[] PROGMEM = R"rawliteral(
      <div id="fwSdArea" style="display:none; margin-top:10px;">
        <h4>4. Pick from SD Card:</h4>
        <div style="display:flex; gap:10px; align-items:center; flex-wrap:wrap;">
          <select id="fwSdSelect" style="flex:1; min-width:220px;"></select>
          <button class="scan" type="button" onclick="refreshSdFwList()">Refresh</button>
        </div>
        <small style="color:#aaa; display:block; margin-top:6px;">Select a file from the SD card root. Supports .bin (direct import), .txt (FW array text), or .hex (convert).</small>
      </div>

      <script>
      async function refreshSdFwList() {
        const sel = document.getElementById('fwSdSelect');
        if (!sel) return;
        sel.innerHTML = '';
        try {
          const r = await fetch('/api/sd/list?path=/');
          if (!r.ok) { sel.innerHTML = '<option value="">SD not available</option>'; return; }
          const j = await r.json();
          const files = (j && j.entries) ? j.entries : [];
          const keep = files.filter(e => !e.isDir && (e.name.endsWith('.bin') || e.name.endsWith('.hex') || e.name.endsWith('.txt')));
          if (!keep.length) { sel.innerHTML = '<option value="">No .bin/.hex/.txt found</option>'; return; }
          keep.forEach(e => {
            const opt = document.createElement('option');
            opt.value = '/' + e.name;
            opt.textContent = e.name + (e.size ? (' (' + e.size + ' bytes)') : '');
            sel.appendChild(opt);
          });
        } catch (e) {
          sel.innerHTML = '<option value="">SD error</option>';
        }
      }
      </script>
)rawliteral";

const char SD_TILE_HTML[] PROGMEM = R"rawliteral(
  <div class="card" id="sdCardTile">
    <h3>SD Card Management</h3>
    <div id="sdStatus" class="status">Loading...</div>

    <div style="margin-top:10px; display:flex; gap:10px; align-items:center; flex-wrap:wrap;">
      <button class="scan" type="button" onclick="sdUp()">Up</button>
      <button class="scan" type="button" onclick="sdOpenSelected()">Open</button>
      <button class="scan" type="button" onclick="sdRefresh()">Refresh</button>
      <button class="warn" type="button" onclick="sdEject()">Safe Eject</button>
    </div>

    <div style="margin-top:10px; text-align:left;">
      <div style="font-size:0.9em; color:#aaa;">Path: <span id="sdPath">/</span></div>
      <select id="sdList" size="10" style="width:100%; margin-top:6px; background:#1e1e1e; color:#eee; border:1px solid #555; border-radius:6px; padding:6px;"></select>
    </div>

    <div style="margin-top:10px; display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
      <button class="danger" type="button" onclick="sdDelete()">Delete</button>
      <button class="scan" type="button" onclick="sdRename()">Rename</button>
      <button class="scan" type="button" onclick="sdDownload()">Download</button>

      <input type="file" id="sdUploadFile" style="display:none" />
      <button class="scan" type="button" onclick="document.getElementById('sdUploadFile').click()">Upload</button>
    </div>

    <hr style="border-color:#555; margin:12px 0;">

    <h4 style="margin:0 0 6px 0;">Convert .hex on SD into Firmware Slot</h4>
    <div style="display:flex; gap:10px; flex-wrap:wrap; align-items:center;">
      <select id="sdFwSlot" style="flex:1; min-width:180px;">
        <option value="Keypad">Keypad</option>
        <option value="Front_Panel">Front Panel</option>
        <option value="Motor_Control">Motor Control</option>
        <option value="TM1637">TM1637 Display</option>
        <option value="Ultrasonic">Ultrasonic</option>
        <option value="Comms">Communications</option>
        <option value="Custom1">Custom Slot 1</option>
        <option value="Custom2">Custom Slot 2</option>
      </select>
      <input type="text" id="sdFwVer" placeholder="Version" style="flex:1; min-width:120px;" />
      <button class="scan" type="button" onclick="sdConvertToFw()">Convert to FW</button>
    </div>
    <div id="sdFwMsg" class="status" style="margin-top:6px; word-break:break-word;"></div>
  </div>

  <script>
    let sdCurPath = '/';

    async function sdFetchJson(url, opts) {
      const r = await fetch(url, opts || {});
      if (!r.ok) throw new Error(await r.text());
      return await r.json();
    }

    async function sdStatus() {
      try {
        const j = await sdFetchJson('/api/sd/status');
        const el = document.getElementById('sdStatus');
        if (el) el.textContent = (j.mounted ? 'Mounted' : 'Not mounted') + (j.msg ? (' - ' + j.msg) : '');
      } catch (e) {
        const el = document.getElementById('sdStatus');
        if (el) el.textContent = 'SD not available';
      }
    }

    async function sdRefresh() {
      await sdStatus();
      const pathEl = document.getElementById('sdPath');
      if (pathEl) pathEl.textContent = sdCurPath;

      const sel = document.getElementById('sdList');
      if (!sel) return;
      sel.innerHTML = '';

      try {
        const j = await sdFetchJson('/api/sd/list?path=' + encodeURIComponent(sdCurPath));
        const entries = j.entries || [];
        // Dirs first
        entries.sort((a,b)=> (b.isDir - a.isDir) || a.name.localeCompare(b.name));
        entries.forEach(e => {
          const opt = document.createElement('option');
          opt.value = e.name;
          opt.textContent = (e.isDir ? '[DIR] ' : '      ') + e.name + (!e.isDir ? ('  ('+e.size+' bytes)') : '');
          opt.dataset.isdir = e.isDir ? '1' : '0';
          sel.appendChild(opt);
        });
        if (!entries.length) {
          const opt = document.createElement('option');
          opt.value=''; opt.textContent='(empty)';
          sel.appendChild(opt);
        }
      } catch (e) {
        const opt = document.createElement('option');
        opt.value=''; opt.textContent='(SD error)';
        sel.appendChild(opt);
      }
    }

    function sdJoin(path, name) {
      if (!path.endsWith('/')) path += '/';
      return path + name;
    }

    function sdGetSelected() {
      const sel = document.getElementById('sdList');
      if (!sel || sel.selectedIndex < 0) return null;
      const opt = sel.options[sel.selectedIndex];
      if (!opt || !opt.value) return null;
      return { name: opt.value, isDir: opt.dataset.isdir === '1' };
    }

    async function sdOpenSelected() {
      const it = sdGetSelected();
      if (!it || !it.isDir) return;
      sdCurPath = sdJoin(sdCurPath, it.name);
      await sdRefresh();
    }

    async function sdUp() {
      if (sdCurPath === '/' ) return;
      let p = sdCurPath;
      if (p.endsWith('/')) p = p.slice(0, -1);
      const i = p.lastIndexOf('/');
      sdCurPath = (i <= 0) ? '/' : p.substring(0, i);
      await sdRefresh();
    }

    async function sdDelete() {
      const it = sdGetSelected();
      if (!it) return;
      if (!confirm('Delete ' + it.name + (it.isDir ? ' (dir)?' : '?'))) return;
      try {
        await sdFetchJson('/api/sd/delete', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ path: sdJoin(sdCurPath, it.name) }) });
        await sdRefresh();
      } catch (e) {
        alert('Delete failed: ' + e.message);
      }
    }

    async function sdRename() {
      const it = sdGetSelected();
      if (!it) return;
      const newName = prompt('Rename to:', it.name);
      if (!newName) return;
      try {
        await sdFetchJson('/api/sd/rename', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ from: sdJoin(sdCurPath, it.name), to: sdJoin(sdCurPath, newName) }) });
        await sdRefresh();
      } catch (e) {
        alert('Rename failed: ' + e.message);
      }
    }

    function sdDownload() {
      const it = sdGetSelected();
      if (!it || it.isDir) return;
      const p = sdJoin(sdCurPath, it.name);
      window.open('/sd/download?path=' + encodeURIComponent(p), '_blank');
    }

    async function sdEject() {
      if (!confirm('Unmount SD card?')) return;
      try {
        await sdFetchJson('/api/sd/eject', {method:'POST'});
      } catch (e) {
        // ignore
      }
      await sdRefresh();
    }

    async function sdConvertToFw() {
      const msg = document.getElementById('sdFwMsg');
      if (msg) msg.textContent = '';
      const it = sdGetSelected();
      if (!it || it.isDir) { if (msg) msg.textContent='Select a .hex file'; return; }
      const full = sdJoin(sdCurPath, it.name);
      if (!full.toLowerCase().endsWith('.hex')) { if (msg) msg.textContent='Selected file is not .hex'; return; }
      const slot = document.getElementById('sdFwSlot') ? document.getElementById('sdFwSlot').value : '';
      const ver  = document.getElementById('sdFwVer') ? document.getElementById('sdFwVer').value : '';
      if (!slot || !ver) { if (msg) msg.textContent='Pick slot and version'; return; }

      try {
        const r = await fetch('/api/sd/convert_fw', { method:'POST', headers:{'Content-Type':'application/json'}, body: JSON.stringify({ path: full, prefix: slot, ver: ver }) });
        const t = await r.text();
        if (msg) msg.textContent = r.ok ? t : ('Error: ' + t);
      } catch (e) {
        if (msg) msg.textContent = 'Error: ' + e.message;
      }
    }

    // Upload
    (function(){
      const f = document.getElementById('sdUploadFile');
      if (!f) return;
      f.addEventListener('change', async function(){
        if (!f.files || !f.files.length) return;
        const fd = new FormData();
        fd.append('file', f.files[0]);
        try {
          const r = await fetch('/api/sd/upload?dir=' + encodeURIComponent(sdCurPath), { method:'POST', body: fd });
          const t = await r.text();
          alert(r.ok ? t : ('Upload failed: ' + t));
        } catch (e) {
          alert('Upload error: ' + e.message);
        }
        f.value = '';
        await sdRefresh();
      });
    })();

    // Initial load
    (function(){
      sdRefresh();
    })();
  </script>
)rawliteral";
#else
const char SD_FW_OPTION_HTML[] PROGMEM = "";
const char SD_FW_AREA_HTML[] PROGMEM = "";
const char SD_TILE_HTML[] PROGMEM = "";
#endif

const char SCANNER_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>I2C Scanner</title>
  <style>
    body { font-family: monospace; background: #000; color: #0f0; padding: 20px; text-align:center; }
    .box { border: 2px solid #0f0; padding: 20px; max-width: 400px; margin: 0 auto; min-height: 150px;}
    button { background: #333; color: white; border: 1px solid white; padding: 10px; width: 100%; margin-top:20px;}
  </style>
  <script>
    setInterval(function() {
      fetch('/scan-data').then(res => res.text()).then(data => {
        document.getElementById("res").innerHTML = data;
      });
    }, 1500); 
  </script>
</head>
<body>
  <h2>I2C SCANNER</h2>
  <div class="box">
    <p>Scanning Bus...</p>
    <div id="res">Waiting...</div>
  </div>
  <button onclick="location.href='/'">RETURN TO DASHBOARD</button>
</body>
</html>
)rawliteral";

// ===================================================================================
// --- HTML: Configurator (secondary tab) ---
// ===================================================================================
const char CONFIG_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html><head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>SW8B Configurator</title>
<style>
  body { font-family: 'Segoe UI', sans-serif; background: #222; color: #eee; text-align: center; margin: 0; padding: 10px; }
  .nav { background: #333; padding: 10px; margin-bottom: 20px; border-bottom: 1px solid #444; }
  .nav a { color: #00d2ff; text-decoration: none; margin: 0 10px; font-weight: bold; }
  .card { background: #333; padding: 15px; margin: 10px auto; max-width: 600px; border-radius: 8px; border: 1px solid #444; text-align: left; }
  h2, h3 { color: #00d2ff; margin-top: 0; }
  .btn { padding: 8px 15px; border: none; border-radius: 4px; cursor: pointer; color: white; margin: 2px; }
  .btn-green { background: #28a745; } .btn-blue { background: #007acc; } .btn-red { background: #c0392b; }
  input, select { padding: 8px; background: #ddd; border: none; border-radius: 4px; width: 100%; box-sizing: border-box; margin-bottom: 5px;}
  .row { display: flex; gap: 10px; align-items: center; margin-bottom: 5px; padding: 5px; background: #2a2a2a; border-radius: 4px;}
  .col { flex: 1; }
  .pin-dot { display: inline-block; width: 12px; height: 12px; border-radius: 50%; margin-right: 5px; background: #555; }
  .pin-used { background: #c0392b; } .pin-free { background: #28a745; }
  label { font-size: 0.8em; color: #aaa; display: block;}
  .error-banner { background: #c0392b; color: white; padding: 10px; margin: 10px 0; border-radius: 4px; display:none; }
</style>
<script>
let config = { device_mode: [], pin_mode: {} };
let variant = "Unknown";
let caps = [];
const DEV_REQS = {"MOTOR_SIMPLE_HBRIDGE": 6, "SERVO_RAMPED": 3, "QUAD_ENC": 5, "ULTRASONIC": 27, "TM1637": 11, "PWM_DIMMER": 16};

function init() {
  fetch('/api/variant').then(r=>r.json()).then(d => {
    variant = d.variant; caps = d.capabilities || [];
    document.getElementById('variantLabel').innerText = variant;
    filterDropdown(); render();
  });
  loadList();
}

function filterDropdown() {
  const sel = document.getElementById('newDevType');
  for (let i = 0; i < sel.options.length; i++) {
    let opt = sel.options[i]; let req = DEV_REQS[opt.value];
    if (req && !caps.includes(req)) { opt.disabled = true; if (!opt.text.includes("(Not supported)")) opt.text += " (Not supported)"; }
    else { opt.disabled = false; opt.text = opt.text.replace(" (Not supported)", ""); }
  }
}

function getUsedPins() {
  let u = new Set();
  config.device_mode.forEach(d => Object.values(d.pins).forEach(p => u.add(parseInt(p))));
  Object.keys(config.pin_mode).forEach(p => u.add(parseInt(p)));
  return u;
}

function getNextFreePin(excludeSet) { for(let i=0; i<8; i++) if(!excludeSet.has(i)) return i; return -1; }

function render() {
  const devList = document.getElementById('devList'); devList.innerHTML = '';
  let usedPins = new Set();
  config.device_mode.forEach(d => Object.values(d.pins).forEach(p => usedPins.add(parseInt(p))));

  config.device_mode.forEach((dev, idx) => {
    let pinInputs = "";
    Object.keys(dev.pins).forEach(k => {
      pinInputs += `<label>${k}: <select style="width:50px" onchange="updDevPin(${idx}, '${k}', this.value)">`;
      for(let i=0; i<8; i++) pinInputs += `<option value="${i}" ${dev.pins[k]==i?'selected':''}>${i}</option>`;
      pinInputs += `</select></label> `;
    });
    devList.innerHTML += `<div class="row" style="border-left: 3px solid #00d2ff;"><div class="col"><b>${dev.type}</b><br><small>${dev.id}</small></div><div class="col">${pinInputs}</div><button class="btn btn-red" onclick="remDev(${idx})">X</button></div>`;
  });

  const pinTable = document.getElementById('pinTable'); pinTable.innerHTML = '';
  for(let i=0; i<8; i++) {
    if(usedPins.has(i)) continue;
    let pConf = config.pin_mode[i] || { mode: "DIGITAL_IN" };
    let opts = `<option value="DIGITAL_IN" ${pConf.mode=='DIGITAL_IN'?'selected':''}>Digital Input</option><option value="DIGITAL_OUT" ${pConf.mode=='DIGITAL_OUT'?'selected':''}>Digital Output</option><option value="INPUT_PULLUP" ${pConf.mode=='INPUT_PULLUP'?'selected':''}>Input Pullup</option>`;
    if(caps.includes(2)) opts += `<option value="ANALOG_IN" ${pConf.mode=='ANALOG_IN'?'selected':''}>Analog Input</option>`;
    if(caps.includes(3)) opts += `<option value="SERVO" ${pConf.mode=='SERVO'?'selected':''}>Servo</option>`;
    if(caps.includes(16)) opts += `<option value="PWM" ${pConf.mode=='PWM'?'selected':''}>PWM</option>`;
    pinTable.innerHTML += `<div class="row"><div class="pin-dot pin-free"></div> WP${i}<div class="col"><select onchange="updPin(${i}, 'mode', this.value)">${opts}</select></div></div>`;
  }
}

function addDevice() {
  const type = document.getElementById('newDevType').value;
  const id = document.getElementById('newDevId').value || ("Dev"+Math.floor(Math.random()*1000));
  let used = getUsedPins(); let p1 = getNextFreePin(used); used.add(p1); let p2 = getNextFreePin(used);
  let newDev = { type: type, id: id, pins: {}, settings: {} };
  if(type.includes("MOTOR_SIMPLE")) newDev.pins = { pwm: (p1>-1?p1:0), dir: (p2>-1?p2:1) };
  else if(type.includes("ULTRASONIC")) newDev.pins = { trig: (p1>-1?p1:0), echo: (p2>-1?p2:1) };
  else if(type.includes("QUAD_ENC")) newDev.pins = { A: (p1>-1?p1:0), B: (p2>-1?p2:1) };
  else if(type.includes("TM1637")) newDev.pins = { clk: (p1>-1?p1:0), dio: (p2>-1?p2:1) };
  else newDev.pins = { pin: (p1>-1?p1:0) };
  config.device_mode.push(newDev); render();
}

function updDevPin(idx, k, v) { config.device_mode[idx].pins[k] = parseInt(v); render(); }
function remDev(idx) { config.device_mode.splice(idx, 1); render(); }
function updPin(p, k, v) { if(!config.pin_mode[p]) config.pin_mode[p] = {}; config.pin_mode[p][k] = v; if(v === "DIGITAL_IN") delete config.pin_mode[p]; }

function validate() {
  let usage = {}; let errors = [];
  config.device_mode.forEach(d => Object.entries(d.pins).forEach(([role, p]) => {
    let pin = parseInt(p); if(pin < 0 || pin > 7) errors.push(`${d.id}: Pin ${pin} OOR`);
    if(usage[pin]) errors.push(`Conflict: ${pin}`); usage[pin] = d.id;
  }));
  const banner = document.getElementById('errBanner');
  if(errors.length > 0) { banner.style.display = 'block'; banner.innerHTML = errors.join("<br>"); return false; }
  banner.style.display = 'none'; return true;
}

function applyConfig() {
  if(!validate()) return;
  config.meta = { variant: variant, timestamp: Date.now() };
  config.schema_version = 1;
  fetch('/api/apply', { method: 'POST', body: JSON.stringify(config) }).then(r => r.text()).then(alert);
}

function saveConfig() {
  if(!validate()) return;
  let name = prompt("Name:"); if(!name) return;
  fetch('/api/config/exists?name='+encodeURIComponent(name)).then(r => r.json()).then(d => {
    if(d.exists && !confirm("Overwrite?")) return;
    fetch('/api/config/save?name='+encodeURIComponent(name), { method: 'POST', body: JSON.stringify(config) }).then(() => loadList());
  });
}

function loadList() {
  fetch('/api/config/list').then(r=>r.json()).then(lst => {
    const sel = document.getElementById('loadSel');
    sel.innerHTML = '<option value="">-- Select --</option>';
    lst.forEach(f => sel.innerHTML += `<option value="${f}">${f}</option>`);
  });
}

function loadConfig() {
  const name = document.getElementById('loadSel').value; if(!name) return;
  fetch('/api/config/load?name='+encodeURIComponent(name)).then(r=>r.json()).then(c => { config = c; render(); });
}
function deleteConfig() {
  const name = document.getElementById('loadSel').value; if(!name) return;
  if(confirm("Del?")) fetch('/api/config/delete?name='+encodeURIComponent(name)).then(() => loadList());
}
</script>
</head><body onload="init()">
<div class="nav"><a href="/">Dashboard</a><a href="/configure" style="color:white; border-bottom:2px solid white;">Configure</a><a href="/settings">System Settings</a></div>
<h2>System Configurator</h2>
<div class="card"><small>Variant: <span id="variantLabel">...</span></small><div id="errBanner" class="error-banner"></div></div>
<div class="card"><h3>1. Device Mode</h3><div id="devList"></div><hr>
<div class="row"><div class="col"><select id="newDevType"><option value="MOTOR_SIMPLE_HBRIDGE">Motor</option><option value="SERVO">Servo</option><option value="QUAD_ENC">Encoder</option><option value="ULTRASONIC">Ultrasonic</option><option value="TM1637">TM1637</option><option value="PWM_DIMMER">PWM</option></select></div>
<div class="col"><input type="text" id="newDevId" placeholder="ID"></div><button class="btn btn-blue" onclick="addDevice()">+</button></div></div>
<div class="card"><h3>2. Pin Mode</h3><div id="pinTable"></div></div>
<div class="card"><h3>Files</h3><div class="row"><div class="col"><select id="loadSel"></select></div><button class="btn btn-green" onclick="loadConfig()">L</button><button class="btn btn-red" onclick="deleteConfig()">D</button></div>
<button class="btn btn-blue" onclick="saveConfig()">Save As</button></div>
<br><button class="btn btn-green" onclick="applyConfig()">APPLY CONFIG</button>
</body></html>
)rawliteral";


const char SETTINGS_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>System Settings</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { ... }
  </style>
</head>
<body>
  <div style='background:#333;padding:10px;margin:0 0 10px 0;border-bottom:1px solid #444;'>
    <a href='/' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>Dashboard</a>
    <a href='/configure' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>Configurator</a>
    <a href='/settings' style='color:white;font-weight:bold;margin:0 10px;text-decoration:none;border-bottom:2px solid white;'>System Settings</a>
  </div>

  <div style='max-width:900px;margin:0 auto;padding:0 10px;'>
    <div style='background:#222;border:1px solid #444;border-radius:10px;padding:15px;margin-bottom:15px;'>
      <h3 style='margin:0 0 10px 0;'>ESP32 Diagnostics</h3>
      <div id='diagBox' style='font-family:monospace;white-space:pre-wrap;color:#ddd;'>Loading...</div>
    </div>

    <div style='background:#222;border:1px solid #444;border-radius:10px;padding:15px;margin-bottom:15px;'>
      <h3 style='margin:0 0 10px 0;'>Memory & Storage</h3>
      <div id='memBox' style='font-family:monospace;white-space:pre-wrap;color:#ddd;'>Loading...</div>
    </div>
  </div>

  <script>
    function fmtBytes(n) {
      if (n === null || n === undefined) return 'n/a';
      const u = ['B','KB','MB','GB','TB'];
      let i = 0; let v = Number(n);
      while (v >= 1024 && i < u.length-1) { v /= 1024; i++; }
      return (i >= 2 ? v.toFixed(2) : v.toFixed(0)) + ' ' + u[i];
    }

    async function refreshSystem() {
      try {
        const r = await fetch('/api/system');
        if (!r.ok) throw new Error(await r.text());
        const j = await r.json();

        const diag = [
          'CPU Frequency : ' + j.cpu_mhz + ' MHz',
          'Flash Speed   : ' + (j.flash_speed_hz ? (j.flash_speed_hz/1000000).toFixed(0) + ' MHz' : 'n/a'),
          'SDK Version   : ' + (j.sdk || 'n/a'),
          'Chip Revision : ' + (j.chip_rev ?? 'n/a'),
          'MAC Address   : ' + (j.mac || 'n/a'),
        ].join('\n');
        document.getElementById('diagBox').textContent = diag;

        const mem = [
          'Heap Total    : ' + fmtBytes(j.heap_total),
          'Heap Free     : ' + fmtBytes(j.heap_free),
          'Min Free Heap : ' + fmtBytes(j.heap_min_free),
          '',
          'LittleFS Total: ' + fmtBytes(j.fs_total),
          'LittleFS Used : ' + fmtBytes(j.fs_used),
          'LittleFS Free : ' + fmtBytes(j.fs_free),
          '',
          'SD Status     : ' + (j.sd_enabled ? (j.sd_mounted ? 'Mounted' : 'Not mounted') : 'Disabled'),
          'SD Total      : ' + (j.sd_enabled ? fmtBytes(j.sd_total) : 'n/a'),
          'SD Used       : ' + (j.sd_enabled ? fmtBytes(j.sd_used) : 'n/a'),
          'SD Free       : ' + (j.sd_enabled ? fmtBytes(j.sd_free) : 'n/a'),
        ].join('\n');
        document.getElementById('memBox').textContent = mem;
      } catch (e) {
        document.getElementById('diagBox').textContent = 'Error: ' + e;
        document.getElementById('memBox').textContent = 'Error: ' + e;
      }
    }

    refreshSystem();
    setInterval(refreshSystem, 2000);
  </script>
</body></html>
)rawliteral";

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
#if SD_USE_SDFAT
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
// VARIANT + CAPABILITY SCAN (used by /api/variant and Configurator filtering)
// ===================================================================================
struct VariantInfo {
  String variant;
  bool caps[41];
};

static VariantInfo getDeepScanInfoSingle(uint8_t addr) {
  VariantInfo info;
  info.variant = "Unknown";
  for (int i = 0; i < 41; i++) info.caps[i] = false;

  SerialWombat sw_scan;
  sw_scan.begin(Wire, addr, false);
  if (!sw_scan.queryVersion()) return info;

  // Scan supported pin modes using the known-good "wrong order" fingerprint
  for (int pm = 0; pm < 41; ++pm) {
    yield();
    uint8_t tx[8] = {201, 1, (uint8_t)pm, 0x55, 0x55, 0x55, 0x55, 0x55};
    int16_t ret = sw_scan.sendPacket(tx);
    if ((ret * -1) == SW_ERROR_PIN_CONFIG_WRONG_ORDER) info.caps[pm] = true;
  }

  // Variant mapping matches the v06 Deep Scan decisions
  if (info.caps[15]) {
    info.variant = "Keypad Firmware";
  } else if (info.caps[27]) {
    info.variant = "Ultrasonic Firmware";
  } else if (info.caps[17]) {
    info.variant = "Communications Firmware";
  } else if (info.caps[11]) {
    info.variant = "TM1637 Display Firmware";
  } else if (info.caps[25] && info.caps[36] && !info.caps[6]) {
    info.variant = "Front Panel Firmware";
  } else if (info.caps[6] && info.caps[3]) {
    info.variant = "Motor Control / Default";
  } else if (info.caps[6] && !info.caps[3]) {
    info.variant = "Brushed Motor Firmware";
  } else {
    info.variant = "Custom_FW";
  }

  return info;
}

// ===================================================================================
// CONFIGURATOR APPLY LOGIC (JSON -> Wombat)
// ===================================================================================
static void applyConfiguration(DynamicJsonDocument& doc) {
  // Safety: reset and re-begin before applying.
  sw.begin(Wire, currentWombatAddress, false);
  sw.hardwareReset();
  delay(600);
  sw.begin(Wire, currentWombatAddress, false);

  JsonArray devices = doc["device_mode"].as<JsonArray>();
  for (JsonObject dev : devices) {
    String type = dev["type"] | "";
    JsonObject pins = dev["pins"];
    JsonObject settings = dev["settings"];

    if (type == "MOTOR_SIMPLE_HBRIDGE") {
      SerialWombatHBridge b(sw);
      b.begin(pins["pwm"] | 0, pins["dir"] | 1);
    } else if (type == "SERVO") {
      SerialWombatServo s(sw);
      s.attach(pins["pin"] | 0, settings["min"] | 544, settings["max"] | 2400);
      s.write(settings["initial"] | 1500);
    } else if (type == "QUAD_ENC") {
      SerialWombatQuadEnc q(sw);
      q.begin(pins["A"] | 0, pins["B"] | 1, settings["debounce"] | 2);
    } else if (type == "ULTRASONIC") {
      SerialWombatUltrasonicDistanceSensor u(sw);
      u.begin(pins["echo"] | 1, SerialWombatUltrasonicDistanceSensor::driver::HC_SR04, pins["trig"] | 0, true, false);
    } else if (type == "TM1637") {
      SerialWombatTM1637 t(sw);
      t.begin(pins["clk"] | 0, pins["dio"] | 1, settings["digits"] | 4, (SWTM1637Mode)2, 0, settings["bright"] | 7);
      t.writeBrightness(settings["bright"] | 7);
    } else if (type == "PWM_DIMMER") {
      SerialWombatPWM p(sw);
      p.begin(pins["pin"] | 0);
      if (settings.containsKey("duty")) p.writeDutyCycle((uint16_t)settings["duty"]);
    }
  }

  JsonObject pinMap = doc["pin_mode"].as<JsonObject>();
  for (JsonPair kv : pinMap) {
    int pin = atoi(kv.key().c_str());
    JsonObject conf = kv.value().as<JsonObject>();
    String mode = conf["mode"] | "DIGITAL_IN";

    if (mode == "DIGITAL_IN") {
      sw.pinMode(pin, INPUT);
    } else if (mode == "INPUT_PULLUP") {
      sw.pinMode(pin, INPUT_PULLUP);
    } else if (mode == "DIGITAL_OUT") {
      sw.pinMode(pin, OUTPUT);
      sw.digitalWrite(pin, conf["initial"] | 0);
    } else if (mode == "SERVO") {
      SerialWombatServo s(sw);
      s.attach(pin);
      if (conf.containsKey("pos")) s.write((uint16_t)conf["pos"]);
    } else if (mode == "PWM") {
      SerialWombatPWM p(sw);
      p.begin(pin);
      if (conf.containsKey("duty")) p.writeDutyCycle((uint16_t)conf["duty"]);
    } else if (mode == "ANALOG_IN") {
      SerialWombatAnalogInput a(sw);
      a.begin(pin);
    }
  }
}

// ===================================================================================
// ROUTE HANDLERS (Dashboard + Tools)
// ===================================================================================
static void handleRoot() {
  String s = FPSTR(INDEX_HTML_HEAD);

  // Insert a simple nav bar without altering the stored v06 HTML constants.
  // (Served HTML gains a 2-tab bar; original Dashboard content remains intact.)
  const String nav =
    "<div style='background:#333;padding:10px;margin:0 -10px 10px -10px;border-bottom:1px solid #444;'>"
    "<a href='/' style='color:white;font-weight:bold;margin:0 10px;text-decoration:none;border-bottom:2px solid white;'>Dashboard</a>"
    "<a href='/configure' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>Configurator</a><a href='/settings' style='color:#00d2ff;font-weight:bold;margin:0 10px;text-decoration:none;'>System Settings</a>"
    "</div>";
  s.replace("<body>", "<body>" + nav);

  String addrHex = String(currentWombatAddress, HEX);
  addrHex.toUpperCase();
  s.replace("%ADDR%", addrHex);
  s.replace("%IP%", WiFi.localIP().toString());

  String options;
  bool found = false;
  fsListFilesBySuffix(".bin", options, found);
  if (!found) options = "<option value=''>No Firmwares Found (Use Manager)</option>";

  s += options;
  s += FPSTR(INDEX_HTML_TAIL);

  // Conditional SD UI injection (zero-clabber: placeholders already exist in HTML templates)
  s.replace("%SD_TILE%", isSDEnabled ? String(FPSTR(SD_TILE_HTML)) : String(""));
  s.replace("%SD_FW_OPTION%", isSDEnabled ? String(FPSTR(SD_FW_OPTION_HTML)) : String(""));
  s.replace("%SD_FW_AREA%", isSDEnabled ? String(FPSTR(SD_FW_AREA_HTML)) : String(""));

  server.send(200, "text/html", s);
}

static void handleScanner() {
  server.send(200, "text/html", FPSTR(SCANNER_HTML));
}

static void handleScanData() {
  String found;
  int count = 0;
  for (uint8_t i = 8; i < 127; i++) {
    Wire.beginTransmission(i);
    if ((i2cMarkTx(), Wire.endTransmission()) == 0) {
      found += "Device Found: 0x" + String(i, HEX) + "<br>";
      count++;
    }
  }
  if (count == 0) found = "No devices found.";
  else found += "<br>Total: " + String(count);
  server.send(200, "text/plain", found);
}

// --- FINGERPRINT MATCHING DEEP SCAN (v06 preserved logic) ---
static void handleDeepScan() {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(F("<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, initial-scale=1'><style>body{font-family:monospace;background:#222;color:#eee;padding:10px;}.chip{border:1px solid #0f0;padding:10px;margin-bottom:10px;background:#333;}h3{color:#00d2ff;margin:0;}b{color:#0f0;}.btn{display:block;padding:10px;background:#007acc;color:white;text-align:center;text-decoration:none;margin-top:20px;}</style></head><body><h2>Serial Wombat Deep Scan</h2>"));

  SerialWombat sw_scan;
  for (int i2cAddress = 0x0E; i2cAddress <= 0x77; ++i2cAddress) {
    yield();
    Wire.beginTransmission((uint8_t)i2cAddress);
    if ((i2cMarkTx(), Wire.endTransmission()) == 0) {
      String out = "<div class='chip'><h3>Device @ 0x" + String(i2cAddress, HEX) + "</h3>";

      sw_scan.begin(Wire, (uint8_t)i2cAddress, false);

      if (sw_scan.queryVersion()) {
        bool supported[41] = {0};
        if (sw_scan.isSW18() || sw_scan.isSW08()) {
          for (int pm = 0; pm < 41; ++pm) {
            yield();
            uint8_t tx[8] = {201, 1, (uint8_t)pm, 0x55, 0x55, 0x55, 0x55, 0x55};
            int16_t ret = sw_scan.sendPacket(tx);
            if ((ret * -1) == SW_ERROR_PIN_CONFIG_WRONG_ORDER) supported[pm] = true;
          }
        }

        String variant = "Custom_FW";
        if (supported[15]) {
          variant = "Keypad Firmware";
        } else if (supported[27]) {
          variant = "Ultrasonic Firmware";
        } else if (supported[17]) {
          variant = "Communications Firmware";
        } else if (supported[11]) {
          variant = "TM1637 Display Firmware";
        } else if (supported[25] && supported[36] && !supported[6]) {
          variant = "Front Panel Firmware";
        } else if (supported[6] && supported[3]) {
          variant = "Motor Control / Default";
        } else if (supported[6] && !supported[3]) {
          variant = "Brushed Motor Firmware";
        }

        out += "<b>Serial Wombat Found!</b><br>";
        if (sw_scan.inBoot) out += "STATUS: <b style='color:orange'>BOOT MODE</b><br>";
        else out += "STATUS: <b>APP MODE</b><br>";

        out += "Model: " + String((char*)sw_scan.model) + "<br>";
        out += "FW Version: " + String((char*)sw_scan.fwVersion) + "<br>";
        out += "<b>Variant: <span style='color:#0ff'>" + variant + "</span></b><br><br>";

        out += "Uptime: " + String(sw_scan.readFramesExecuted()) + " frames<br>";
        out += "Overflows: " + String(sw_scan.readOverflowFrames()) + "<br>";
        out += "Errors: " + String(sw_scan.errorCount) + "<br>";
        out += "Birthday: " + String(sw_scan.readBirthday()) + "<br>";

        char brand[32];
        sw_scan.readBrand(brand);
        out += "Brand: " + String(brand) + "<br>";
        out += "UUID: ";
        for (int i = 0; i < sw_scan.uniqueIdentifierLength; ++i) {
          if (sw_scan.uniqueIdentifier[i] < 16) out += "0";
          out += String(sw_scan.uniqueIdentifier[i], HEX) + " ";
        }
        out += "<br>Voltage: " + String(sw_scan.readSupplyVoltage_mV()) + " mV<br>";
        if (sw_scan.isSW18()) {
          uint16_t t = sw_scan.readTemperature_100thsDegC();
          out += "Temp: " + String(t / 100) + "." + String(t % 100) + " C<br>";
        }

        if (sw_scan.isSW18() || sw_scan.isSW08()) {
          out += "<br><b>Supported Pin Modes:</b><br><span style='font-size:0.8em;color:#aaa;'>";
          for (int pm = 0; pm < 41; ++pm) {
            if (supported[pm]) {
              out += String(FPSTR(pinModeStrings[pm])) + ", ";
            }
          }
          out += "</span>";
        }
      } else {
        out += "Unknown I2C Device";
      }

      out += "</div>";
      server.sendContent(out);
    }
  }

  server.sendContent(F("<a href='/' class='btn'>Return to Dashboard</a></body></html>"));
  server.sendContent("");
}

static void handleConnect() {
  if (server.hasArg("addr")) {
    currentWombatAddress = (uint8_t)strtol(server.arg("addr").c_str(), NULL, 16);
    sw.begin(Wire, currentWombatAddress);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleSetPin() {
  if (server.hasArg("pin") && server.hasArg("mode")) {
    uint8_t pin = (uint8_t)server.arg("pin").toInt();
    int mode = server.arg("mode").toInt();
    uint8_t tx[8] = {200, pin, (uint8_t)mode, 0, 0, 0, 0, 0};
    sw.sendPacket(tx);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleChangeAddr() {
  if (server.hasArg("newaddr")) {
    String val = server.arg("newaddr");
    uint8_t newAddr = (uint8_t)strtol(val.c_str(), NULL, 16);

    if (newAddr >= 0x08 && newAddr <= 0x77) {
      // 1) Library method (known good on SW8B)
      sw.setThroughputPin((uint32_t)newAddr);
      delay(200);

      // 2) Fallback raw packet
      Wire.beginTransmission(currentWombatAddress);
      Wire.write(0xAF);
      Wire.write(0x5F);
      Wire.write(0x42);
      Wire.write(0xAF);
      Wire.write(newAddr);
      Wire.write(0x55);
      Wire.write(0x55);
      Wire.write(0x55);
      Wire.endTransmission();
      i2cMarkTx();

      delay(200);

      // 3) Reset to latch
      sw.begin(Wire, currentWombatAddress);
      sw.hardwareReset();
      delay(1500);

      // 4) Switch to new address
      currentWombatAddress = newAddr;
      sw.begin(Wire, currentWombatAddress);
    }
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleResetTarget() {
  sw.hardwareReset();
  server.sendHeader("Location", "/");
  server.send(303);
}

static void handleResetWiFi() {
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

static void handleFormat() {
  LittleFS.format();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===================================================================================
// Firmware slot + upload handlers (ESP32 LittleFS iteration)
// ===================================================================================
static void handleCleanSlot() {
  if (!server.hasArg("prefix")) {
    server.send(400, "text/plain", "Missing prefix");
    return;
  }

  const String prefix = server.arg("prefix");
  // Collect candidates first (safer than deleting while iterating)
  String toDelete[64];
  int delCount = 0;

  auto collectInDir = [&](const char* dirPath) {
    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) return;
    SDFile f;
    while (f) {
      String n = String(f.name());
      String base = n;
      // base name for matching
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
  // Back-compat: legacy root
  collectInDir("/");

  int removed = 0;
  for (int i = 0; i < delCount; i++) {
    if (LittleFS.remove(toDelete[i])) removed++;
  }

  server.send(200, "text/plain", "Cleaned " + String(removed));
}



// ===================================================================================
// Intel HEX upload + conversion (saved temporarily to /temp, converted to /fw/<slot>_<ver>.bin)
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

static void handleUploadHex() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_hexUploadOk = false;
    g_hexUploadMsg = "";
    g_hexUploadPath = "";

    if (g_hexUploadFile) g_hexUploadFile.close();

    ensureDir("/temp");

    // Save to /temp with sanitized basename
    String safeName = sanitizeBasename(upload.filename);
    if (!safeName.endsWith(".hex")) {
      // still allow; keep name
    }
    g_hexUploadPath = joinPath("/temp", safeName);

    g_hexUploadFile = LittleFS.open(g_hexUploadPath, "w");
    if (!g_hexUploadFile) {
      g_hexUploadMsg = "Open failed: " + g_hexUploadPath;
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_hexUploadFile) return;
    size_t w = g_hexUploadFile.write(upload.buf, upload.currentSize);
    if (w != upload.currentSize) {
      g_hexUploadMsg = "Write failed";
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_hexUploadFile) g_hexUploadFile.close();
    if (g_hexUploadMsg.length() == 0) {
      g_hexUploadOk = true;
      g_hexUploadMsg = "Saved temp: " + g_hexUploadPath + " (" + String(upload.totalSize) + ")";
    } else {
      if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (g_hexUploadFile) g_hexUploadFile.close();
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    g_hexUploadOk = false;
    g_hexUploadMsg = "Upload aborted";
  }
}

static void handleUploadHexPost() {
  // Respond exactly once from the POST handler
  if (!g_hexUploadOk) {
    String msg = g_hexUploadMsg.length() ? g_hexUploadMsg : String("Upload failed");
    server.send(500, "text/plain", msg);
    return;
  }

  if (!server.hasArg("prefix") || !server.hasArg("ver")) {
    // cleanup temp file
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    server.send(400, "text/plain", "Missing prefix/ver");
    return;
  }

  String prefix = server.arg("prefix");
  String ver = server.arg("ver");
  prefix = sanitizeBasename(prefix);
  ver.trim();

  if (prefix.length() == 0 || ver.length() == 0) {
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    server.send(400, "text/plain", "Bad prefix/ver");
    return;
  }

  // Final output name matches Blob workflow: <slot>_<ver>.bin in /fw
  String finalName = prefix + "_" + ver + ".bin";
  String outPath = joinPath(FW_DIR, finalName);

  // Convert
  String warnOrErr;
  bool ok = convertHexToFirmwareBin(g_hexUploadPath, outPath, warnOrErr);

  // Always delete temp hex immediately
  if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);

  if (!ok) {
    // remove any partial output
    LittleFS.remove(outPath);
    String msg = warnOrErr.length() ? warnOrErr : String("Conversion failed");
    server.send(500, "text/plain", msg);
    return;
  }

  String msg = "Converted & saved: " + outPath;
  if (warnOrErr.length()) {
    // Keep it short but useful
    msg += "\nWarnings:\n";
    msg += warnOrErr;
  }
  server.send(200, "text/plain", msg);
}
static void handleUploadFW() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_fwUploadOk = false;
    g_fwUploadMsg = "";
    g_fwUploadPath = "";

    if (g_fwUploadFile) g_fwUploadFile.close();

    // Force firmware blobs into /fw
    String safeName = sanitizeBasename(upload.filename);
    g_fwUploadPath = joinPath(FW_DIR, safeName);

    // Ensure directory exists
    if (!LittleFS.exists(FW_DIR)) {
      LittleFS.mkdir(FW_DIR);
    }

    g_fwUploadFile = LittleFS.open(g_fwUploadPath, "w");
    if (!g_fwUploadFile) {
      g_fwUploadMsg = "Open failed: " + g_fwUploadPath;
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_fwUploadFile) return;
    size_t w = g_fwUploadFile.write(upload.buf, upload.currentSize);
    if (w != upload.currentSize) {
      g_fwUploadMsg = "Write failed";
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_fwUploadFile) g_fwUploadFile.close();
    if (g_fwUploadMsg.length() == 0) {
      g_fwUploadOk = true;
      g_fwUploadMsg = "Saved: " + g_fwUploadPath + " (" + String(upload.totalSize) + ")";
    } else {
      // Remove partial
      if (g_fwUploadPath.length()) LittleFS.remove(g_fwUploadPath);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (g_fwUploadFile) g_fwUploadFile.close();
    if (g_fwUploadPath.length()) LittleFS.remove(g_fwUploadPath);
    g_fwUploadOk = false;
    g_fwUploadMsg = "Upload aborted";
  }
}

// ===================================================================================
// Firmware Flasher (v06 - row-by-row writing) PRESERVED
// ===================================================================================
static void handleFlashFW() {
  if (!server.hasArg("fw_name")) {
    server.send(400, "text/plain", "No selection");
    return;
  }

  // Be forgiving about what the UI posts. Depending on browser + HTML source,
  // this can be:
  //   - "Default_FW_2.2.2.bin" (basename)
  //   - "/fw/Default_FW_2.2.2.bin" (preferred)
  //   - "fw/Default_FW_2.2.2.bin" (no leading slash)
  //   - legacy root paths
  String raw = server.arg("fw_name");
  raw.trim();

  String fwName = normalizePath(raw);

  // If caller passed a basename, try /fw first, then legacy root.
  // If caller passed a path, still try a couple of normalizations.
  String resolved;
  auto tryPath = [&](const String& p) {
    if (resolved.length()) return;
    if (LittleFS.exists(p)) resolved = p;
  };

  // Direct as provided
  tryPath(fwName);

  // If it doesn't look like a directory path, try firmware dir
  if (!resolved.length()) {
    String base = raw;
    // strip any leading directory
    int lastSlash = base.lastIndexOf('/');
    if (lastSlash >= 0) base = base.substring(lastSlash + 1);
    base.trim();
    if (base.length()) {
      tryPath(joinPath(FW_DIR, base));
      tryPath("/" + base); // legacy root
    }
  }

  // Final fallback: if it started with "//" from any weirdness, collapse it.
  if (!resolved.length() && fwName.startsWith("//")) {
    String collapsed = fwName;
    while (collapsed.startsWith("//")) collapsed.remove(0, 1);
    tryPath(collapsed);
  }

  if (!resolved.length()) {
    // Provide a helpful diagnostic list (kept short) so you can see what's actually on FS.
    String msg = "File missing. Requested='" + raw + "'\n";
    msg += "Checked: " + fwName + "\n";
    msg += "FW dir: " + String(FW_DIR) + "\n\n";
    msg += "Available firmwares:\n";
    File dir = LittleFS.open(FW_DIR);
    if (dir && dir.isDirectory()) {
      SDFile f;
      int shown = 0;
      while (f && shown < 30) {
        String n = String(f.name());
        if (n.endsWith(".bin")) {
          msg += " - " + n + " (" + String(f.size()) + ")\n";
          shown++;
        }
        f.close();
        f.close();
    // next

      }
    } else {
      msg += " (cannot open /fw)\n";
    }
    server.send(400, "text/plain", msg);
    return;
  }

  fwName = resolved;

  File fwFile = LittleFS.open(fwName, "r");
  if (!fwFile) {
    server.send(500, "text/plain", "File Error");
    return;
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(F("<!DOCTYPE HTML><html><head><meta http-equiv='refresh' content='10;url=/'><style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;white-space:pre-wrap;}</style></head><body><h2>SW8B Firmware Update</h2>"));
  server.sendContent("Flashing: " + fwName + " (" + String(fwFile.size()) + " bytes)\n");

  sw.begin(Wire, currentWombatAddress, false);
  if (!sw.queryVersion()) server.sendContent("Connecting...\n");

  if (!sw.inBoot) {
    sw.jumpToBoot();
    sw.hardwareReset();
    delay(2000);
  }

  sw.begin(Wire, currentWombatAddress, false);
  if (!sw.queryVersion()) {
    server.sendContent("Error: Bootloader not found.\n");
    fwFile.close();
    return;
  }

  sw.eraseFlashPage(0);
  server.sendContent("Erasing...\n");

  uint32_t address = 0;
  uint8_t buffer[64];

  while (fwFile.available()) {
    yield();
    ArduinoOTA.handle();

    int bytesRead = fwFile.read(buffer, 64);
    if (bytesRead < 64) {
      for (int k = bytesRead; k < 64; k++) buffer[k] = 0xFF;
    }

    uint32_t page[16];
    bool dirty = false;
    for (int i = 0; i < 16; i++) {
      uint32_t val =
        (uint32_t)buffer[i * 4] +
        ((uint32_t)buffer[i * 4 + 1] << 8) +
        ((uint32_t)buffer[i * 4 + 2] << 16) +
        ((uint32_t)buffer[i * 4 + 3] << 24);
      page[i] = val;
      if (val != 0xFFFFFFFF) dirty = true;
    }

    if (dirty) {
      sw.writeUserBuffer(0, (uint8_t*)page, 64);
      sw.writeFlashRow(address * 4 + 0x08000000);
      if (address % 128 == 0) server.sendContent("Writing addr: 0x" + String(address, HEX) + "\n");
      delay(10);
    }

    address += 16;
  }

  fwFile.close();

  uint8_t tx[] = {164, 4, 0, 0, 0, 0, 0, 0};
  sw.sendPacket(tx);
  delay(100);
  sw.hardwareReset();

  server.sendContent("\n<h3>SUCCESS! Redirecting...</h3></body></html>");
  server.sendContent("");

  delay(1000);
  sw.begin(Wire, currentWombatAddress);
}

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
// TCP Bridge (I2C passthrough)
// ===================================================================================
static void handleTcpBridge() {
  if (tcpServer.hasClient()) {
    if (!tcpClient || !tcpClient.connected()) {
      tcpClient = tcpServer.available();
      tcpClient.setNoDelay(true);
    } else {
      WiFiClient reject = tcpServer.available();
      reject.stop();
    }
  }

  if (tcpClient && tcpClient.connected()) {
    while (tcpClient.available() >= 8) {
      uint8_t txBuffer[8];
      uint8_t rxBuffer[8];

      tcpClient.read(txBuffer, 8);

      Wire.beginTransmission(currentWombatAddress);
      Wire.write(txBuffer, 8);
      Wire.endTransmission();
      i2cMarkTx();

      uint8_t bytesRead = Wire.requestFrom(currentWombatAddress, (uint8_t)8);
      i2cMarkRx();
      for (int i = 0; i < 8; i++) {
        if (i < bytesRead) rxBuffer[i] = Wire.read();
        else rxBuffer[i] = 0xFF;
      }

      tcpClient.write(rxBuffer, 8);
      ArduinoOTA.handle();
      yield();
    }
  }
}

// ===================================================================================
// CONFIG API
// ===================================================================================
static void handleApiVariant() {
  VariantInfo info = getDeepScanInfoSingle(currentWombatAddress);
  DynamicJsonDocument doc(1536);
  doc["variant"] = info.variant;
  JsonArray capsArr = doc.createNestedArray("capabilities");
  for (int i = 0; i < 41; i++) if (info.caps[i]) capsArr.add(i);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleApiApply() {
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", String("Bad JSON: ") + err.c_str());
    return;
  }
  applyConfiguration(doc);
  server.send(200, "text/plain", "OK");
}

static String configPathFromName(const String& name) {
  // New location: /config/<name>.json
  // Back-compat: legacy root files /config_<name>.json are still supported on load/list.
  String safe = sanitizeBasename(name);
  if (safe.length() == 0) safe = "config";
  return joinPath(CFG_DIR, safe + String(".json"));
}

static void handleConfigSave() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  String name = server.arg("name");
  String path = configPathFromName(name);
  File f = LittleFS.open(path, "w");
  if (!f) { server.send(500, "text/plain", "Open failed"); return; }
  f.print(server.arg("plain"));
  f.close();
  server.send(200, "text/plain", "Saved");
}

static void handleConfigLoad() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  String name = server.arg("name");
  String path = configPathFromName(name);
  File f = LittleFS.open(path, "r");
  if (!f) {
    // Back-compat: legacy root path
    String legacy = String("/config_") + sanitizeBasename(name) + String(".json");
    f = LittleFS.open(legacy, "r");
  }
  if (!f) { server.send(404, "text/plain", "Not found"); return; }
  server.streamFile(f, "application/json");
  f.close();
}

static void handleConfigList() {
  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();

  // New location: /config directory
  File cfg = LittleFS.open(CFG_DIR);
  if (cfg && cfg.isDirectory()) {
    File file = cfg.openNextFile();
    while (file) {
      String n = String(file.name());
      // Expect /config/<name>.json
      if (n.endsWith(".json")) {
        String base = n;
        int slash = base.lastIndexOf('/');
        if (slash >= 0) base = base.substring(slash + 1);
        if (base.endsWith(".json")) base = base.substring(0, base.length() - 5);
        if (base.length()) arr.add(base);
      }
      file.close();
      file = cfg.openNextFile();
    }
    cfg.close();
  }

  // Back-compat: legacy root files /config_<name>.json
  File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      String n = String(file.name());
      if (n.startsWith("/config_") && n.endsWith(".json")) {
        n.replace("/config_", "");
        n.replace(".json", "");
        arr.add(n);
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  String out;
  serializeJson(arr, out);
  server.send(200, "application/json", out);
}

static void handleConfigExists() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  DynamicJsonDocument doc(128);
  doc["exists"] = LittleFS.exists(configPathFromName(server.arg("name")));
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleConfigDelete() {
  if (!server.hasArg("name")) { server.send(400, "text/plain", "Missing name"); return; }
  LittleFS.remove(configPathFromName(server.arg("name")));
  server.send(200, "text/plain", "Deleted");
}





// ===================================================================================
// SD / Firmware Import helpers
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
// SYSTEM SETTINGS API
// ===================================================================================
static void handleApiSystem() {
  DynamicJsonDocument doc(768);
  doc["cpu_mhz"] = ESP.getCpuFreqMHz();
  doc["flash_speed_hz"] = (uint32_t)ESP.getFlashChipSpeed();
  doc["sdk"] = String(ESP.getSdkVersion());
  doc["chip_rev"] = ESP.getChipRevision();
  doc["mac"] = WiFi.macAddress();
  doc["heap_total"] = ESP.getHeapSize();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min_free"] = ESP.getMinFreeHeap();

  size_t fsTotal = LittleFS.totalBytes();
  size_t fsUsed = LittleFS.usedBytes();
  doc["fs_total"] = (uint64_t)fsTotal;
  doc["fs_used"] = (uint64_t)fsUsed;
  doc["fs_free"] = (uint64_t)(fsTotal >= fsUsed ? (fsTotal - fsUsed) : 0);

  doc["sd_enabled"] = (bool)isSDEnabled;
#if SD_SUPPORT_ENABLED
  // Lazy-mount to reflect insertion state without forcing a long init on every refresh.
  // If not mounted, attempt a quick mount. If it fails, report not mounted.
  if (!g_sdMounted && isSDEnabled) {
    sdEnsureMounted();
  }
  doc["sd_mounted"] = (bool)g_sdMounted;
  if (g_sdMounted) {
    uint64_t total = 0, used = 0;
    // Arduino-ESP32 SD.h provides totalBytes/usedBytes on FS.
    #if defined(ARDUINO_ARCH_ESP32)
      total = sd_total_bytes();
      used  = sd_used_bytes();
    #endif
    doc["sd_total"] = total;
    doc["sd_used"]  = used;
    doc["sd_free"]  = (total >= used) ? (total - used) : 0;
  } else {
    doc["sd_total"] = 0;
    doc["sd_used"]  = 0;
    doc["sd_free"]  = 0;
  }
#else
  doc["sd_mounted"] = false;
  doc["sd_total"] = 0;
  doc["sd_used"]  = 0;
  doc["sd_free"]  = 0;
#endif

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

#if SD_SUPPORT_ENABLED
// ===================================================================================
// SD CARD API
// ===================================================================================

 
static void handleApiSdStatus() {
  DynamicJsonDocument doc(256);
  doc["enabled"] = (bool)isSDEnabled;
  if (!isSDEnabled) {
    doc["mounted"] = false;
    doc["msg"] = "Disabled";
  } else {
    bool ok = sdEnsureMounted();
    doc["mounted"] = ok;
    doc["msg"] = g_sdMountMsg;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

static void handleApiSdList() {
#if !SD_SUPPORT_ENABLED
  server.send(400, "text/plain", "SD disabled");
  return;
#else
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }
  String dir = server.arg("dir");
  if (!dir.length()) dir = "/";
  if (!dir.startsWith("/")) dir = "/" + dir;

  SDFile d = sd_open(dir, O_RDONLY);
  if (!d) { server.send(404, "text/plain", "Dir not found"); return; }

  // Iterate
  String json = "[";
  bool first = true;
  SDFile e;
  while (sdOpenNext(d, e)) {
    String name = sdFileName(e);
    bool isdir = sdFileIsDir(e);
    uint64_t size = sd_filesize(e);
    e.close();

    if (name == "." || name == "..") continue;
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"" + jsonEscape(name) + "\",\"dir\":" + String(isdir ? "true" : "false") + ",\"size\":" + String((unsigned long long)size) + "}";
  }
  d.close();
  json += "]";
  server.send(200, "application/json", json);
#endif
}

static bool sdDeleteRecursive(const String& path) { return sdRemoveRecursive(path); }

static void handleApiSdDelete() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }
  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { server.send(400, "text/plain", "Bad JSON"); return; }
  String path = sanitizePath(doc["path"] | "");
  if (path == "/" || path.length() < 2) { server.send(400, "text/plain", "Refuse" ); return; }
  bool ok = sdDeleteRecursive(path);
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Delete failed");
}

static void handleApiSdRename() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }
  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { server.send(400, "text/plain", "Bad JSON"); return; }
  String from = sanitizePath(doc["from"] | "");
  String to = sanitizePath(doc["to"] | "");
  if (!from.length() || !to.length() || from == "/" || to == "/") { server.send(400, "text/plain", "Bad path"); return; }
  bool ok = sd_rename(from, to);
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Rename failed");
}

static void handleSdDownload() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }
  String path = server.hasArg("path") ? server.arg("path") : String("");
  path = sanitizePath(path);

  SDFile f = sd_open(path.c_str(), O_RDONLY);
  if (!f || f.isDirectory()) { server.send(404, "text/plain", "Not found"); return; }

  String fn = path;
  int slash = fn.lastIndexOf('/');
  if (slash >= 0) fn = fn.substring(slash + 1);

  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + fn + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength((int)f.size());
  server.send(200, "application/octet-stream", "");

  WiFiClient client = server.client();
  uint8_t buf[1024];
  while (client.connected()) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    client.write(buf, n);
    delay(0);
  }
  f.close();
}



static void handleApiSdEject() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  sdUnmount();
  server.send(200, "text/plain", "Ejected");
}

static void handleApiSdUploadPost() {
  // Response is sent when upload completes (in handleUploadSD).
  server.send(200, "text/plain", g_sdUploadOk ? String("OK: ") + g_sdUploadMsg : String("ERR: ") + g_sdUploadMsg);
}

static void handleUploadSD() {
  if (!isSDEnabled) { g_sdUploadOk = false; g_sdUploadMsg = "SD disabled"; return; }
  if (!sdEnsureMounted()) { g_sdUploadOk = false; g_sdUploadMsg = g_sdMountMsg; return; }

  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    g_sdUploadOk = false;
    g_sdUploadMsg = "";
    String dir = server.hasArg("dir") ? server.arg("dir") : String("/");
    dir = sanitizePath(dir);
    if (!dir.endsWith("/")) dir += "/";
    String fn = sanitizeBasename(upload.filename);
    if (!fn.length()) fn = "upload.bin";
    g_sdUploadPath = dir + fn;
    g_sdUploadFile = sd_open(g_sdUploadPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!g_sdUploadFile) {
      g_sdUploadMsg = "Open failed";
      return;
    }
    g_sdUploadOk = true;
    g_sdUploadMsg = g_sdUploadPath;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_sdUploadOk && g_sdUploadFile) {
      size_t w = g_sdUploadFile.write(upload.buf, upload.currentSize);
      if (w != upload.currentSize) {
        g_sdUploadOk = false;
        g_sdUploadMsg = "Write failed";
      }
    }
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_sdUploadFile) g_sdUploadFile.close();
    // Leave mounted; safe-eject is explicit.
    yield();
  }
}

static void handleUploadSdPost() {
  if (g_sdUploadOk) {
    server.send(200, "text/plain", "Uploaded: " + g_sdUploadMsg);
  } else {
    String msg = g_sdUploadMsg.length() ? g_sdUploadMsg : String("Upload failed");
    server.send(500, "text/plain", msg);
  }
}

// Import a file from SD into internal firmware storage (supports .bin, .hex, .txt)
static void handleApiSdImportFw() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { server.send(400, "text/plain", "Bad JSON"); return; }

  String sdPath = sanitizePath(doc["path"] | "");
  String slot = sanitizeBasename(doc["slot"] | "");
  String ver  = sanitizeBasename(doc["ver"] | "");
  if (!sdPath.length() || !slot.length() || !ver.length()) { server.send(400, "text/plain", "Missing fields"); return; }

  SDFile src = sd_open(sdPath, O_RDONLY);
  if (!src || src.isDirectory()) { server.send(404, "text/plain", "SD file not found"); return; }
  src.close();

  String lower = sdPath; lower.toLowerCase();
  String outPath = fwSlotPath(slot, ver);

  // Clean existing slot before import
  fsCleanSlot(slot);

  bool ok = false;
  String msg;

  if (lower.endsWith(".hex")) {
    // Convert HEX -> SW8B text -> .bin
    String tmpIn = ensureTempPathForUpload("sd.hex");
    String tmpOut = ensureTempPathForUpload("sd_fw.txt");

    // Copy SD hex to temp file to reuse converter's FS-agnostic begin
    SDFile inSD = sd_open(sdPath, O_RDONLY);
    File inLF = LittleFS.open(tmpIn, "w");
    if (!inSD || !inLF) { if(inSD) inSD.close(); if(inLF) inLF.close(); server.send(500, "text/plain", "Temp open failed"); return; }
    uint8_t buf[2048];
    while (true) {
      size_t r = inSD.read(buf, sizeof(buf));
      if (!r) break;
      if (inLF.write(buf, r) != r) { break; }
      yield();
    }
    inSD.close(); inLF.close();

    IntelHexSW8B conv;
    if (!conv.begin(LittleFS, TEMP_DIR)) {
      msg = "Converter init failed";
    } else if (!conv.loadHexFile(tmpIn.c_str())) {
      msg = "HEX parse failed";
    } else if (!conv.exportFW_CH32V003_16K_Strict(tmpOut.c_str(), true, false)) {
      msg = "Text export failed";
    } else {
      // Now convert fw.txt -> bin with existing pipeline
      ok = fwTxtToBin(tmpOut, outPath, msg);
    }

    LittleFS.remove(tmpIn);
    LittleFS.remove(tmpOut);
  } else if (lower.endsWith(".txt")) {
    ok = fwTxtToBin(sdPath, outPath, msg, /*fromSD=*/true);
  } else {
    // .bin or unknown: copy raw
    ok = sdCopyToLittleFS(sdPath.c_str(), outPath.c_str());
  }

  server.send(ok ? 200 : 500, "text/plain", ok ? String("Imported: ") + outPath : String("Failed: ") + msg);
}

// Convert selected SD .hex to a firmware slot directly
static void handleApiSdConvertFw() {
  if (!isSDEnabled) { server.send(403, "text/plain", "SD disabled"); return; }
  if (!sdEnsureMounted()) { server.send(500, "text/plain", g_sdMountMsg); return; }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) { server.send(400, "text/plain", "Bad JSON"); return; }
  doc["slot"] = doc["slot"] | "";
  doc["ver"] = doc["ver"] | "";
  doc["path"] = doc["path"] | "";
  // Reuse import handler
  handleApiSdImportFw();
}
#endif
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

  // Dashboard + Tools
  server.on("/", handleRoot);
  server.on("/scanner", handleScanner);
  server.on("/scan-data", handleScanData);
  server.on("/deepscan", handleDeepScan);
  server.on("/connect", handleConnect);
  server.on("/setpin", handleSetPin);
  server.on("/changeaddr", handleChangeAddr);
  server.on("/resetwifi", handleResetWiFi);
  server.on("/flashfw", HTTP_POST, handleFlashFW);
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
    handleUploadFW
  );

  server.on(
    "/upload_hex",
    HTTP_POST,
    handleUploadHexPost,
    handleUploadHex
  );

  server.on("/clean_slot", handleCleanSlot);
  server.on("/resetwombat", handleResetTarget);
  server.on("/formatfs", handleFormat);

  // Configurator UI
  server.on("/configure", []() { server.send(200, "text/html", FPSTR(CONFIG_HTML)); });

  // System Settings UI
  server.on("/settings", []() { server.send(200, "text/html", FPSTR(SETTINGS_HTML)); });

  // System Settings API
  server.on("/api/system", HTTP_GET, handleApiSystem);

#if SD_SUPPORT_ENABLED
  // SD Card Manager API (only registered when enabled)
  if (isSDEnabled) {
    server.on("/api/sd/status", HTTP_GET, handleApiSdStatus);
    server.on("/api/sd/list", HTTP_GET, handleApiSdList);
    server.on("/api/sd/delete", HTTP_POST, handleApiSdDelete);
    server.on("/api/sd/rename", HTTP_POST, handleApiSdRename);
    server.on("/api/sd/eject", HTTP_POST, handleApiSdEject);
    server.on("/api/sd/import_fw", HTTP_POST, handleApiSdImportFw);
    server.on("/api/sd/convert_fw", HTTP_POST, handleApiSdImportFw);
    server.on("/sd/download", HTTP_GET, handleSdDownload);
    server.on("/api/sd/upload", HTTP_POST, handleUploadSdPost, handleUploadSD);
  }
#endif

  // Configurator API
  server.on("/api/variant", HTTP_GET, handleApiVariant);
  server.on("/api/apply", HTTP_POST, handleApiApply);
  server.on("/api/config/save", HTTP_POST, handleConfigSave);
  server.on("/api/config/load", HTTP_GET, handleConfigLoad);
  server.on("/api/config/list", HTTP_GET, handleConfigList);
  server.on("/api/config/exists", HTTP_GET, handleConfigExists);
  server.on("/api/config/delete", HTTP_GET, handleConfigDelete);

  server.begin();
  tcpServer.begin();
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