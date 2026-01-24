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

// Services
#include "../services/web_server/api_handlers.h"
#include "../services/web_server/html_templates.h"
#include "../services/serialwombat/serialwombat_manager.h"

// UI
#include "../ui/lvgl_wrapper.h"
#include "../ui/screens/setup_wizard.h"

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
extern bool isSDEnabled;                // Defined in main .ino

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
App::App() : server(80), tcpServer(TCP_PORT) {
}

// ===================================================================================
// Application Lifecycle - Initialization
// ===================================================================================
void App::begin() {
  initSerial();
  initFileSystem();
  initConfiguration();
  initHardware();
  initDisplay();
  initNetwork();
  initWebServer();
  initOTA();
}

// ===================================================================================
// Initialization Phases
// ===================================================================================

void App::initSerial() {
  Serial.begin(115200);
}

void App::initFileSystem() {
  // LittleFS (ESP32) - auto format on first mount failure
  if (!LittleFS.begin(true)) {
    LittleFS.format();
    LittleFS.begin(true);
  }

  // Ensure storage folders exist
  if (!LittleFS.exists(FW_DIR)) LittleFS.mkdir(FW_DIR);
  if (!LittleFS.exists(CFG_DIR)) LittleFS.mkdir(CFG_DIR);
  if (!LittleFS.exists("/temp")) LittleFS.mkdir("/temp");
  if (!LittleFS.exists("/hexcache")) LittleFS.mkdir("/hexcache");
}

void App::initConfiguration() {
  // Load (or create) CYD runtime config
  if (!loadConfig(g_cfg)) {
    setConfigDefaults(g_cfg);
    // first boot (not configured yet)
    g_cfg.configured = false;
    saveConfig(g_cfg);
  }
}

void App::initHardware() {
  // Keep WiFi responsive during long flash operations
  WiFi.setSleep(false);

  // Apply dynamic I2C pins from config *before* Wire.begin
  Wire.begin(g_cfg.i2c_sda, g_cfg.i2c_scl);
  Wire.setClock(100000);

  sw.begin(Wire, currentWombatAddress);
}

void App::initDisplay() {
#if DISPLAY_SUPPORT_ENABLED
  // Initialize local display stack (LovyanGFX + LVGL) if enabled
  if (g_cfg.display_enable && g_cfg.lvgl_enable && !g_cfg.headless) {
    if (lvglInitIfEnabled()) {
      if (!g_cfg.configured) {
        firstBootShowModelSelect();
      }
    }
  }
#endif
}

void App::initNetwork() {
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);
  if (!wm.autoConnect("Wombat-Setup")) {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("Wombat-Setup");
  }
  
  // Security warning on startup
  #if SECURITY_ENABLED
  Serial.println("\n*** SECURITY WARNING ***");
  Serial.println("Authentication is ENABLED");
  Serial.print("Username: ");
  Serial.println(AUTH_USERNAME);
  if (strcmp(AUTH_PASSWORD, "CHANGE_ME_NOW") == 0) {
    Serial.println("*** DEFAULT PASSWORD DETECTED ***");
    Serial.println("*** CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY ***");
    Serial.println("*** SYSTEM IS NOT SECURE WITH DEFAULT PASSWORD ***");
  }
  Serial.println("************************\n");
  #else
  Serial.println("\n*** WARNING: SECURITY DISABLED ***\n");
  #endif
}

void App::initWebServer() {
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
  tcpServer.begin();
}

void App::initOTA() {
  // Configure OTA security
  ArduinoOTA.setPassword(AUTH_PASSWORD);
  ArduinoOTA.setHostname("wombat-bridge");
  
  // OTA security callbacks
  ArduinoOTA.onStart([]() {
    String type = (ArduinoOTA.getCommand() == U_FLASH) ? "firmware" : "filesystem";
    Serial.println("OTA Update Start: " + type);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
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
  handleTcpBridge();
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
