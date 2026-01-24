/*
 * Application Orchestrator - Implementation
 * 
 * Manages the overall application lifecycle, initialization, and coordination
 * between services, HAL, and UI components.
 * 
 * Extracted from original setup() and loop() functions in main .ino file.
 */

#include "app.h"

// Standard Arduino/ESP32 libraries
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <ArduinoOTA.h>
#include <LittleFS.h>

// Configuration
#include "../config/config_manager.h"
#include "../config/system_config.h"
#include "../config/defaults.h"

// Core
#include "../core/globals.h"
#include "../core/messages/message_center.h"
#include "../core/messages/boot_manager.h"
#include "../core/messages/message_codes.h"

// Services
#include "../services/web_server/api_handlers.h"
#include "../services/web_server/html_templates.h"
#include "../services/serialwombat/serialwombat_manager.h"
#include "../services/tcp_bridge/tcp_bridge.h"

// HAL
#include "../hal/storage/sd_storage.h"

// UI
#include "../ui/lvgl_wrapper.h"
#include "../ui/screens/setup_wizard.h"

// Time sync
#include <time.h>
#include <sys/time.h>

// ===================================================================================
// Security Configuration (from main .ino file)
// ===================================================================================
#define SECURITY_ENABLED 1
#define AUTH_USERNAME "admin"
#define AUTH_PASSWORD "CHANGE_ME_NOW"

// Display support (from main .ino file)
#ifndef DISPLAY_SUPPORT_ENABLED
#define DISPLAY_SUPPORT_ENABLED 1
#endif

// SD support (from main .ino file)
#ifndef SD_SUPPORT_ENABLED
#define SD_SUPPORT_ENABLED 1
#endif

// ===================================================================================
// External Global Variables
// ===================================================================================
// These are defined in other modules and referenced here
extern SystemConfig g_cfg;              // Defined in config_manager.cpp
extern SerialWombat sw;                 // Defined in serialwombat_manager.cpp
extern uint8_t currentWombatAddress;    // Defined in serialwombat_manager.cpp
extern bool g_lvgl_ready;               // Defined in lvgl_wrapper.cpp
extern bool g_fwUploadOk;               // Defined in api_handlers.cpp
extern String g_fwUploadMsg;            // Defined in api_handlers.cpp
// isSDEnabled is now defined in core/globals.cpp

// Directory constants
static const char* FW_DIR = "/fw";
static const char* CFG_DIR = "/config";

// TCP port for SerialWombat bridge
#define TCP_PORT 3000

// ===================================================================================
// Singleton Implementation
// ===================================================================================
App& App::getInstance() {
  static App instance;
  return instance;
}

// Private constructor
App::App() : server(80), tcpServer(TCP_PORT), tcpClient() {
}

// ===================================================================================
// Application Lifecycle - Initialization
// ===================================================================================
void App::begin() {
  // Initialize MessageCenter first (before any messages can be posted)
  MessageCenter::getInstance().begin();
  
  // Initialize BootManager
  BootManager::getInstance().begin();
  
  // Run initialization phases with boot stage tracking
  initSerial();
  initFileSystem();
  initConfiguration();
  initHardware();
  initSD();
  initDisplay();
  initTouch();
  initNetwork();
  initTimeSync();
  initWebServer();
  initOTA();
  
  // Mark boot complete
  BootManager::getInstance().bootComplete();
}

// ===================================================================================
// Initialization Phases
// ===================================================================================

void App::initSerial() {
  boot_stage_begin(BootStage::BOOT_01_EARLY, "Early Initialization");
  Serial.begin(115200);
  delay(100);  // Allow serial to stabilize
  boot_stage_ok(BootStage::BOOT_01_EARLY, "Serial console ready @ 115200 baud");
}

