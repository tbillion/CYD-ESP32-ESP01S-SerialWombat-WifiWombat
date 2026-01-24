# Refactoring Map

## Overview
This document maps the transformation from the monolithic `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino` (4767 lines) to a modular, maintainable codebase.

## Philosophy
- **Preserve Behavior**: No functional regressions
- **Surgical Changes**: Extract, don't rewrite
- **Clear Ownership**: One module, one responsibility
- **Test

ability**: Each module independently testable
- **Backward Compatibility**: Existing configs/APIs remain functional

## Directory Structure (New)

```
/home/runner/work/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/
├── src/
│   ├── main.cpp                          # NEW: Clean entry point (setup/loop only)
│   ├── app/
│   │   ├── app.h                         # NEW: Application orchestrator
│   │   └── app.cpp
│   ├── core/
│   │   ├── logging.h                     # NEW: Logging utilities
│   │   ├── logging.cpp
│   │   ├── error_codes.h                 # NEW: Error code definitions
│   │   ├── types.h                       # NEW: Common types
│   │   └── event_bus.h                   # NEW: Event system for UI↔Services
│   │   └── event_bus.cpp
│   ├── hal/
│   │   ├── hal.h                         # NEW: HAL interface definitions
│   │   ├── display/
│   │   │   ├── display_interface.h       # NEW: Display abstraction
│   │   │   └── lgfx_display.h            # EXTRACT: LovyanGFX wrapper
│   │   │   └── lgfx_display.cpp          # EXTRACT: Lines 584-827 from .ino
│   │   ├── touch/
│   │   │   ├── touch_interface.h         # NEW: Touch abstraction
│   │   │   └── lgfx_touch.h              # EXTRACT: Touch glue
│   │   │   └── lgfx_touch.cpp
│   │   ├── storage/
│   │   │   ├── storage_interface.h       # NEW: Storage abstraction
│   │   │   ├── littlefs_storage.h        # EXTRACT: LittleFS operations
│   │   │   ├── littlefs_storage.cpp
│   │   │   ├── sd_storage.h              # EXTRACT: Lines 156-255 from .ino
│   │   │   └── sd_storage.cpp
│   │   ├── network/
│   │   │   ├── network_interface.h       # NEW: Network abstraction
│   │   │   ├── wifi_network.h            # EXTRACT: WiFi management
│   │   │   └── wifi_network.cpp
│   │   ├── gpio/
│   │   │   ├── led_rgb.h                 # NEW: RGB LED control
│   │   │   ├── led_rgb.cpp
│   │   │   └── gpio_utils.h              # NEW: GPIO helpers
│   │   └── adc/
│   │       ├── battery_adc.h             # NEW: Battery monitoring
│   │       └── battery_adc.cpp
│   │   └── targets/
│   │       ├── target_profiles.h         # NEW: Hardware profile definitions
│   │       └── lcdwiki_35_esp32_32e.h    # NEW: LCDWIKI 3.5" profile
│   ├── drivers/
│   │   ├── touch/
│   │   │   ├── xpt2046.h                 # EXTRACT: XPT2046 touch driver
│   │   │   ├── xpt2046.cpp
│   │   │   ├── gt911.h                   # EXTRACT: GT911 touch driver
│   │   │   └── gt911.cpp
│   │   ├── display/
│   │   │   ├── st7796.h                  # EXTRACT: ST7796 panel driver
│   │   │   ├── st7796.cpp
│   │   │   ├── ili9341.h                 # EXTRACT: ILI9341 panel driver
│   │   │   ├── ili9341.cpp
│   │   │   └── st7789.h                  # EXTRACT: ST7789 panel driver
│   │   │   └── st7789.cpp
│   │   └── i2c/
│   │       └── i2c_scanner.h             # EXTRACT: I2C scan logic
│   │       └── i2c_scanner.cpp           # EXTRACT: Lines 2836-2881 from .ino
│   ├── services/
│   │   ├── i2c_manager/
│   │   │   ├── i2c_manager.h             # EXTRACT: I2C service
│   │   │   └── i2c_manager.cpp           # EXTRACT: Fast/deep scan logic
│   │   ├── serialwombat/
│   │   │   ├── serialwombat_manager.h    # EXTRACT: SW device management
│   │   │   └── serialwombat_manager.cpp  # EXTRACT: Pin modes, device modes
│   │   ├── firmware_manager/
│   │   │   ├── firmware_manager.h        # EXTRACT: Firmware operations
│   │   │   ├── firmware_manager.cpp      # EXTRACT: Upload, flash, metadata
│   │   │   ├── hex_parser.h              # EXTRACT: IntelHexSW8B class
│   │   │   └── hex_parser.cpp            # EXTRACT: Lines 1125-1500 from .ino
│   │   ├── web_server/
│   │   │   ├── web_server.h              # EXTRACT: HTTP server
│   │   │   ├── web_server.cpp            # EXTRACT: Route handlers
│   │   │   ├── html_templates.h          # EXTRACT: HTML strings
│   │   │   ├── html_templates.cpp        # EXTRACT: Lines 1769-2588 from .ino
│   │   │   ├── api_handlers.h            # EXTRACT: REST API routes
│   │   │   └── api_handlers.cpp          # EXTRACT: Lines 2950-4550 from .ino
│   │   ├── file_manager/
│   │   │   ├── file_manager.h            # EXTRACT: File operations
│   │   │   └── file_manager.cpp          # EXTRACT: Browse, upload, delete
│   │   ├── ota/
│   │   │   ├── ota_service.h             # EXTRACT: OTA updates
│   │   │   └── ota_service.cpp
│   │   ├── security/
│   │   │   ├── auth_service.h            # EXTRACT: Authentication
│   │   │   ├── auth_service.cpp          # EXTRACT: Lines 1653-1751 from .ino
│   │   │   ├── validators.h              # EXTRACT: Input validation
│   │   │   └── validators.cpp
│   │   ├── system_info/
│   │   │   ├── system_info.h             # EXTRACT: System diagnostics
│   │   │   └── system_info.cpp
│   │   ├── power_manager/
│   │   │   ├── power_manager.h           # NEW: Battery monitoring
│   │   │   └── power_manager.cpp
│   │   ├── tcp_bridge/
│   │   │   ├── tcp_bridge.h              # EXTRACT: TCP-to-I2C bridge
│   │   │   └── tcp_bridge.cpp
│   │   └── mqtt/
│   │       ├── mqtt_service.h            # NEW: MQTT client (placeholder for future)
│   │       └── mqtt_service.cpp
│   ├── config/
│   │   ├── config_manager.h              # EXTRACT: Config load/save
│   │   ├── config_manager.cpp            # EXTRACT: Lines 259-582 from .ino
│   │   ├── system_config.h               # EXTRACT: SystemConfig struct
│   │   ├── defaults.h                    # NEW: Default config values
│   │   ├── defaults.cpp
│   │   ├── validator.h                   # NEW: Config validation
│   │   ├── validator.cpp
│   │   └── presets.h                     # EXTRACT: CYD model presets
│   │   └── presets.cpp
│   └── ui/
│       ├── ui_manager.h                  # NEW: LVGL UI coordinator
│       ├── ui_manager.cpp
│       ├── lvgl_wrapper.h                # EXTRACT: LVGL v8/v9 glue
│       ├── lvgl_wrapper.cpp              # EXTRACT: Lines 1021-1123 from .ino
│       ├── navigation.h                  # NEW: Screen navigation
│       ├── navigation.cpp
│       ├── theme.h                       # NEW: UI theme/colors
│       ├── theme.cpp
│       ├── components/
│       │   ├── statusbar.h               # EXTRACT: Status bar
│       │   ├── statusbar.cpp             # EXTRACT: Lines 844-894 from .ino
│       │   ├── toast.h                   # NEW: Toast notifications
│       │   ├── toast.cpp
│       │   └── messagebox.h              # NEW: Message boxes
│       │   └── messagebox.cpp
│       ├── screens/
│       │   ├── home.h                    # NEW: Home dashboard
│       │   ├── home.cpp
│       │   ├── system_status.h           # NEW: System info screen
│       │   ├── system_status.cpp
│       │   ├── network.h                 # NEW: Network config screen
│       │   ├── network.cpp
│       │   ├── storage.h                 # NEW: Storage manager screen
│       │   ├── storage.cpp
│       │   ├── i2c_tools.h               # NEW: I2C scanner screen
│       │   ├── i2c_tools.cpp
│       │   ├── serialwombat.h            # NEW: SerialWombat manager screen
│       │   ├── serialwombat.cpp
│       │   ├── firmware.h                # NEW: Firmware manager screen
│       │   ├── firmware.cpp
│       │   ├── touch_calibration.h       # NEW: Touch calibration screen
│       │   ├── touch_calibration.cpp
│       │   ├── logs.h                    # NEW: Log viewer screen
│       │   ├── logs.cpp
│       │   ├── settings.h                # NEW: Settings screen
│       │   ├── settings.cpp
│       │   ├── setup_wizard.h            # EXTRACT: First-boot wizard
│       │   └── setup_wizard.cpp          # EXTRACT: Lines 895-1018 from .ino
│       └── assets/
│           ├── icons.h                   # NEW: LVGL symbol icons
│           └── fonts.h                   # NEW: Custom fonts (if needed)
├── platformio.ini                        # UPDATE: Add src_dir, include_dir
├── cyd_predecls.h                        # KEEP: Forward declarations
├── docs/
│   ├── FEATURE_MATRIX.md                 # NEW: This file (already created)
│   ├── REFACTOR_MAP.md                   # NEW: This file
│   ├── SMOKETEST.md                      # NEW: Test procedures
│   ├── BUILD.md                          # NEW: Build instructions
│   └── API.md                            # NEW: Module API documentation
└── CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino  # ARCHIVE: Original (reference only)
```

