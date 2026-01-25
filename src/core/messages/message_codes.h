/*
 * Message Codes Registry
 * 
 * Defines all stable message codes used throughout the system.
 * These codes NEVER change - they are part of the API contract.
 * 
 * Format: SUBSYS_EVENT (e.g., BOOT_START, FS_LFS_MOUNT_OK, NET_WIFI_CONNECTED)
 */

#ifndef MESSAGE_CODES_H
#define MESSAGE_CODES_H

// ===================================================================================
// Boot Messages
// ===================================================================================
#define BOOT_START                   "BOOT_START"
#define BOOT_01_EARLY_BEGIN          "BOOT_01_EARLY_BEGIN"
#define BOOT_01_EARLY_OK             "BOOT_01_EARLY_OK"
#define BOOT_02_CONFIG_BEGIN         "BOOT_02_CONFIG_BEGIN"
#define BOOT_02_CONFIG_OK            "BOOT_02_CONFIG_OK"
#define BOOT_02_CONFIG_WARN          "BOOT_02_CONFIG_WARN"
#define BOOT_02_CONFIG_FAIL          "BOOT_02_CONFIG_FAIL"
#define BOOT_03_FS_BEGIN             "BOOT_03_FS_BEGIN"
#define BOOT_03_FS_OK                "BOOT_03_FS_OK"
#define BOOT_03_FS_FAIL              "BOOT_03_FS_FAIL"
#define BOOT_04_SD_BEGIN             "BOOT_04_SD_BEGIN"
#define BOOT_04_SD_OK                "BOOT_04_SD_OK"
#define BOOT_04_SD_NOT_PRESENT       "BOOT_04_SD_NOT_PRESENT"
#define BOOT_04_SD_FAIL              "BOOT_04_SD_FAIL"
#define BOOT_05_DISPLAY_BEGIN        "BOOT_05_DISPLAY_BEGIN"
#define BOOT_05_DISPLAY_OK           "BOOT_05_DISPLAY_OK"
#define BOOT_05_DISPLAY_DISABLED     "BOOT_05_DISPLAY_DISABLED"
#define BOOT_05_DISPLAY_FAIL         "BOOT_05_DISPLAY_FAIL"
#define BOOT_06_TOUCH_BEGIN          "BOOT_06_TOUCH_BEGIN"
#define BOOT_06_TOUCH_OK             "BOOT_06_TOUCH_OK"
#define BOOT_06_TOUCH_FAIL           "BOOT_06_TOUCH_FAIL"
#define BOOT_06_TOUCH_CAL_REQ        "BOOT_06_TOUCH_CAL_REQ"
#define BOOT_07_NET_BEGIN            "BOOT_07_NET_BEGIN"
#define BOOT_07_NET_OK               "BOOT_07_NET_OK"
#define BOOT_07_NET_AP_FALLBACK      "BOOT_07_NET_AP_FALLBACK"
#define BOOT_07_NET_FAIL             "BOOT_07_NET_FAIL"
#define BOOT_08_TIME_BEGIN           "BOOT_08_TIME_BEGIN"
#define BOOT_08_TIME_OK              "BOOT_08_TIME_OK"
#define BOOT_08_TIME_FAIL            "BOOT_08_TIME_FAIL"
#define BOOT_09_SERVICES_BEGIN       "BOOT_09_SERVICES_BEGIN"
#define BOOT_09_SERVICES_OK          "BOOT_09_SERVICES_OK"
#define BOOT_09_WEB_FAIL             "BOOT_09_WEB_FAIL"
#define BOOT_09_OTA_FAIL             "BOOT_09_OTA_FAIL"
#define BOOT_10_SELFTEST_BEGIN       "BOOT_10_SELFTEST_BEGIN"
#define BOOT_10_SELFTEST_OK          "BOOT_10_SELFTEST_OK"
#define BOOT_10_SELFTEST_FAIL        "BOOT_10_SELFTEST_FAIL"
#define BOOT_OK_READY                "BOOT_OK_READY"
#define BOOT_DEGRADED                "BOOT_DEGRADED"

