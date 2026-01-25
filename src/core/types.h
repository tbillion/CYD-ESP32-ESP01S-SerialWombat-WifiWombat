#pragma once

#include <Arduino.h>

// Forward declarations for oflag_t from SdFat
typedef uint8_t oflag_t;

// Common enums used across the firmware
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

enum TouchKind : uint8_t { TOUCH_NONE = 0, TOUCH_XPT2046, TOUCH_GT911, TOUCH_I2C_GENERIC };

// Convert model enum to string
const char* modelToStr(CydModel m);

// Convert panel enum to string
const char* panelToStr(PanelKind p);

// Convert touch enum to string
const char* touchToStr(TouchKind t);