## Extraction Strategy

### Phase 1A: Core Infrastructure (Foundation)
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/core/types.h` | New | - | Common types (oflag_t, enums) |
| `src/core/logging.h` | New | - | Logging macros/functions |
| `src/core/error_codes.h` | New | - | Error code definitions |
| `src/core/event_bus.h` | New | - | Event system for decoupling |

### Phase 1B: Configuration System
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/config/system_config.h` | Extract | 259-350 | SystemConfig struct, enums |
| `src/config/config_manager.cpp` | Extract | 351-582 | loadConfig, saveConfig, JSON |
| `src/config/presets.cpp` | Extract | 487-557 | applyModelPreset() |
| `src/config/defaults.cpp` | New + Extract | 59-149 | Compile-time defaults + LCDWIKI profile |

### Phase 1C: HAL Layer
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/hal/display/lgfx_display.cpp` | Extract | 584-760 | LGFX class, panel/bus setup |
| `src/hal/touch/lgfx_touch.cpp` | Extract | 761-827 | Touch configuration |
| `src/hal/storage/sd_storage.cpp` | Extract | 156-255 | SD abstraction (SdFat) |
| `src/hal/storage/littlefs_storage.cpp` | Extract | Scattered | LittleFS operations |
| `src/hal/network/wifi_network.cpp` | Extract | In setup() | WiFiManager integration |
| `src/hal/gpio/led_rgb.cpp` | New | - | RGB LED control (GPIO 22/16/17) |
| `src/hal/adc/battery_adc.cpp` | New | - | Battery ADC (GPIO 34) |
| `src/hal/targets/lcdwiki_35_esp32_32e.h` | New | - | Default hardware profile |

### Phase 1D: Drivers
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/drivers/i2c/i2c_scanner.cpp` | Extract | 2836-2881 | Fast/deep scan logic |
| `src/drivers/touch/xpt2046.cpp` | Extract | 761-800 | XPT2046 SPI touch |
| `src/drivers/touch/gt911.cpp` | Extract | 801-827 | GT911 I2C touch |

