#pragma once

#include "../../config/system_config.h"

#if DISPLAY_SUPPORT_ENABLED

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

// Panel drivers
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

// LovyanGFX wrapper class
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

  bool beginFromConfig(const SystemConfig &cfg);
};

// Global LCD instance
extern LGFX lcd;

#endif // DISPLAY_SUPPORT_ENABLED