// ===================================================================================
// Filesystem Messages (LittleFS)
// ===================================================================================
#define FS_LFS_MOUNT_BEGIN           "FS_LFS_MOUNT_BEGIN"
#define FS_LFS_MOUNT_OK              "FS_LFS_MOUNT_OK"
#define FS_LFS_MOUNT_FAIL            "FS_LFS_MOUNT_FAIL"
#define FS_LFS_FORMAT_BEGIN          "FS_LFS_FORMAT_BEGIN"
#define FS_LFS_FORMAT_OK             "FS_LFS_FORMAT_OK"
#define FS_LFS_FORMAT_FAIL           "FS_LFS_FORMAT_FAIL"
#define FS_DIR_CREATE_FAIL           "FS_DIR_CREATE_FAIL"
#define FS_LOW_SPACE                 "FS_LOW_SPACE"
#define FS_WRITE_ERROR               "FS_WRITE_ERROR"
#define FS_READ_ERROR                "FS_READ_ERROR"

// ===================================================================================
// SD Card Messages
// ===================================================================================
#define SD_MOUNT_BEGIN               "SD_MOUNT_BEGIN"
#define SD_MOUNT_OK                  "SD_MOUNT_OK"
#define SD_NOT_PRESENT               "SD_NOT_PRESENT"
#define SD_MOUNT_FAIL                "SD_MOUNT_FAIL"
#define SD_EJECTED                   "SD_EJECTED"
#define SD_INSERTED                  "SD_INSERTED"
#define SD_REMOVED                   "SD_REMOVED"
#define SD_WRITE_ERROR               "SD_WRITE_ERROR"
#define SD_READ_ERROR                "SD_READ_ERROR"
#define SD_LOW_SPACE                 "SD_LOW_SPACE"
#define SD_FULL                      "SD_FULL"

// ===================================================================================
// Configuration Messages
// ===================================================================================
#define CFG_LOAD_BEGIN               "CFG_LOAD_BEGIN"
#define CFG_LOAD_OK                  "CFG_LOAD_OK"
#define CFG_DEFAULT_APPLIED          "CFG_DEFAULT_APPLIED"
#define CFG_PARSE_ERROR              "CFG_PARSE_ERROR"
#define CFG_SAVE_OK                  "CFG_SAVE_OK"
#define CFG_SAVE_FAIL                "CFG_SAVE_FAIL"
#define CFG_VALIDATE_FAIL            "CFG_VALIDATE_FAIL"
#define CFG_IMPORT_OK                "CFG_IMPORT_OK"
#define CFG_EXPORT_OK                "CFG_EXPORT_OK"

// ===================================================================================
// Display Messages
// ===================================================================================
#define DISP_INIT_BEGIN              "DISP_INIT_BEGIN"
#define DISP_INIT_OK                 "DISP_INIT_OK"
#define DISP_INIT_FAIL               "DISP_INIT_FAIL"
#define DISP_DISABLED                "DISP_DISABLED"
#define DISP_FB_ALLOC_FAIL           "DISP_FB_ALLOC_FAIL"
#define DISP_PANEL_NO_RESP           "DISP_PANEL_NO_RESP"

// ===================================================================================
// Touch Messages
// ===================================================================================
#define TOUCH_INIT_BEGIN             "TOUCH_INIT_BEGIN"
#define TOUCH_INIT_OK                "TOUCH_INIT_OK"
#define TOUCH_INIT_FAIL              "TOUCH_INIT_FAIL"
#define TOUCH_CAL_REQUIRED           "TOUCH_CAL_REQUIRED"
#define TOUCH_CAL_OK                 "TOUCH_CAL_OK"
#define TOUCH_NO_RESP                "TOUCH_NO_RESP"

// ===================================================================================
// I2C Messages
// ===================================================================================
#define I2C_INIT_BEGIN               "I2C_INIT_BEGIN"
#define I2C_INIT_OK                  "I2C_INIT_OK"
#define I2C_BUS_OK                   "I2C_BUS_OK"
#define I2C_BUS_STUCK                "I2C_BUS_STUCK"
#define I2C_DEVICE_NOT_FOUND         "I2C_DEVICE_NOT_FOUND"
#define I2C_COMM_ERROR               "I2C_COMM_ERROR"
#define I2C_SCAN_COMPLETE            "I2C_SCAN_COMPLETE"