void App::initFileSystem() {
  boot_stage_begin(BootStage::BOOT_03_FILESYSTEM, "Filesystem Mount");
  
  // LittleFS (ESP32) - auto format on first mount failure
  if (!LittleFS.begin(true)) {
    msg_warn("fs", FS_LFS_FORMAT_BEGIN, "Formatting Filesystem", 
             "Auto-formatting LittleFS (first boot or corrupted)");
    LittleFS.format();
    if (!LittleFS.begin(true)) {
      boot_stage_fail(BootStage::BOOT_03_FILESYSTEM, "LittleFS mount and format failed");
      return;  // Critical failure, but continue in RAM-only mode
    }
    msg_info("fs", FS_LFS_FORMAT_OK, "Filesystem Formatted", "LittleFS ready");
  }

  boot_stage_ok(BootStage::BOOT_03_FILESYSTEM, "LittleFS mounted successfully");

  // Ensure storage folders exist
  if (!LittleFS.exists(FW_DIR)) LittleFS.mkdir(FW_DIR);
  if (!LittleFS.exists(CFG_DIR)) LittleFS.mkdir(CFG_DIR);
  if (!LittleFS.exists("/temp")) LittleFS.mkdir("/temp");
  if (!LittleFS.exists("/hexcache")) LittleFS.mkdir("/hexcache");
}

void App::initConfiguration() {
  boot_stage_begin(BootStage::BOOT_02_CONFIG, "Configuration Load");
  
  // Load (or create) CYD runtime config
  if (!loadConfig(g_cfg)) {
    setConfigDefaults(g_cfg);
    // first boot (not configured yet)
    g_cfg.configured = false;
    saveConfig(g_cfg);
    boot_stage_warn(BootStage::BOOT_02_CONFIG, "Config file missing, defaults applied");
  } else {
    boot_stage_ok(BootStage::BOOT_02_CONFIG, "Configuration loaded successfully");
  }
}

void App::initHardware() {
  // Keep WiFi responsive during long flash operations
  WiFi.setSleep(false);

  // Apply dynamic I2C pins from config *before* Wire.begin
  msg_info("i2c", I2C_INIT_BEGIN, "I2C Initialization", 
           "Configuring I2C bus: SDA=%d, SCL=%d", g_cfg.i2c_sda, g_cfg.i2c_scl);
  
  Wire.begin(g_cfg.i2c_sda, g_cfg.i2c_scl);
  Wire.setClock(100000);
  
  msg_info("i2c", I2C_BUS_OK, "I2C Bus Ready", 
           "I2C initialized at 100kHz");

  // Initialize SerialWombat
  msg_info("serialwombat", SW_INIT_BEGIN, "SerialWombat Initialization", 
           "Initializing SerialWombat at address 0x%02X", currentWombatAddress);
  
  sw.begin(Wire, currentWombatAddress);
  
  msg_info("serialwombat", SW_INIT_OK, "SerialWombat Ready", 
           "SerialWombat initialized successfully");
}

void App::initSD() {
#if SD_SUPPORT_ENABLED
  boot_stage_begin(BootStage::BOOT_04_SD, "SD Card Detection");
  
  if (SD_SUPPORT_ENABLED && g_cfg.sd_enable) {
    if (sdMount()) {
      boot_stage_ok(BootStage::BOOT_04_SD, "SD card mounted successfully");
      msg_info("sd", SD_MOUNT_OK, "SD Card Ready", 
               "SD card detected and mounted");
    } else {
      boot_stage_warn(BootStage::BOOT_04_SD, "No SD card detected, continuing without SD");
      msg_warn("sd", SD_NOT_PRESENT, "SD Card Not Present", 
               "No SD card detected. Insert SD card for additional storage");
    }
  } else {
    boot_stage_warn(BootStage::BOOT_04_SD, "SD support disabled in configuration");
  }
#else
  boot_stage_warn(BootStage::BOOT_04_SD, "SD support not compiled");
#endif
}

void App::initDisplay() {
#if DISPLAY_SUPPORT_ENABLED
  boot_stage_begin(BootStage::BOOT_05_DISPLAY, "Display Initialization");
  
  // Initialize local display stack (LovyanGFX + LVGL) if enabled
  if (g_cfg.display_enable && g_cfg.lvgl_enable && !g_cfg.headless) {
    if (lvglInitIfEnabled()) {
      boot_stage_ok(BootStage::BOOT_05_DISPLAY, "Display ready");
      if (!g_cfg.configured) {
        firstBootShowModelSelect();
      }
    } else {
      boot_stage_fail(BootStage::BOOT_05_DISPLAY, "Display initialization failed");
    }
  } else {
    boot_stage_warn(BootStage::BOOT_05_DISPLAY, "Display disabled (headless mode)");
  }
#else
  boot_stage_warn(BootStage::BOOT_05_DISPLAY, "Display support not compiled");
#endif
}

