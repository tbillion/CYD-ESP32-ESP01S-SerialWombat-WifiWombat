#include "types.h"

const char* modelToStr(CydModel m) {
  switch (m) {
    case CYD_2432S028R:
      return "2432S028R";
    case CYD_2432S028C:
      return "2432S028C";
    case CYD_2432S022C:
      return "2432S022C";
    case CYD_2432S032:
      return "2432S032";
    case CYD_3248S035:
      return "3248S035";
    case CYD_4827S043:
      return "4827S043";
    case CYD_8048S050:
      return "8048S050";
    case CYD_8048S070:
      return "8048S070";
    case CYD_S3_GENERIC:
      return "S3_GENERIC";
    default:
      return "UNKNOWN";
  }
}

const char* panelToStr(PanelKind p) {
  switch (p) {
    case PANEL_NONE:
      return "NONE";
    case PANEL_SPI_ILI9341:
      return "ILI9341";
    case PANEL_SPI_ST7789:
      return "ST7789";
    case PANEL_SPI_ST7796:
      return "ST7796";
    case PANEL_RGB_800x480:
      return "RGB_800x480";
    default:
      return "UNKNOWN";
  }
}

const char* touchToStr(TouchKind t) {
  switch (t) {
    case TOUCH_NONE:
      return "NONE";
    case TOUCH_XPT2046:
      return "XPT2046";
    case TOUCH_GT911:
      return "GT911";
    case TOUCH_I2C_GENERIC:
      return "I2C_GENERIC";
    default:
      return "UNKNOWN";
  }
}
