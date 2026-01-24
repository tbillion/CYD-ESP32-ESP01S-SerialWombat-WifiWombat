#include "config_manager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

#if SD_SUPPORT_ENABLED
// SD runtime pins (forward declarations from sd_storage)
extern int g_sd_sck;
extern int g_sd_mosi;
extern int g_sd_miso;
extern int g_sd_cs;
#endif

// Global configuration instance
SystemConfig g_cfg;

bool cfgExists() {
  return LittleFS.exists(CFG_PATH);
}

CydModel strToModel(const String& s) {
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

void syncSdRuntimePins(const SystemConfig &cfg) {
#if SD_SUPPORT_ENABLED
  g_sd_sck = cfg.sd_sck;
  g_sd_mosi = cfg.sd_mosi;
  g_sd_miso = cfg.sd_miso;
  g_sd_cs = cfg.sd_cs;
#else
  (void)cfg;
#endif
}

void applyModelPreset(SystemConfig &cfg) {
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

void setConfigDefaults(SystemConfig &cfg) {
  cfg = SystemConfig();
  cfg.configured = false;
  cfg.headless = false;
  // Start unconfigured: no panel/touch until user selects a model (or headless triggers).
  cfg.model = CYD_UNKNOWN;
  applyModelPreset(cfg);
}

bool loadConfig(SystemConfig &cfg) {
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

bool saveConfig(const SystemConfig &cfg) {
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
