# Message Origins Matrix

This document enumerates every subsystem/module and all operator-relevant events that originate INFO/WARN/ERROR messages in the PLC-style MessageCenter system.

## Subsystem Event Matrix

| Subsystem | Event | Severity | Message Code | Trigger Condition | Recovery/Action | UI Location |
|-----------|-------|----------|--------------|-------------------|-----------------|-------------|
| **boot** | Boot sequence start | INFO | BOOT_START | System power-on or reset | None (informational) | Messages, Boot Progress |
| **boot** | Early init begin | INFO | BOOT_01_EARLY_BEGIN | Serial and core init starts | None | Boot Progress |
| **boot** | Early init complete | INFO | BOOT_01_EARLY_OK | Serial initialized successfully | None | Boot Progress |
| **boot** | Config load begin | INFO | BOOT_02_CONFIG_BEGIN | Configuration loading starts | None | Boot Progress |
| **boot** | Config load success | INFO | BOOT_02_CONFIG_OK | Config loaded or defaults applied | None | Boot Progress |
| **boot** | Config load warning | WARN | BOOT_02_CONFIG_WARN | Config file missing, using defaults | Review config in Settings | Messages, Boot Progress |
| **boot** | Config parse error | ERROR | BOOT_02_CONFIG_FAIL | Config JSON malformed | Check config file, restore defaults | Messages, Boot Progress |
| **boot** | Filesystem begin | INFO | BOOT_03_FS_BEGIN | LittleFS mount attempt | None | Boot Progress |
| **boot** | Filesystem success | INFO | BOOT_03_FS_OK | LittleFS mounted successfully | None | Boot Progress |
| **boot** | Filesystem mount fail | ERROR | BOOT_03_FS_FAIL | LittleFS mount and format failed | Check flash integrity | Messages, Boot Progress |
| **boot** | SD mount begin | INFO | BOOT_04_SD_BEGIN | SD card mount attempt | None | Boot Progress |
| **boot** | SD mount success | INFO | BOOT_04_SD_OK | SD card detected and mounted | None | Boot Progress |
| **boot** | SD not present | WARN | BOOT_04_SD_NOT_PRESENT | SD card slot empty | Insert SD card if needed | Messages, Boot Progress |
| **boot** | SD mount fail | WARN | BOOT_04_SD_FAIL | SD card present but mount failed | Check SD card format/health | Messages, Boot Progress |
| **boot** | Display init begin | INFO | BOOT_05_DISPLAY_BEGIN | Display initialization starts | None | Boot Progress, Serial |
| **boot** | Display init success | INFO | BOOT_05_DISPLAY_OK | Display initialized successfully | None | Boot Progress |
| **boot** | Display init fail | ERROR | BOOT_05_DISPLAY_FAIL | Display hardware not responding | Check display connection | Messages, Serial |
| **boot** | Display disabled | WARN | BOOT_05_DISPLAY_DISABLED | Display disabled in config (headless) | Enable in config if needed | Messages |
| **boot** | Touch init begin | INFO | BOOT_06_TOUCH_BEGIN | Touch controller init starts | None | Boot Progress |
| **boot** | Touch init success | INFO | BOOT_06_TOUCH_OK | Touch initialized successfully | None | Boot Progress |
| **boot** | Touch init fail | WARN | BOOT_06_TOUCH_FAIL | Touch controller not responding | Check touch connection | Messages, Boot Progress |
| **boot** | Touch calibration required | WARN | BOOT_06_TOUCH_CAL_REQ | First boot, calibration needed | Complete touch calibration | Messages, Setup Wizard |
| **boot** | Network init begin | INFO | BOOT_07_NET_BEGIN | WiFi initialization starts | None | Boot Progress |
| **boot** | Network connected | INFO | BOOT_07_NET_OK | WiFi connected successfully | None | Boot Progress, Status Bar |
| **boot** | Network AP fallback | WARN | BOOT_07_NET_AP_FALLBACK | WiFi failed, AP mode enabled | Connect to AP and configure WiFi | Messages, Status Bar |
| **boot** | Network fail | ERROR | BOOT_07_NET_FAIL | WiFi init completely failed | Check WiFi hardware | Messages |
| **boot** | Time sync begin | INFO | BOOT_08_TIME_BEGIN | NTP time sync attempt | None | Boot Progress |
| **boot** | Time sync success | INFO | BOOT_08_TIME_OK | Time synchronized via NTP | None | Boot Progress, Status Bar |
| **boot** | Time sync fail | WARN | BOOT_08_TIME_FAIL | NTP sync failed, using local time | Check network connectivity | Messages |
| **boot** | Services start begin | INFO | BOOT_09_SERVICES_BEGIN | Starting web/MQTT/OTA services | None | Boot Progress |
| **boot** | Services started | INFO | BOOT_09_SERVICES_OK | All services started successfully | None | Boot Progress |
| **boot** | Web server fail | ERROR | BOOT_09_WEB_FAIL | Web server failed to start | Check port 80 availability | Messages |
| **boot** | OTA fail | WARN | BOOT_09_OTA_FAIL | OTA service failed to start | Check network/credentials | Messages |
| **boot** | Self-test begin | INFO | BOOT_10_SELFTEST_BEGIN | Running optional self-tests | None | Boot Progress |
| **boot** | Self-test complete | INFO | BOOT_10_SELFTEST_OK | Self-tests passed | None | Boot Progress |
| **boot** | Self-test fail | WARN | BOOT_10_SELFTEST_FAIL | Self-test detected issues | Review test results | Messages |
| **boot** | Boot complete | INFO | BOOT_OK_READY | System ready for operation | None | Messages, Boot Progress |
| **boot** | Boot degraded | WARN | BOOT_DEGRADED | Boot completed with warnings | Review warnings | Messages |
| **fs** | LittleFS mount begin | INFO | FS_LFS_MOUNT_BEGIN | Mounting LittleFS | None | Boot Progress |
| **fs** | LittleFS mount ok | INFO | FS_LFS_MOUNT_OK | LittleFS mounted successfully | None | Boot Progress |
| **fs** | LittleFS mount fail | ERROR | FS_LFS_MOUNT_FAIL | LittleFS mount failed | Check flash integrity | Messages |
| **fs** | LittleFS format begin | WARN | FS_LFS_FORMAT_BEGIN | Auto-formatting LittleFS | None | Messages |
| **fs** | LittleFS format ok | INFO | FS_LFS_FORMAT_OK | Format completed successfully | None | Messages |
| **fs** | LittleFS format fail | ERROR | FS_LFS_FORMAT_FAIL | Format failed | Flash may be damaged | Messages |
| **fs** | Directory create fail | WARN | FS_DIR_CREATE_FAIL | Failed to create required directory | Check filesystem health | Messages |
| **fs** | Low space warning | WARN | FS_LOW_SPACE | < 10% free space remaining | Delete unused files | Messages, Dashboard |
| **fs** | Write error | ERROR | FS_WRITE_ERROR | File write operation failed | Check space/permissions | Messages |
| **fs** | Read error | ERROR | FS_READ_ERROR | File read operation failed | Check file integrity | Messages |
| **sd** | SD mount begin | INFO | SD_MOUNT_BEGIN | Attempting SD card mount | None | Boot Progress |
| **sd** | SD mount ok | INFO | SD_MOUNT_OK | SD card mounted successfully | None | Messages, Dashboard |
| **sd** | SD not present | WARN | SD_NOT_PRESENT | No SD card detected | Insert SD card if needed | Messages, Dashboard |
| **sd** | SD mount fail | WARN | SD_MOUNT_FAIL | SD card mount failed | Check SD format (FAT32) | Messages, Dashboard |
| **sd** | SD ejected | INFO | SD_EJECTED | SD card safely ejected | None | Messages, Dashboard |
| **sd** | SD inserted | INFO | SD_INSERTED | SD card detected | None | Messages, Dashboard |
| **sd** | SD removed (unexpected) | WARN | SD_REMOVED | SD removed without eject | Use eject before removal | Messages, Dashboard |
| **sd** | SD write error | ERROR | SD_WRITE_ERROR | Write to SD failed | Check SD card health | Messages |
| **sd** | SD read error | ERROR | SD_READ_ERROR | Read from SD failed | Check SD card health | Messages |
| **sd** | SD low space | WARN | SD_LOW_SPACE | < 10% free space on SD | Free up space | Messages, Dashboard |
| **sd** | SD full | ERROR | SD_FULL | SD card full | Delete files or use larger card | Messages, Dashboard |
| **config** | Config load begin | INFO | CFG_LOAD_BEGIN | Loading configuration | None | Boot Progress |
| **config** | Config load ok | INFO | CFG_LOAD_OK | Configuration loaded | None | Boot Progress |
| **config** | Config default applied | WARN | CFG_DEFAULT_APPLIED | Config missing, defaults used | Configure system in Settings | Messages |
| **config** | Config parse error | ERROR | CFG_PARSE_ERROR | JSON parsing failed | Fix or delete config.json | Messages |
| **config** | Config save ok | INFO | CFG_SAVE_OK | Configuration saved | None | Messages |
| **config** | Config save fail | ERROR | CFG_SAVE_FAIL | Failed to save configuration | Check filesystem | Messages |
| **config** | Config validate fail | ERROR | CFG_VALIDATE_FAIL | Invalid configuration values | Review config parameters | Messages |
| **config** | Config import ok | INFO | CFG_IMPORT_OK | Configuration imported from file | None | Messages |
| **config** | Config export ok | INFO | CFG_EXPORT_OK | Configuration exported | None | Messages |
| **display** | Display init begin | INFO | DISP_INIT_BEGIN | Display initialization starts | None | Boot Progress |
| **display** | Display init ok | INFO | DISP_INIT_OK | Display ready | None | Boot Progress |
| **display** | Display init fail | ERROR | DISP_INIT_FAIL | Display hardware failure | Check connections | Messages, Serial |
| **display** | Display disabled | WARN | DISP_DISABLED | Headless mode active | Enable in config if needed | Messages, Serial |
| **display** | Framebuffer alloc fail | ERROR | DISP_FB_ALLOC_FAIL | Not enough RAM for framebuffer | Reduce buffer size | Messages |
| **display** | Panel not responding | ERROR | DISP_PANEL_NO_RESP | Display panel communication error | Check SPI connections | Messages |
| **touch** | Touch init begin | INFO | TOUCH_INIT_BEGIN | Touch controller init starts | None | Boot Progress |
| **touch** | Touch init ok | INFO | TOUCH_INIT_OK | Touch controller ready | None | Boot Progress |
| **touch** | Touch init fail | WARN | TOUCH_INIT_FAIL | Touch controller not found | Check connections, use mouse | Messages |
| **touch** | Touch calibration required | WARN | TOUCH_CAL_REQUIRED | Calibration needed | Run calibration wizard | Messages, UI |
| **touch** | Touch calibration ok | INFO | TOUCH_CAL_OK | Calibration completed | None | Messages |
| **touch** | Touch not responding | WARN | TOUCH_NO_RESP | Touch controller stopped responding | Restart or check hardware | Messages |
| **i2c** | I2C init begin | INFO | I2C_INIT_BEGIN | I2C bus initialization | None | Boot Progress |
| **i2c** | I2C init ok | INFO | I2C_INIT_OK | I2C bus ready | None | Boot Progress |
| **i2c** | I2C bus stuck | ERROR | I2C_BUS_STUCK | SDA/SCL lines stuck | Check pullups, power cycle | Messages |
| **i2c** | I2C device not found | WARN | I2C_DEVICE_NOT_FOUND | Expected device missing at address | Check connections | Messages, Scanner |
| **i2c** | I2C comm error | WARN | I2C_COMM_ERROR | Communication error with device | Check wiring/power | Messages |
| **i2c** | I2C scan complete | INFO | I2C_SCAN_COMPLETE | Bus scan finished | None | Messages, Scanner |
| **serialwombat** | SW init begin | INFO | SW_INIT_BEGIN | SerialWombat init starts | None | Boot Progress |
| **serialwombat** | SW connected | INFO | SW_CONNECTED | SerialWombat device found | None | Messages, Dashboard |
| **serialwombat** | SW not found | WARN | SW_NOT_FOUND | No SerialWombat detected | Check I2C connection | Messages, Dashboard |
| **serialwombat** | SW version mismatch | WARN | SW_VERSION_MISMATCH | Firmware version incompatible | Update SerialWombat firmware | Messages |
| **serialwombat** | SW comm error | ERROR | SW_COMM_ERROR | Communication failure | Check I2C bus | Messages |
| **serialwombat** | SW addr changed | INFO | SW_ADDR_CHANGED | Device address changed | None | Messages |
| **serialwombat** | SW pin mode set | INFO | SW_PIN_MODE_SET | Pin configuration applied | None | Messages (if manual) |
| **serialwombat** | SW reset | INFO | SW_RESET | Device reset commanded | None | Messages |
| **net** | WiFi init begin | INFO | NET_WIFI_INIT_BEGIN | WiFi subsystem init | None | Boot Progress |
| **net** | WiFi connecting | INFO | NET_WIFI_CONNECTING | Attempting WiFi connection | None | Status Bar |
| **net** | WiFi connected | INFO | NET_WIFI_CONNECTED | WiFi connected successfully | None | Messages, Status Bar |
| **net** | WiFi disconnected | WARN | NET_WIFI_DISCONNECTED | WiFi connection lost | Check AP/credentials | Messages, Status Bar |
| **net** | WiFi auth fail | ERROR | NET_WIFI_AUTH_FAIL | Authentication failed | Check WiFi password | Messages |
| **net** | WiFi AP mode | WARN | NET_WIFI_AP_MODE | Running in AP mode | Connect and configure WiFi | Messages, Status Bar |
| **net** | WiFi reconnecting | INFO | NET_WIFI_RECONNECT | Automatic reconnection attempt | None | Status Bar |
| **net** | IP assigned | INFO | NET_IP_ASSIGNED | IP address obtained | None | Messages, Dashboard |
| **net** | DHCP fail | WARN | NET_DHCP_FAIL | Failed to obtain IP | Check DHCP server | Messages |
| **net** | DNS fail | WARN | NET_DNS_FAIL | DNS resolution failed | Check DNS settings | Messages |
| **net** | NTP sync begin | INFO | NET_NTP_SYNC_BEGIN | Syncing time with NTP | None | Boot Progress |
| **net** | NTP sync ok | INFO | NET_NTP_SYNC_OK | Time synchronized | None | Messages, Status Bar |
| **net** | NTP sync fail | WARN | NET_NTP_SYNC_FAIL | NTP sync failed | Check network/time servers | Messages |
| **mqtt** | MQTT init begin | INFO | MQTT_INIT_BEGIN | MQTT client initialization | None | Boot Progress |
| **mqtt** | MQTT connected | INFO | MQTT_CONNECTED | MQTT broker connected | None | Messages, Dashboard |
| **mqtt** | MQTT disconnected | WARN | MQTT_DISCONNECTED | Connection to broker lost | Check broker/network | Messages, Dashboard |
| **mqtt** | MQTT auth fail | ERROR | MQTT_AUTH_FAIL | Authentication failed | Check credentials | Messages |
| **mqtt** | MQTT reconnecting | INFO | MQTT_RECONNECTING | Automatic reconnection | None | Dashboard |
| **mqtt** | MQTT publish fail | WARN | MQTT_PUBLISH_FAIL | Failed to publish message | Check connection | Messages |
| **mqtt** | MQTT subscribe fail | WARN | MQTT_SUBSCRIBE_FAIL | Failed to subscribe to topic | Check permissions | Messages |
| **web** | Web server start | INFO | WEB_SERVER_START | Web server starting | None | Boot Progress |
| **web** | Web server ok | INFO | WEB_SERVER_OK | Web server listening on port 80 | None | Messages |
| **web** | Web server fail | ERROR | WEB_SERVER_FAIL | Failed to start web server | Check port availability | Messages |
| **web** | Web server stopped | WARN | WEB_SERVER_STOPPED | Web server stopped | Restart system | Messages |
| **web** | Auth failure | WARN | WEB_AUTH_FAIL | HTTP authentication failed | Check credentials | Messages, Logs |
| **web** | Rate limit hit | WARN | WEB_RATE_LIMIT | Too many auth attempts | Wait before retry | Messages |
| **web** | Upload success | INFO | WEB_UPLOAD_OK | File uploaded successfully | None | Messages |
| **web** | Upload fail | ERROR | WEB_UPLOAD_FAIL | File upload failed | Check file size/format | Messages |
| **ota** | OTA init begin | INFO | OTA_INIT_BEGIN | OTA service initialization | None | Boot Progress |
| **ota** | OTA init ok | INFO | OTA_INIT_OK | OTA service ready | None | Messages |
| **ota** | OTA init fail | WARN | OTA_INIT_FAIL | OTA service failed to start | Check network/credentials | Messages |
| **ota** | OTA update available | INFO | OTA_UPDATE_AVAILABLE | New firmware available | Review and apply if desired | Messages, Dashboard |
| **ota** | OTA update start | INFO | OTA_UPDATE_START | Firmware update beginning | Do not power off | Messages, UI |
| **ota** | OTA progress | INFO | OTA_PROGRESS | Update in progress (%) | None | UI Progress Bar |
| **ota** | OTA update ok | INFO | OTA_UPDATE_OK | Firmware updated successfully | None | Messages |
| **ota** | OTA update fail | ERROR | OTA_UPDATE_FAIL | Firmware update failed | Retry or check image | Messages |
| **ota** | OTA auth fail | ERROR | OTA_AUTH_FAIL | OTA authentication failed | Check OTA password | Messages |
| **ota** | OTA begin fail | ERROR | OTA_BEGIN_FAIL | Update begin failed | Check flash space | Messages |
| **ota** | OTA write fail | ERROR | OTA_WRITE_FAIL | Write error during update | Do not interrupt | Messages |
| **ota** | OTA end fail | ERROR | OTA_END_FAIL | Update finalization failed | System may be unstable | Messages |
| **tcp** | TCP bridge start | INFO | TCP_BRIDGE_START | TCP bridge starting | None | Boot Progress |
| **tcp** | TCP bridge ok | INFO | TCP_BRIDGE_OK | TCP bridge listening on port 3000 | None | Messages |
| **tcp** | TCP client connected | INFO | TCP_CLIENT_CONNECTED | Client connected to bridge | None | Messages (debug) |
| **tcp** | TCP client disconnected | INFO | TCP_CLIENT_DISCONNECTED | Client disconnected | None | Messages (debug) |
| **tcp** | TCP bridge fail | ERROR | TCP_BRIDGE_FAIL | Failed to start TCP bridge | Check port availability | Messages |
| **security** | Default password warning | ERROR | SEC_DEFAULT_PASSWORD | Default password detected | Change password immediately | Messages, Serial |
| **security** | Security disabled warning | WARN | SEC_DISABLED | Authentication disabled | Enable security for production | Messages, Serial |
| **security** | Auth lockout | WARN | SEC_AUTH_LOCKOUT | Too many failed attempts | Wait before retry | Messages |
| **security** | Password changed | INFO | SEC_PASSWORD_CHANGED | Password updated | None | Messages |
| **security** | Session expired | INFO | SEC_SESSION_EXPIRED | User session timed out | Re-authenticate | Messages |
| **firmware** | Hex conversion start | INFO | FW_CONVERT_START | Converting hex file | None | Messages, UI |
| **firmware** | Hex conversion ok | INFO | FW_CONVERT_OK | Hex file converted | None | Messages |
| **firmware** | Hex parse error | ERROR | FW_PARSE_ERROR | Invalid hex file format | Check file integrity | Messages |
| **firmware** | Flash write ok | INFO | FW_FLASH_OK | Firmware written to device | None | Messages |
| **firmware** | Flash write fail | ERROR | FW_FLASH_FAIL | Failed to write firmware | Check device connection | Messages |
| **firmware** | CRC error | ERROR | FW_CRC_ERROR | Firmware CRC mismatch | Re-upload file | Messages |
| **adc** | Battery ADC init | INFO | ADC_BATTERY_INIT | Battery ADC initialized | None | Boot Progress |
| **adc** | Battery low | WARN | ADC_BATTERY_LOW | Battery voltage below threshold | Charge or connect power | Messages, Status Bar |
| **adc** | Battery critical | ERROR | ADC_BATTERY_CRITICAL | Battery critically low | Connect power immediately | Messages, Status Bar |
| **adc** | Battery charging | INFO | ADC_BATTERY_CHARGING | Battery charging detected | None | Status Bar |
| **system** | Watchdog reset | WARN | SYS_WATCHDOG_RESET | System reset by watchdog | Check for hangs/crashes | Messages |
| **system** | Brown-out reset | WARN | SYS_BROWNOUT_RESET | Power supply voltage drop | Check power supply | Messages |
| **system** | Heap low | WARN | SYS_HEAP_LOW | Low free heap memory | Check for memory leaks | Messages, Dashboard |
| **system** | Heap critical | ERROR | SYS_HEAP_CRITICAL | Critical heap shortage | System may crash | Messages |
| **system** | Task crash | ERROR | SYS_TASK_CRASH | Background task crashed | Check logs, may restart | Messages |
| **system** | Stack overflow | ERROR | SYS_STACK_OVERFLOW | Task stack overflow detected | Increase stack size | Messages |
| **ui** | UI init begin | INFO | UI_INIT_BEGIN | LVGL UI initialization | None | Boot Progress |
| **ui** | UI init ok | INFO | UI_INIT_OK | LVGL UI ready | None | Messages |
| **ui** | UI init fail | ERROR | UI_INIT_FAIL | LVGL init failed | Check display/memory | Messages, Serial |
| **ui** | Screen transition fail | WARN | UI_SCREEN_FAIL | Screen load failed | May revert to previous screen | Messages |
| **ui** | Splash load fail | WARN | UI_SPLASH_FAIL | Splash image not found | Using default splash | Messages |
| **ui** | Setup wizard start | INFO | UI_WIZARD_START | First-boot setup started | Complete configuration | Messages, UI |
| **ui** | Setup wizard complete | INFO | UI_WIZARD_COMPLETE | First-boot setup completed | None | Messages |

## Summary Statistics

- **Total Subsystems**: 18 (boot, fs, sd, config, display, touch, i2c, serialwombat, net, mqtt, web, ota, tcp, security, firmware, adc, system, ui)
- **Total Message Origins**: 165+ unique event types
- **INFO Messages**: ~85 (normal operational events)
- **WARN Messages**: ~55 (degraded but functional)
- **ERROR Messages**: ~45 (requires operator attention)

## Usage Guidelines

1. **Every subsystem** must use MessageCenter to post operator-relevant events
2. **Message codes** are stable and defined in `src/core/messages/message_codes.h`
3. **Source names** must match the subsystem column exactly
4. **All messages** require acknowledgment (PLC style) to move from active to history
5. **Coalescing**: duplicate {severity, source, code} increments count rather than creating new message
6. **UI integration**: Both LVGL and Web UI display active messages, badge count, and provide ACK controls