void App::initTouch() {
#if DISPLAY_SUPPORT_ENABLED
  boot_stage_begin(BootStage::BOOT_06_TOUCH, "Touch Controller Initialization");
  
  if (g_cfg.display_enable && g_cfg.touch_enable && g_lvgl_ready) {
    // Touch is initialized as part of LovyanGFX/LVGL setup
    // The touch driver is part of the display initialization
    if (g_cfg.touch != TOUCH_NONE) {
      boot_stage_ok(BootStage::BOOT_06_TOUCH, "Touch controller initialized");
      msg_info("touch", TOUCH_INIT_OK, "Touch Initialized", "Touch controller ready for input");
    } else {
      boot_stage_warn(BootStage::BOOT_06_TOUCH, "Touch type not configured");
      msg_warn("touch", TOUCH_CAL_REQUIRED, "Touch Not Configured", 
               "Touch controller type not set. Configure in setup wizard");
    }
  } else {
    boot_stage_warn(BootStage::BOOT_06_TOUCH, "Touch disabled or display not available");
  }
#else
  boot_stage_warn(BootStage::BOOT_06_TOUCH, "Display/Touch support not compiled");
#endif
}

void App::initNetwork() {
  boot_stage_begin(BootStage::BOOT_07_NETWORK, "Network Initialization");
  
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Wombat-Setup")) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Wombat-Setup");
    boot_stage_warn(BootStage::BOOT_07_NETWORK, "WiFi failed, AP mode active: 'Wombat-Setup'");
  } else {
    boot_stage_ok(BootStage::BOOT_07_NETWORK, 
                  "WiFi connected: %s (IP: %s)", 
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  }
  
  // Security warning on startup
  #if SECURITY_ENABLED
  if (strcmp(AUTH_PASSWORD, "CHANGE_ME_NOW") == 0) {
    msg_error("security", SEC_DEFAULT_PASSWORD, "Default Password Detected", 
              "CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY - System is NOT secure");
  }
  #else
  msg_warn("security", SEC_DISABLED, "Security Disabled", 
           "Authentication is DISABLED - Enable for production use");
  #endif
}

void App::initTimeSync() {
  boot_stage_begin(BootStage::BOOT_08_TIME, "Time Synchronization");
  
  // Only attempt NTP sync if WiFi is connected (not in AP mode)
  if (WiFi.getMode() == WIFI_MODE_STA && WiFi.status() == WL_CONNECTED) {
    // Configure NTP with multiple servers for redundancy
    configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    
    // Wait up to 5 seconds for time sync
    int retry = 0;
    const int maxRetries = 10;  // 10 * 500ms = 5 seconds max
    struct tm timeinfo;
    
    while (retry < maxRetries) {
      if (getLocalTime(&timeinfo, 500)) {
        // Time sync successful
        char timeStr[64];
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
        boot_stage_ok(BootStage::BOOT_08_TIME, "Time synchronized: %s UTC", timeStr);
        msg_info("time", BOOT_08_TIME_OK, "Time Synchronized", 
                 "System time set via NTP: %s UTC", timeStr);
        return;
      }
      retry++;
      delay(500);
    }
    
    // Time sync failed but not critical
    boot_stage_warn(BootStage::BOOT_08_TIME, "NTP sync failed, continuing with system time");
    msg_warn("time", BOOT_08_TIME_FAIL, "Time Sync Failed", 
             "Could not synchronize time with NTP servers. Timestamps may be incorrect");
  } else {
    // No internet connection (AP mode), skip NTP
    boot_stage_warn(BootStage::BOOT_08_TIME, "No internet connection, skipping NTP sync");
    msg_warn("time", BOOT_08_TIME_FAIL, "Time Sync Skipped", 
             "No internet connection available for NTP synchronization");
  }
}