### Phase 1E: Services
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/services/i2c_manager/i2c_manager.cpp` | Extract | 2836-2881, 2994-3006 | I2C scan + deep scan |
| `src/services/serialwombat/serialwombat_manager.cpp` | Extract | 3101-3400 | SW device management |
| `src/services/firmware_manager/firmware_manager.cpp` | Extract | 3411-3601 | Firmware operations |
| `src/services/firmware_manager/hex_parser.cpp` | Extract | 1125-1500 | IntelHexSW8B class |
| `src/services/web_server/web_server.cpp` | Extract | 4560-4600 | WebServer setup, routes |
| `src/services/web_server/api_handlers.cpp` | Extract | 2950-4550 | All HTTP handlers |
| `src/services/web_server/html_templates.cpp` | Extract | 1769-2588 | HTML in PROGMEM |
| `src/services/security/auth_service.cpp` | Extract | 1577-1630, 1653-1751 | checkAuth, validators |
| `src/services/file_manager/file_manager.cpp` | Extract | 4241-4530 | SD/LFS file operations |
| `src/services/ota/ota_service.cpp` | Extract | In setup() | ArduinoOTA setup |
| `src/services/system_info/system_info.cpp` | New + Extract | Scattered | Heap, uptime, reset reason |
| `src/services/power_manager/power_manager.cpp` | New | - | Battery ADC monitoring |
| `src/services/tcp_bridge/tcp_bridge.cpp` | Extract | In loop() | TCP-to-I2C bridge |

### Phase 1F: UI Layer
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/ui/lvgl_wrapper.cpp` | Extract | 1021-1123 | LVGL init, v8/v9 compat |
| `src/ui/components/statusbar.cpp` | Extract | 844-894 | Status bar with indicators |
| `src/ui/screens/setup_wizard.cpp` | Extract | 895-1018 | First-boot model selector |
| `src/ui/screens/home.cpp` | New | - | Home dashboard (tiles) |
| `src/ui/screens/system_status.cpp` | New | - | System info screen |
| `src/ui/screens/network.cpp` | New | - | Network config screen |
| `src/ui/screens/storage.cpp` | New | - | Storage manager screen |
| `src/ui/screens/i2c_tools.cpp` | New | - | I2C scanner screen |
| `src/ui/screens/serialwombat.cpp` | New | - | SerialWombat manager screen |
| `src/ui/screens/firmware.cpp` | New | - | Firmware manager screen |
| `src/ui/screens/touch_calibration.cpp` | New | - | Touch calibration screen |
| `src/ui/screens/logs.cpp` | New | - | Log viewer screen |
| `src/ui/screens/settings.cpp` | New | - | Settings screen |
| `src/ui/navigation.cpp` | New | - | Screen navigation logic |
| `src/ui/components/toast.cpp` | New | - | Toast notifications |
| `src/ui/components/messagebox.cpp` | New | - | Message boxes |

