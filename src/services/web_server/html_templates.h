#pragma once

#include <Arduino.h>

// HTML Template declarations
extern const char INDEX_HTML_HEAD[] PROGMEM;
extern const char INDEX_HTML_TAIL[] PROGMEM;

#if SD_SUPPORT_ENABLED
extern const char SD_FW_OPTION_HTML[] PROGMEM;
extern const char SD_FW_AREA_HTML[] PROGMEM;
extern const char SD_TILE_HTML[] PROGMEM;
#else
extern const char SD_FW_OPTION_HTML[] PROGMEM;
extern const char SD_FW_AREA_HTML[] PROGMEM;
extern const char SD_TILE_HTML[] PROGMEM;
#endif

extern const char SCANNER_HTML[] PROGMEM;
extern const char CONFIG_HTML[] PROGMEM;
extern const char SETTINGS_HTML[] PROGMEM;