// ===================================================================================
// SerialWombat Messages
// ===================================================================================
#define SW_INIT_BEGIN                "SW_INIT_BEGIN"
#define SW_INIT_OK                   "SW_INIT_OK"
#define SW_CONNECTED                 "SW_CONNECTED"
#define SW_NOT_FOUND                 "SW_NOT_FOUND"
#define SW_VERSION_MISMATCH          "SW_VERSION_MISMATCH"
#define SW_COMM_ERROR                "SW_COMM_ERROR"
#define SW_ADDR_CHANGED              "SW_ADDR_CHANGED"
#define SW_PIN_MODE_SET              "SW_PIN_MODE_SET"
#define SW_RESET                     "SW_RESET"

// ===================================================================================
// Network Messages (WiFi)
// ===================================================================================
#define NET_WIFI_INIT_BEGIN          "NET_WIFI_INIT_BEGIN"
#define NET_WIFI_CONNECTING          "NET_WIFI_CONNECTING"
#define NET_WIFI_CONNECTED           "NET_WIFI_CONNECTED"
#define NET_WIFI_DISCONNECTED        "NET_WIFI_DISCONNECTED"
#define NET_WIFI_AUTH_FAIL           "NET_WIFI_AUTH_FAIL"
#define NET_WIFI_AP_MODE             "NET_WIFI_AP_MODE"
#define NET_WIFI_RECONNECT           "NET_WIFI_RECONNECT"
#define NET_IP_ASSIGNED              "NET_IP_ASSIGNED"
#define NET_DHCP_FAIL                "NET_DHCP_FAIL"
#define NET_DNS_FAIL                 "NET_DNS_FAIL"
#define NET_NTP_SYNC_BEGIN           "NET_NTP_SYNC_BEGIN"
#define NET_NTP_SYNC_OK              "NET_NTP_SYNC_OK"
#define NET_NTP_SYNC_FAIL            "NET_NTP_SYNC_FAIL"

// ===================================================================================
// MQTT Messages
// ===================================================================================
#define MQTT_INIT_BEGIN              "MQTT_INIT_BEGIN"
#define MQTT_CONNECTED               "MQTT_CONNECTED"
#define MQTT_DISCONNECTED            "MQTT_DISCONNECTED"
#define MQTT_AUTH_FAIL               "MQTT_AUTH_FAIL"
#define MQTT_RECONNECTING            "MQTT_RECONNECTING"
#define MQTT_PUBLISH_FAIL            "MQTT_PUBLISH_FAIL"
#define MQTT_SUBSCRIBE_FAIL          "MQTT_SUBSCRIBE_FAIL"

// ===================================================================================
// Web Server Messages
// ===================================================================================
#define WEB_SERVER_START             "WEB_SERVER_START"
#define WEB_SERVER_OK                "WEB_SERVER_OK"
#define WEB_SERVER_FAIL              "WEB_SERVER_FAIL"
#define WEB_SERVER_STOPPED           "WEB_SERVER_STOPPED"
#define WEB_AUTH_FAIL                "WEB_AUTH_FAIL"
#define WEB_RATE_LIMIT               "WEB_RATE_LIMIT"
#define WEB_UPLOAD_OK                "WEB_UPLOAD_OK"
#define WEB_UPLOAD_FAIL              "WEB_UPLOAD_FAIL"

