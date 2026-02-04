#include "lgfx_display.h"

#if DISPLAY_SUPPORT_ENABLED

LGFX lcd;

bool LGFX::beginFromConfig(const SystemConfig& cfg) {
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
      bcfg.pin_d0 = (gpio_num_t)cfg.rgb_pins[0];
      bcfg.pin_d1 = (gpio_num_t)cfg.rgb_pins[1];
      bcfg.pin_d2 = (gpio_num_t)cfg.rgb_pins[2];
      bcfg.pin_d3 = (gpio_num_t)cfg.rgb_pins[3];
      bcfg.pin_d4 = (gpio_num_t)cfg.rgb_pins[4];
      bcfg.pin_d5 = (gpio_num_t)cfg.rgb_pins[5];
      bcfg.pin_d6 = (gpio_num_t)cfg.rgb_pins[6];
      bcfg.pin_d7 = (gpio_num_t)cfg.rgb_pins[7];
      bcfg.pin_d8 = (gpio_num_t)cfg.rgb_pins[8];
      bcfg.pin_d9 = (gpio_num_t)cfg.rgb_pins[9];
      bcfg.pin_d10 = (gpio_num_t)cfg.rgb_pins[10];
      bcfg.pin_d11 = (gpio_num_t)cfg.rgb_pins[11];
      bcfg.pin_d12 = (gpio_num_t)cfg.rgb_pins[12];
      bcfg.pin_d13 = (gpio_num_t)cfg.rgb_pins[13];
      bcfg.pin_d14 = (gpio_num_t)cfg.rgb_pins[14];
      bcfg.pin_d15 = (gpio_num_t)cfg.rgb_pins[15];
      bcfg.pin_henable = (gpio_num_t)cfg.rgb_hen;
      bcfg.pin_vsync = (gpio_num_t)cfg.rgb_vsync;
      bcfg.pin_hsync = (gpio_num_t)cfg.rgb_hsync;
      bcfg.pin_pclk = (gpio_num_t)cfg.rgb_pclk;
      bcfg.freq_write = cfg.rgb_freq_write;
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
      lcfg.pin_bl = -1;  // many RGB boards use external control; keep disabled
      lcfg.invert = false;
      lcfg.freq = 44100;
      lcfg.pwm_channel = 7;
      _light_pwm.config(lcfg);
    }
    _panel_rgb.setLight(&_light_pwm);

    // Touch
    if (cfg.touch_enable && cfg.touch == TOUCH_GT911) {
      auto tcfg = _touch_gt.config();
      tcfg.x_min = 0;
      tcfg.y_min = 0;
      tcfg.x_max = 799;
      tcfg.y_max = 479;
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
  if (cfg.panel == PANEL_SPI_ILI9341)
    panel = &_panel_ili;
  else if (cfg.panel == PANEL_SPI_ST7789)
    panel = &_panel_7789;
  else if (cfg.panel == PANEL_SPI_ST7796)
    panel = &_panel_7796;
  else
    return false;

  {
    auto bcfg = _bus_spi.config();
    bcfg.spi_host = VSPI_HOST;
    bcfg.spi_mode = 0;
    bcfg.freq_write = cfg.tft_freq;
    bcfg.freq_read = 16000000;
    bcfg.pin_sclk = cfg.tft_sck;
    bcfg.pin_mosi = cfg.tft_mosi;
    bcfg.pin_miso = cfg.tft_miso;
    bcfg.pin_dc = cfg.tft_dc;
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
    tcfg.spi_host = HSPI_HOST;  // CYD touch is often on HSPI
    tcfg.freq = 2000000;
    tcfg.pin_sclk = cfg.tp_sck;
    tcfg.pin_mosi = cfg.tp_mosi;
    tcfg.pin_miso = cfg.tp_miso;
    tcfg.pin_cs = cfg.tp_cs;
    tcfg.pin_int = cfg.tp_irq;
    tcfg.bus_shared = false;
    tcfg.x_min = 200;
    tcfg.x_max = 3900;
    tcfg.y_min = 200;
    tcfg.y_max = 3900;
    tcfg.offset_rotation = 0;
    _touch_xpt.config(tcfg);
    panel->setTouch(&_touch_xpt);
  }

  setPanel(panel);
  return init();
}

#endif  // DISPLAY_SUPPORT_ENABLED
