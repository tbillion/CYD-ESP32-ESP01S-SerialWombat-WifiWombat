#pragma once

#include <Arduino.h>
#include "../core/types.h"
#include "defaults.h"

// Configuration file path
static const char* CFG_PATH = "/config.json";

// SystemConfig structure - Runtime configuration
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

// Global configuration instance
extern SystemConfig g_cfg;

// Helper to check if config exists
bool cfgExists();

// String to model enum conversion
CydModel strToModel(const String& s);