### Phase 1G: Application Orchestration
| New File | Source | Lines from .ino | Description |
|----------|--------|----------------|-------------|
| `src/app/app.cpp` | New | - | Application lifecycle management |
| `src/main.cpp` | Extract | 4560-4767 | setup(), loop() only |

## Line-by-Line Mapping (Original .ino → New Structure)

| Original Lines | New Location | Content |
|----------------|--------------|---------|
| 1-35 | `src/main.cpp` (includes) | Header comments, includes |
| 36-53 | `src/main.cpp` (includes) | Forward declarations for SD helpers |
| 54-82 | `src/config/defaults.h` | Security config defines |
| 84-149 | `src/config/defaults.h` | Compile-time config, SD pins |
| 156-255 | `src/hal/storage/sd_storage.cpp` | SD abstraction layer (SdFat) |
| 259-350 | `src/config/system_config.h` | SystemConfig struct, enums |
| 351-486 | `src/config/config_manager.cpp` | JSON serialization helpers |
| 487-557 | `src/config/presets.cpp` | applyModelPreset() with 9 models |
| 558-582 | `src/config/config_manager.cpp` | loadConfig(), saveConfig() |
| 584-760 | `src/hal/display/lgfx_display.cpp` | LGFX class, panels, bus |
| 761-827 | `src/hal/touch/lgfx_touch.cpp` | Touch config (XPT2046, GT911) |
| 844-894 | `src/ui/components/statusbar.cpp` | buildStatusBar() |
| 895-999 | `src/ui/screens/setup_wizard.cpp` | firstBootShowModelSelect() |
| 1000-1018 | `src/ui/screens/setup_wizard.cpp` | firstBootShowSplashPicker() |
| 1021-1072 | `src/ui/lvgl_wrapper.cpp` | lvglInitIfEnabled() |
| 1073-1123 | `src/ui/lvgl_wrapper.cpp` | lvglTickAndUpdate() |
| 1125-1500 | `src/services/firmware_manager/hex_parser.cpp` | IntelHexSW8B class |
| 1559-1576 | `src/services/security/auth_service.cpp` | addSecurityHeaders() |
| 1577-1630 | `src/services/security/auth_service.cpp` | checkAuth() with rate limiting |
| 1631-1652 | `src/services/security/auth_service.cpp` | authenticateRequest() helpers |
| 1653-1717 | `src/services/security/validators.cpp` | isValidI2CAddress, isValidPinNumber, etc. |
| 1722-1751 | `src/services/security/validators.cpp` | sanitizeError, isPathSafe, etc. |
| 1769-2588 | `src/services/web_server/html_templates.cpp` | HTML strings (PROGMEM) |
| 2590-2700 | `src/services/file_manager/file_manager.cpp` | File list helpers |
| 2701-2835 | `src/services/web_server/api_handlers.cpp` | Helper functions for handlers |
| 2836-2881 | `src/drivers/i2c/i2c_scanner.cpp` | getDeepScanInfoSingle() |
| 2882-2949 | `src/services/serialwombat/serialwombat_manager.cpp` | applyConfiguration() |
| 2950-2993 | `src/services/web_server/api_handlers.cpp` | handleRoot() |
| 2994-3005 | `src/services/i2c_manager/i2c_manager.cpp` | handleScanData() |
| 3006-3100 | `src/services/i2c_manager/i2c_manager.cpp` | handleDeepScan() |
| 3101-3122 | `src/services/serialwombat/serialwombat_manager.cpp` | handleConnect() |
| 3123-3150 | `src/services/serialwombat/serialwombat_manager.cpp` | handleSetPin() |
| 3151-3250 | `src/services/serialwombat/serialwombat_manager.cpp` | handleChangeAddr() |
| 3251-3410 | `src/services/serialwombat/serialwombat_manager.cpp` | Other SW handlers |
| 3411-3451 | `src/services/firmware_manager/firmware_manager.cpp` | convertHexToFirmwareBin() |
| 3452-3553 | `src/services/firmware_manager/firmware_manager.cpp` | handleUploadHex() |
| 3554-3601 | `src/services/firmware_manager/firmware_manager.cpp` | handleUploadFW() |
| 3602-3734 | `src/services/firmware_manager/firmware_manager.cpp` | handleFlashFW() |
| 3735-3886 | `src/services/web_server/api_handlers.cpp` | handleApiVariant(), etc. |
| 3887-3917 | `src/config/config_manager.cpp` | handleConfigSave() |
| 3918-3936 | `src/config/config_manager.cpp` | handleConfigLoad() |
| 3937-4000 | `src/config/config_manager.cpp` | handleConfigList(), etc. |
| 4001-4240 | `src/services/web_server/api_handlers.cpp` | Misc API handlers |
| 4241-4280 | `src/services/file_manager/file_manager.cpp` | handleApiSdList() |
| 4281-4417 | `src/services/file_manager/file_manager.cpp` | handleApiSdDelete(), etc. |
| 4418-4491 | `src/services/file_manager/file_manager.cpp` | handleUploadSD() |
| 4492-4559 | `src/services/firmware_manager/firmware_manager.cpp` | handleApiSdImportFw() |
| 4560-4650 | `src/main.cpp`, `src/app/app.cpp` | setup() - initialization |
| 4651-4744 | `src/main.cpp`, `src/app/app.cpp` | setup() - route registration |
| 4745-4767 | `src/main.cpp` | loop() |