void App::initWebServer() {
  boot_stage_begin(BootStage::BOOT_09_SERVICES, "Services Initialization");
  
  // ===================================================================================
  // Dashboard + Tools
  // ===================================================================================
  server.on("/", []() { handleRoot(App::getInstance().getWebServer()); });
  server.on("/scanner", []() { handleScanner(App::getInstance().getWebServer()); });
  server.on("/scan-data", []() { handleScanData(App::getInstance().getWebServer()); });
  server.on("/deepscan", []() { handleDeepScan(App::getInstance().getWebServer()); });
  server.on("/connect", []() { handleConnect(App::getInstance().getWebServer()); });
  server.on("/setpin", []() { handleSetPin(App::getInstance().getWebServer()); });
  server.on("/changeaddr", []() { handleChangeAddr(App::getInstance().getWebServer()); });
  server.on("/resetwifi", []() { handleResetWiFi(App::getInstance().getWebServer()); });
  server.on("/flashfw", HTTP_POST, []() { handleFlashFW(App::getInstance().getWebServer()); });
  server.on(
    "/upload_fw",
    HTTP_POST,
    []() {
      // ESP32 WebServer: send exactly ONE response from the POST handler.
      auto& srv = App::getInstance().getWebServer();
      if (g_fwUploadOk) {
        srv.send(200, "text/plain", "Saved.");
      } else {
        String msg = g_fwUploadMsg.length() ? g_fwUploadMsg : String("Upload failed");
        srv.send(500, "text/plain", msg);
      }
    },
    []() { handleUploadFW(App::getInstance().getWebServer()); }
  );

  server.on(
    "/upload_hex",
    HTTP_POST,
    []() { handleUploadHexPost(App::getInstance().getWebServer()); },
    []() { handleUploadHex(App::getInstance().getWebServer()); }
  );

  server.on("/clean_slot", []() { handleCleanSlot(App::getInstance().getWebServer()); });
  server.on("/resetwombat", []() { handleResetTarget(App::getInstance().getWebServer()); });
  server.on("/formatfs", []() { handleFormat(App::getInstance().getWebServer()); });

  // ===================================================================================
  // Messages UI and API
  // ===================================================================================
  server.on("/messages", []() { handleMessagesPage(App::getInstance().getWebServer()); });
  server.on("/api/messages/summary", HTTP_GET, []() { 
    handleApiMessagesSummary(App::getInstance().getWebServer()); 
  });
  server.on("/api/messages/active", HTTP_GET, []() { 
    handleApiMessagesActive(App::getInstance().getWebServer()); 
  });
  server.on("/api/messages/history", HTTP_GET, []() { 
    handleApiMessagesHistory(App::getInstance().getWebServer()); 
  });
  server.on("/api/messages/ack", HTTP_POST, []() { 
    handleApiMessagesAck(App::getInstance().getWebServer()); 
  });
  server.on("/api/messages/ack_all", HTTP_POST, []() { 
    handleApiMessagesAckAll(App::getInstance().getWebServer()); 
  });
  server.on("/api/messages/clear_history", HTTP_POST, []() { 
    handleApiMessagesClearHistory(App::getInstance().getWebServer()); 
  });

  // ===================================================================================
  // Test/Debug API
  // ===================================================================================
  server.on("/api/test/gauntlet", HTTP_GET, []() { 
    handleApiTestGauntlet(App::getInstance().getWebServer()); 
  });

  // ===================================================================================
  // Configurator UI
  // ===================================================================================
  server.on("/configure", []() { 
    App::getInstance().getWebServer().send(200, "text/html", FPSTR(CONFIG_HTML)); 
  });

  // ===================================================================================
  // System Settings UI
  // ===================================================================================
  server.on("/settings", []() { 
    App::getInstance().getWebServer().send(200, "text/html", FPSTR(SETTINGS_HTML)); 
  });

  // ===================================================================================
  // Health check endpoint (public, no auth)
  // ===================================================================================
  server.on("/api/health", HTTP_GET, []() { 
    handleApiHealth(App::getInstance().getWebServer()); 
  });
  
  // ===================================================================================
  // System Settings API
  // ===================================================================================
  server.on("/api/system", HTTP_GET, []() { 
    handleApiSystem(App::getInstance().getWebServer()); 
  });

#if SD_SUPPORT_ENABLED
  // ===================================================================================
  // SD Card Manager API (only registered when enabled)
  // ===================================================================================
  if (isSDEnabled) {
    server.on("/api/sd/status", HTTP_GET, []() { 
      handleApiSdStatus(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/list", HTTP_GET, []() { 
      handleApiSdList(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/delete", HTTP_POST, []() { 
      handleApiSdDelete(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/rename", HTTP_POST, []() { 
      handleApiSdRename(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/eject", HTTP_POST, []() { 
      handleApiSdEject(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/import_fw", HTTP_POST, []() { 
      handleApiSdImportFw(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/convert_fw", HTTP_POST, []() { 
      handleApiSdImportFw(App::getInstance().getWebServer()); 
    });
    server.on("/sd/download", HTTP_GET, []() { 
      handleSdDownload(App::getInstance().getWebServer()); 
    });
    server.on("/api/sd/upload", HTTP_POST, 
      []() { handleUploadSdPost(App::getInstance().getWebServer()); }, 
      []() { handleUploadSD(App::getInstance().getWebServer()); }
    );
  }
#endif

  // ===================================================================================
  // Configurator API
  // ===================================================================================
  server.on("/api/variant", HTTP_GET, []() { 
    handleApiVariant(App::getInstance().getWebServer()); 
  });
  server.on("/api/apply", HTTP_POST, []() { 
    handleApiApply(App::getInstance().getWebServer()); 
  });
  server.on("/api/config/save", HTTP_POST, []() { 
    handleConfigSave(App::getInstance().getWebServer()); 
  });
  server.on("/api/config/load", HTTP_GET, []() { 
    handleConfigLoad(App::getInstance().getWebServer()); 
  });
  server.on("/api/config/list", HTTP_GET, []() { 
    handleConfigList(App::getInstance().getWebServer()); 
  });
  server.on("/api/config/exists", HTTP_GET, []() { 
    handleConfigExists(App::getInstance().getWebServer()); 
  });
  server.on("/api/config/delete", HTTP_GET, []() { 
    handleConfigDelete(App::getInstance().getWebServer()); 
  });

  // Start web server and TCP server
  server.begin();
  msg_info("web", WEB_SERVER_START, "Web Server Started", "HTTP server listening on port 80");
  
  tcpServer.begin();
  msg_info("tcp", TCP_BRIDGE_START, "TCP Bridge Started", "TCP bridge listening on port %d", TCP_PORT);
  
  boot_stage_ok(BootStage::BOOT_09_SERVICES, "Web server (port 80) and TCP bridge (port %d) started", TCP_PORT);
}

void App::initOTA() {
  // Note: OTA init doesn't have a separate boot stage; it's part of BOOT_09_SERVICES
  
  // Configure OTA security
  ArduinoOTA.setPassword(AUTH_PASSWORD);
  ArduinoOTA.setHostname("wombat-bridge");
  
  // OTA security callbacks
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    msg_info("ota", OTA_UPDATE_START, "OTA Update Started", "Type: %s", type.c_str());
  });
  
  ArduinoOTA.onEnd([]() {
    msg_info("ota", OTA_UPDATE_OK, "OTA Update Complete", "Rebooting...");
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    const char* err_msg = "Unknown error";
    if (error == OTA_AUTH_ERROR) err_msg = "Authentication Failed";
    else if (error == OTA_BEGIN_ERROR) err_msg = "Begin Failed";
    else if (error == OTA_CONNECT_ERROR) err_msg = "Connect Failed";
    else if (error == OTA_RECEIVE_ERROR) err_msg = "Receive Failed";
    else if (error == OTA_END_ERROR) err_msg = "End Failed";
    
    msg_error("ota", OTA_UPDATE_FAIL, "OTA Update Failed", err_msg);
  });
  
  ArduinoOTA.begin();
}

// ===================================================================================
// Application Lifecycle - Runtime Loop
// ===================================================================================
void App::update() {
  updateOTA();
  updateWebServer();
  updateTCPBridge();
  updateDisplay();
}

// ===================================================================================
// Runtime Update Phases
// ===================================================================================

void App::updateOTA() {
  ArduinoOTA.handle();
}

void App::updateWebServer() {
  server.handleClient();
}

void App::updateTCPBridge() {
  handleTcpBridge(tcpServer, tcpClient, currentWombatAddress);
}

void App::updateDisplay() {
#if DISPLAY_SUPPORT_ENABLED
  if (g_lvgl_ready) {
    static uint32_t last = millis();
    uint32_t now = millis();
    uint32_t dt = now - last;
    last = now;
#if LVGL_VERSION_MAJOR >= 9
    lv_tick_set(lv_tick_get() + dt);
#else
  #if defined(LVGL_VERSION_MAJOR) && (LVGL_VERSION_MAJOR < 9)
    // LVGL v8 uses a software tick increment.
    lv_tick_inc(dt);
  #endif
#endif
    lvglTickAndUpdate();
  }
#endif
}