// ===================================================================================
// OTA Messages
// ===================================================================================
#define OTA_INIT_BEGIN               "OTA_INIT_BEGIN"
#define OTA_INIT_OK                  "OTA_INIT_OK"
#define OTA_INIT_FAIL                "OTA_INIT_FAIL"
#define OTA_UPDATE_AVAILABLE         "OTA_UPDATE_AVAILABLE"
#define OTA_UPDATE_START             "OTA_UPDATE_START"
#define OTA_PROGRESS                 "OTA_PROGRESS"
#define OTA_UPDATE_OK                "OTA_UPDATE_OK"
#define OTA_UPDATE_FAIL              "OTA_UPDATE_FAIL"
#define OTA_AUTH_FAIL                "OTA_AUTH_FAIL"
#define OTA_BEGIN_FAIL               "OTA_BEGIN_FAIL"
#define OTA_WRITE_FAIL               "OTA_WRITE_FAIL"
#define OTA_END_FAIL                 "OTA_END_FAIL"

// ===================================================================================
// TCP Bridge Messages
// ===================================================================================
#define TCP_BRIDGE_START             "TCP_BRIDGE_START"
#define TCP_BRIDGE_OK                "TCP_BRIDGE_OK"
#define TCP_CLIENT_CONNECTED         "TCP_CLIENT_CONNECTED"
#define TCP_CLIENT_DISCONNECTED      "TCP_CLIENT_DISCONNECTED"
#define TCP_BRIDGE_FAIL              "TCP_BRIDGE_FAIL"

// ===================================================================================
// Security Messages
// ===================================================================================
#define SEC_DEFAULT_PASSWORD         "SEC_DEFAULT_PASSWORD"
#define SEC_DISABLED                 "SEC_DISABLED"
#define SEC_AUTH_LOCKOUT             "SEC_AUTH_LOCKOUT"
#define SEC_PASSWORD_CHANGED         "SEC_PASSWORD_CHANGED"
#define SEC_SESSION_EXPIRED          "SEC_SESSION_EXPIRED"

// ===================================================================================
// Firmware Manager Messages
// ===================================================================================
#define FW_CONVERT_START             "FW_CONVERT_START"
#define FW_CONVERT_OK                "FW_CONVERT_OK"
#define FW_PARSE_ERROR               "FW_PARSE_ERROR"
#define FW_FLASH_OK                  "FW_FLASH_OK"
#define FW_FLASH_FAIL                "FW_FLASH_FAIL"
#define FW_CRC_ERROR                 "FW_CRC_ERROR"

// ===================================================================================
// ADC/Battery Messages
// ===================================================================================
#define ADC_BATTERY_INIT             "ADC_BATTERY_INIT"
#define ADC_BATTERY_LOW              "ADC_BATTERY_LOW"
#define ADC_BATTERY_CRITICAL         "ADC_BATTERY_CRITICAL"
#define ADC_BATTERY_CHARGING         "ADC_BATTERY_CHARGING"

// ===================================================================================
// System Messages
// ===================================================================================
#define SYS_WATCHDOG_RESET           "SYS_WATCHDOG_RESET"
#define SYS_BROWNOUT_RESET           "SYS_BROWNOUT_RESET"
#define SYS_HEAP_LOW                 "SYS_HEAP_LOW"
#define SYS_HEAP_CRITICAL            "SYS_HEAP_CRITICAL"
#define SYS_TASK_CRASH               "SYS_TASK_CRASH"
#define SYS_STACK_OVERFLOW           "SYS_STACK_OVERFLOW"

// ===================================================================================
// UI Messages
// ===================================================================================
#define UI_INIT_BEGIN                "UI_INIT_BEGIN"
#define UI_INIT_OK                   "UI_INIT_OK"
#define UI_INIT_FAIL                 "UI_INIT_FAIL"
#define UI_SCREEN_FAIL               "UI_SCREEN_FAIL"
#define UI_SPLASH_FAIL               "UI_SPLASH_FAIL"
#define UI_WIZARD_START              "UI_WIZARD_START"
#define UI_WIZARD_COMPLETE           "UI_WIZARD_COMPLETE"

// ===================================================================================
// Test Messages (for gauntlet tests)
// ===================================================================================
#define TEST_INFO                    "TEST_INFO"
#define TEST_WARN                    "TEST_WARN"
#define TEST_ERROR                   "TEST_ERROR"
#define TEST_COALESCE                "TEST_COALESCE"
#define TEST_RELATCH                 "TEST_RELATCH"

#endif // MESSAGE_CODES_H