## Migration Checklist

### Pre-Refactor
- [x] Backup original .ino file
- [x] Document all features (FEATURE_MATRIX.md)
- [x] Create line-by-line mapping (this file)
- [ ] Ensure repo compiles successfully in original state

### During Refactor
- [ ] Create directory structure
- [ ] Extract core utilities (logging, types, errors)
- [ ] Extract config system
- [ ] Extract HAL layer
- [ ] Extract drivers
- [ ] Extract services
- [ ] Extract UI layer
- [ ] Create main.cpp entry point
- [ ] Update platformio.ini
- [ ] Verify each module compiles independently (where possible)

### Post-Refactor
- [ ] Verify full build succeeds
- [ ] Test basic functionality (WiFi, display, touch)
- [ ] Test I2C scan
- [ ] Test SerialWombat operations
- [ ] Test firmware upload/flash
- [ ] Test SD card operations
- [ ] Test web UI
- [ ] Test LVGL UI (all screens)
- [ ] Run clang-format on all new files
- [ ] Update README.md with new structure
- [ ] Create BUILD.md with instructions

## Breaking Changes & Migration

### API Changes
**None expected**: Internal refactoring only. External interfaces (web API, config JSON) remain unchanged.

### Config Changes
**New keys added**:
- `display.driver` (replaces implicit panel kind)
- `led.*` (new RGB LED support)
- `battery.*` (new battery monitoring)
- `touch.cal_*` (new calibration values)

**Backward compatibility**: Old configs auto-upgrade on first load.

### Build Changes
**Required**:
- `platformio.ini`: Update `src_dir = src` (was `.`)
- `platformio.ini`: Update `include_dir = src` (was `.`)

**Optional**:
- Add environment for LCDWIKI board: `[env:lcdwiki-35-esp32-32e]`

## Testing Strategy

### Unit Tests (Optional, not in scope)
Each module can be independently tested:
- `config_manager`: Load/save JSON
- `validators`: Input validation
- `hex_parser`: HEX file parsing
- `i2c_scanner`: Mock I2C device detection

### Integration Tests (Manual)
1. **Boot Test**: Device boots, WiFi connects, web UI loads
2. **Display Test**: LVGL UI renders, touch responds
3. **I2C Test**: Scan finds devices, deep scan works
4. **Storage Test**: SD mount, file operations
5. **Firmware Test**: Upload, flash to SerialWombat device
6. **Config Test**: Save/load config profiles

### Regression Tests (Critical)
- [ ] All existing web endpoints return same data
- [ ] Config JSON format unchanged
- [ ] Firmware flashing protocol unchanged
- [ ] I2C operations unchanged

## Rollback Plan
If refactoring fails:
1. `git checkout <previous-commit>`
2. Original `.ino` file preserved as backup
3. All functionality remains in original file (no destructive changes until refactor complete)

## Success Criteria
- [ ] Full build succeeds with zero errors
- [ ] All original features functional
- [ ] New LVGL UI operational
- [ ] LCDWIKI 3.5" board works with defaults
- [ ] Web UI unchanged
- [ ] No regressions in I2C, firmware, SD operations
- [ ] Code review passed
- [ ] Security scan passed (CodeQL)
- [ ] Documentation complete (BUILD.md, SMOKETEST.md, API.md)

## Timeline Estimate
- **Phase 1 (Refactor)**: 10-12 hours
- **Phase 2 (Config System)**: 2-3 hours
- **Phase 3 (LVGL UI)**: 8-10 hours
- **Phase 4 (Driver Integration)**: 2-3 hours
- **Phase 5 (Testing & Docs)**: 3-4 hours
- **Total**: 25-32 hours of development time

## Notes
- This is a **surgical refactor**, not a rewrite
- Every line of code is accounted for (no deletions, only relocations + additions)
- Original .ino preserved for reference until refactor validated
- Modular structure enables future enhancements (MQTT, more sensors, etc.)
- Clear module boundaries facilitate testing and maintenance
