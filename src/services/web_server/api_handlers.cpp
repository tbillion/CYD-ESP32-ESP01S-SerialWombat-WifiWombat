#include "api_handlers.h"

#include <ArduinoOTA.h>
#include <LittleFS.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "../../config/config_manager.h"
#include "../../core/messages/boot_manager.h"
#include "../../core/messages/health_snapshot.h"
#include "../../core/messages/message_center.h"
#include "../i2c_manager/i2c_manager.h"
#include "../security/auth_service.h"
#include "../security/validators.h"
#include "../serialwombat/serialwombat_manager.h"
#include "html_templates.h"

// External global variables
extern SerialWombat sw;
extern uint8_t currentWombatAddress;
extern WebServer server;
extern WiFiServer tcpServer;
extern WiFiClient tcpClient;
extern SystemConfig g_cfg;
extern bool isSDEnabled;

// External constants
extern const char* FW_DIR;
extern const char* CFG_DIR;
extern const char* TEMP_DIR;

// External upload state
extern bool g_hexUploadOk;
extern String g_hexUploadMsg;
extern String g_hexUploadPath;
extern File g_hexUploadFile;
extern IntelHexSW8B g_hexConv;
extern bool g_fwUploadOk;
extern String g_fwUploadMsg;
extern String g_fwUploadPath;
extern File g_fwUploadFile;

#if SD_SUPPORT_ENABLED
extern bool g_sdMounted;
extern String g_sdMountMsg;
extern bool g_sdUploadOk;
extern String g_sdUploadMsg;
extern String g_sdUploadPath;
extern SDFile g_sdUploadFile;

// SD helper functions (external)
extern bool sdEnsureMounted();
extern void sdUnmount();
extern SDFile sd_open(const char* path, oflag_t flags);
extern bool sd_rename(const String& from, const String& to);
extern uint64_t sd_filesize(SDFile& f);
extern bool sdRemoveRecursive(const String& path);
extern String sdFileName(SDFile& f);
extern bool sdFileIsDir(SDFile& f);
extern bool sdOpenNext(SDFile& dir, SDFile& out);
extern uint64_t sd_total_bytes();
extern uint64_t sd_used_bytes();
extern bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path);
#endif

// Helper functions (external)
extern String sanitizeBasename(const String& name);
extern String jsonEscape(const String& s);
extern String joinPath(const String& dir, const String& base);
extern String normalizePath(String p);
extern void fsListFilesBySuffix(const char* suffix, String& outOptionsHtml, bool& foundAny);
extern bool convertFwTxtToBin(const char* fwTxtPath, const char* outBinPath, String& err);
extern bool convertHexToFirmwareBin(const String& tempHexPath, const String& outBinPath,
                                    String& outWarnOrErr);
extern bool ensureDir(const char* path);
extern String makeFileSafeName(const String& in);
extern String sanitizePath(const String& raw);
extern String ensureTempPathForUpload(const char* leafName);
extern String fwSlotPath(const String& prefix, const String& version);
extern void fsCleanSlot(const String& prefix);
extern bool fwTxtToBin(const String& inPath, const String& outBinPath, String& err, bool fromSD);

// ===================================================================================
// ROOT AND MAIN HANDLERS
// ===================================================================================
void handleRoot(WebServer& server) {
  // Public page but add security headers
  addSecurityHeaders(server);

  String s = FPSTR(INDEX_HTML_HEAD);

  // Insert a simple nav bar without altering the stored v06 HTML constants.
  // (Served HTML gains a 2-tab bar; original Dashboard content remains intact.)
  const String nav = "<div style='background:#333;padding:10px;margin:0 -10px 10px "
                     "-10px;border-bottom:1px solid #444;'>"
                     "<a href='/' style='color:white;font-weight:bold;margin:0 "
                     "10px;text-decoration:none;border-bottom:2px solid white;'>Dashboard</a>"
                     "<a href='/configure' style='color:#00d2ff;font-weight:bold;margin:0 "
                     "10px;text-decoration:none;'>Configurator</a><a href='/settings' "
                     "style='color:#00d2ff;font-weight:bold;margin:0 "
                     "10px;text-decoration:none;'>System Settings</a>"
                     "</div>";
  s.replace("<body>", "<body>" + nav);

  String addrHex = String(currentWombatAddress, HEX);
  addrHex.toUpperCase();
  s.replace("%ADDR%", addrHex);
  s.replace("%IP%", WiFi.localIP().toString());

  String options;
  bool found = false;
  fsListFilesBySuffix(".bin", options, found);
  if (!found) options = "<option value=''>No Firmwares Found (Use Manager)</option>";

  s += options;
  s += FPSTR(INDEX_HTML_TAIL);

  // Conditional SD UI injection (zero-clabber: placeholders already exist in HTML templates)
  s.replace("%SD_TILE%", isSDEnabled ? String(FPSTR(SD_TILE_HTML)) : String(""));
  s.replace("%SD_FW_OPTION%", isSDEnabled ? String(FPSTR(SD_FW_OPTION_HTML)) : String(""));
  s.replace("%SD_FW_AREA%", isSDEnabled ? String(FPSTR(SD_FW_AREA_HTML)) : String(""));

  server.send(200, "text/html", s);
}

void handleScanner(WebServer& server) {
  server.send(200, "text/html", FPSTR(SCANNER_HTML));
}

// ===================================================================================
// WIFI AND SYSTEM HANDLERS
// ===================================================================================
void handleResetWiFi(WebServer& server) {
  // Authentication required for WiFi reset (critical operation)
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void handleFormat(WebServer& server) {
  // Authentication required for filesystem format (destructive operation)
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  LittleFS.format();
  server.sendHeader("Location", "/");
  server.send(303);
}

// ===================================================================================
// FIRMWARE MANAGEMENT HANDLERS
// ===================================================================================
void handleCleanSlot(WebServer& server) {
  // Authentication required for slot cleanup
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("prefix")) {
    server.send(400, "text/plain", "Missing prefix");
    return;
  }

  const String prefix = server.arg("prefix");
  // Collect candidates first (safer than deleting while iterating)
  String toDelete[64];
  int delCount = 0;

  auto collectInDir = [&](const char* dirPath) {
    File dir = LittleFS.open(dirPath);
    if (!dir || !dir.isDirectory()) return;
    File f = dir.openNextFile();
    while (f) {
      String n = String(f.name());
      String base = n;
      // base name for matching
      int slash = base.lastIndexOf('/');
      if (slash >= 0) base = base.substring(slash + 1);

      if (base.startsWith(prefix + "_") && base.endsWith(".bin")) {
        if (delCount < 64) toDelete[delCount++] = n;
      }
      f.close();
      f = dir.openNextFile();
    }
    dir.close();
  };

  collectInDir(FW_DIR);
  // Back-compat: legacy root
  collectInDir("/");

  int removed = 0;
  for (int i = 0; i < delCount; i++) {
    if (LittleFS.remove(toDelete[i])) removed++;
  }

  server.send(200, "text/plain", "Cleaned " + String(removed));
}

void handleUploadHex(WebServer& server) {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_hexUploadOk = false;
    g_hexUploadMsg = "";
    g_hexUploadPath = "";

    if (g_hexUploadFile) g_hexUploadFile.close();

    ensureDir("/temp");

    // Save to /temp with sanitized basename
    String safeName = sanitizeBasename(upload.filename);
    if (!safeName.endsWith(".hex")) {
      // still allow; keep name
    }
    g_hexUploadPath = joinPath("/temp", safeName);

    g_hexUploadFile = LittleFS.open(g_hexUploadPath, "w");
    if (!g_hexUploadFile) {
      g_hexUploadMsg = "Open failed: " + g_hexUploadPath;
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_hexUploadFile) return;
    size_t w = g_hexUploadFile.write(upload.buf, upload.currentSize);
    if (w != upload.currentSize) {
      g_hexUploadMsg = "Write failed";
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_hexUploadFile) g_hexUploadFile.close();
    if (g_hexUploadMsg.length() == 0) {
      g_hexUploadOk = true;
      g_hexUploadMsg = "Saved temp: " + g_hexUploadPath + " (" + String(upload.totalSize) + ")";
    } else {
      if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (g_hexUploadFile) g_hexUploadFile.close();
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    g_hexUploadOk = false;
    g_hexUploadMsg = "Upload aborted";
  }
}

void handleUploadHexPost(WebServer& server) {
  // Respond exactly once from the POST handler
  if (!g_hexUploadOk) {
    String msg = g_hexUploadMsg.length() ? g_hexUploadMsg : String("Upload failed");
    server.send(500, "text/plain", msg);
    return;
  }

  if (!server.hasArg("prefix") || !server.hasArg("ver")) {
    // cleanup temp file
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    server.send(400, "text/plain", "Missing prefix/ver");
    return;
  }

  String prefix = server.arg("prefix");
  String ver = server.arg("ver");
  prefix = sanitizeBasename(prefix);
  ver.trim();

  if (prefix.length() == 0 || ver.length() == 0) {
    if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);
    server.send(400, "text/plain", "Bad prefix/ver");
    return;
  }

  // Final output name matches Blob workflow: <slot>_<ver>.bin in /fw
  String finalName = prefix + "_" + ver + ".bin";
  String outPath = joinPath(FW_DIR, finalName);

  // Convert
  String warnOrErr;
  bool ok = convertHexToFirmwareBin(g_hexUploadPath, outPath, warnOrErr);

  // Always delete temp hex immediately
  if (g_hexUploadPath.length()) LittleFS.remove(g_hexUploadPath);

  if (!ok) {
    // remove any partial output
    LittleFS.remove(outPath);
    String msg = warnOrErr.length() ? warnOrErr : String("Conversion failed");
    server.send(500, "text/plain", msg);
    return;
  }

  String msg = "Converted & saved: " + outPath;
  if (warnOrErr.length()) {
    // Keep it short but useful
    msg += "\nWarnings:\n";
    msg += warnOrErr;
  }
  server.send(200, "text/plain", msg);
}

void handleUploadFW(WebServer& server) {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    g_fwUploadOk = false;
    g_fwUploadMsg = "";
    g_fwUploadPath = "";

    if (g_fwUploadFile) g_fwUploadFile.close();

    // Force firmware blobs into /fw
    String safeName = sanitizeBasename(upload.filename);
    g_fwUploadPath = joinPath(FW_DIR, safeName);

    // Ensure directory exists
    if (!LittleFS.exists(FW_DIR)) {
      LittleFS.mkdir(FW_DIR);
    }

    g_fwUploadFile = LittleFS.open(g_fwUploadPath, "w");
    if (!g_fwUploadFile) {
      g_fwUploadMsg = "Open failed: " + g_fwUploadPath;
      return;
    }

  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (!g_fwUploadFile) return;
    size_t w = g_fwUploadFile.write(upload.buf, upload.currentSize);
    if (w != upload.currentSize) {
      g_fwUploadMsg = "Write failed";
    }

  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_fwUploadFile) g_fwUploadFile.close();
    if (g_fwUploadMsg.length() == 0) {
      g_fwUploadOk = true;
      g_fwUploadMsg = "Saved: " + g_fwUploadPath + " (" + String(upload.totalSize) + ")";
    } else {
      // Remove partial
      if (g_fwUploadPath.length()) LittleFS.remove(g_fwUploadPath);
    }

  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (g_fwUploadFile) g_fwUploadFile.close();
    if (g_fwUploadPath.length()) LittleFS.remove(g_fwUploadPath);
    g_fwUploadOk = false;
    g_fwUploadMsg = "Upload aborted";
  }
}

void handleFlashFW(WebServer& server) {
  // Authentication required for firmware flashing (critical operation)
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("fw_name")) {
    server.send(400, "text/plain", "No selection");
    return;
  }

  // Be forgiving about what the UI posts. Depending on browser + HTML source,
  // this can be:
  //   - "Default_FW_2.2.2.bin" (basename)
  //   - "/fw/Default_FW_2.2.2.bin" (preferred)
  //   - "fw/Default_FW_2.2.2.bin" (no leading slash)
  //   - legacy root paths
  String raw = server.arg("fw_name");
  raw.trim();

  String fwName = normalizePath(raw);

  // If caller passed a basename, try /fw first, then legacy root.
  // If caller passed a path, still try a couple of normalizations.
  String resolved;
  auto tryPath = [&](const String& p) {
    if (resolved.length()) return;
    if (LittleFS.exists(p)) resolved = p;
  };

  // Direct as provided
  tryPath(fwName);

  // If it doesn't look like a directory path, try firmware dir
  if (!resolved.length()) {
    String base = raw;
    // strip any leading directory
    int lastSlash = base.lastIndexOf('/');
    if (lastSlash >= 0) base = base.substring(lastSlash + 1);
    base.trim();
    if (base.length()) {
      tryPath(joinPath(FW_DIR, base));
      tryPath("/" + base);  // legacy root
    }
  }

  // Final fallback: if it started with "//" from any weirdness, collapse it.
  if (!resolved.length() && fwName.startsWith("//")) {
    String collapsed = fwName;
    while (collapsed.startsWith("//"))
      collapsed.remove(0, 1);
    tryPath(collapsed);
  }

  if (!resolved.length()) {
    // Provide a helpful diagnostic list (kept short) so you can see what's actually on FS.
    String msg = "File missing. Requested='" + raw + "'\n";
    msg += "Checked: " + fwName + "\n";
    msg += "FW dir: " + String(FW_DIR) + "\n\n";
    msg += "Available firmwares:\n";
    File dir = LittleFS.open(FW_DIR);
    if (dir && dir.isDirectory()) {
      File f = dir.openNextFile();
      int shown = 0;
      while (f && shown < 30) {
        String n = String(f.name());
        if (n.endsWith(".bin")) {
          msg += " - " + n + " (" + String(f.size()) + ")\n";
          shown++;
        }
        f.close();
        f = dir.openNextFile();
      }
    } else {
      msg += " (cannot open /fw)\n";
    }
    server.send(400, "text/plain", msg);
    return;
  }

  fwName = resolved;

  File fwFile = LittleFS.open(fwName, "r");
  if (!fwFile) {
    server.send(500, "text/plain", "File Error");
    return;
  }

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(
      F("<!DOCTYPE HTML><html><head><meta http-equiv='refresh' "
        "content='10;url=/"
        "'><style>body{background:#000;color:#0f0;font-family:monospace;padding:20px;white-space:"
        "pre-wrap;}</style></head><body><h2>SW8B Firmware Update</h2>"));
  server.sendContent("Flashing: " + fwName + " (" + String(fwFile.size()) + " bytes)\n");

  sw.begin(Wire, currentWombatAddress, false);
  if (!sw.queryVersion()) server.sendContent("Connecting...\n");

  if (!sw.inBoot) {
    sw.jumpToBoot();
    sw.hardwareReset();
    delay(2000);
  }

  sw.begin(Wire, currentWombatAddress, false);
  if (!sw.queryVersion()) {
    server.sendContent("Error: Bootloader not found.\n");
    fwFile.close();
    return;
  }

  sw.eraseFlashPage(0);
  server.sendContent("Erasing...\n");

  uint32_t address = 0;
  uint8_t buffer[64];

  while (fwFile.available()) {
    yield();
    ArduinoOTA.handle();

    int bytesRead = fwFile.read(buffer, 64);
    if (bytesRead < 64) {
      for (int k = bytesRead; k < 64; k++)
        buffer[k] = 0xFF;
    }

    uint32_t page[16];
    bool dirty = false;
    for (int i = 0; i < 16; i++) {
      uint32_t val = (uint32_t)buffer[i * 4] + ((uint32_t)buffer[i * 4 + 1] << 8) +
                     ((uint32_t)buffer[i * 4 + 2] << 16) + ((uint32_t)buffer[i * 4 + 3] << 24);
      page[i] = val;
      if (val != 0xFFFFFFFF) dirty = true;
    }

    if (dirty) {
      sw.writeUserBuffer(0, (uint8_t*)page, 64);
      sw.writeFlashRow(address * 4 + 0x08000000);
      if (address % 128 == 0) server.sendContent("Writing addr: 0x" + String(address, HEX) + "\n");
      delay(10);
    }

    address += 16;
  }

  fwFile.close();

  uint8_t tx[] = {164, 4, 0, 0, 0, 0, 0, 0};
  sw.sendPacket(tx);
  delay(100);
  sw.hardwareReset();

  server.sendContent("\n<h3>SUCCESS! Redirecting...</h3></body></html>");
  server.sendContent("");

  delay(1000);
  sw.begin(Wire, currentWombatAddress);
}

// ===================================================================================
// TCP BRIDGE HANDLER
// ===================================================================================
void handleTcpBridge() {
  if (tcpServer.hasClient()) {
    if (!tcpClient || !tcpClient.connected()) {
      tcpClient = tcpServer.available();
      tcpClient.setNoDelay(true);
    } else {
      WiFiClient reject = tcpServer.available();
      reject.stop();
    }
  }

  if (tcpClient && tcpClient.connected()) {
    while (tcpClient.available() >= 8) {
      uint8_t txBuffer[8];
      uint8_t rxBuffer[8];

      tcpClient.read(txBuffer, 8);

      Wire.beginTransmission(currentWombatAddress);
      Wire.write(txBuffer, 8);
      Wire.endTransmission();
      i2cMarkTx();

      uint8_t bytesRead = Wire.requestFrom(currentWombatAddress, (uint8_t)8);
      i2cMarkRx();
      for (int i = 0; i < 8; i++) {
        if (i < bytesRead)
          rxBuffer[i] = Wire.read();
        else
          rxBuffer[i] = 0xFF;
      }

      tcpClient.write(rxBuffer, 8);
      ArduinoOTA.handle();
      yield();
    }
  }
}

// ===================================================================================
// CONFIG API HANDLERS (Configurator)
// ===================================================================================
static String configPathFromName(const String& name) {
  // New location: /config/<name>.json
  // Back-compat: legacy root files /config_<name>.json are still supported on load/list.
  String safe = sanitizeBasename(name);
  if (safe.length() == 0) safe = "config";
  return joinPath(CFG_DIR, safe + String(".json"));
}

void handleApiVariant(WebServer& server) {
  // Authentication required for variant info
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  VariantInfo info = getDeepScanInfoSingle(currentWombatAddress);
  DynamicJsonDocument doc(1536);
  doc["variant"] = info.variant;
  JsonArray capsArr = doc.createNestedArray("capabilities");
  for (int i = 0; i < 41; i++)
    if (info.caps[i]) capsArr.add(i);
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleApiApply(WebServer& server) {
  // Authentication required for configuration changes
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  // Validate JSON size
  if (!isJsonSizeSafe(server.arg("plain"))) {
    server.send(413, "text/plain", "Payload too large");
    return;
  }

  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", String("Bad JSON: ") + err.c_str());
    return;
  }
  applyConfiguration(doc);
  server.send(200, "text/plain", "OK");
}

void handleConfigSave(WebServer& server) {
  // Authentication required for saving configurations
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing name");
    return;
  }

  // Validate JSON size
  if (!isJsonSizeSafe(server.arg("plain"))) {
    server.send(413, "text/plain", "Payload too large");
    return;
  }

  String name = server.arg("name");
  String path = configPathFromName(name);
  File f = LittleFS.open(path, "w");
  if (!f) {
    server.send(500, "text/plain", "Open failed");
    return;
  }
  f.print(server.arg("plain"));
  f.close();
  server.send(200, "text/plain", "Saved");
}

void handleConfigLoad(WebServer& server) {
  // Config load might contain sensitive info, require auth
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing name");
    return;
  }
  String name = server.arg("name");
  String path = configPathFromName(name);
  File f = LittleFS.open(path, "r");
  if (!f) {
    // Back-compat: legacy root path
    String legacy = String("/config_") + sanitizeBasename(name) + String(".json");
    f = LittleFS.open(legacy, "r");
  }
  if (!f) {
    server.send(404, "text/plain", "Not found");
    return;
  }
  server.streamFile(f, "application/json");
  f.close();
}

void handleConfigList(WebServer& server) {
  // Authentication required for config listing
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  DynamicJsonDocument doc(4096);
  JsonArray arr = doc.to<JsonArray>();

  // New location: /config directory
  File cfg = LittleFS.open(CFG_DIR);
  if (cfg && cfg.isDirectory()) {
    File file = cfg.openNextFile();
    while (file) {
      String n = String(file.name());
      // Expect /config/<name>.json
      if (n.endsWith(".json")) {
        String base = n;
        int slash = base.lastIndexOf('/');
        if (slash >= 0) base = base.substring(slash + 1);
        if (base.endsWith(".json")) base = base.substring(0, base.length() - 5);
        if (base.length()) arr.add(base);
      }
      file.close();
      file = cfg.openNextFile();
    }
    cfg.close();
  }

  // Back-compat: legacy root files /config_<name>.json
  File root = LittleFS.open("/");
  if (root && root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      String n = String(file.name());
      if (n.startsWith("/config_") && n.endsWith(".json")) {
        n.replace("/config_", "");
        n.replace(".json", "");
        arr.add(n);
      }
      file.close();
      file = root.openNextFile();
    }
    root.close();
  }

  String out;
  serializeJson(arr, out);
  server.send(200, "application/json", out);
}

void handleConfigExists(WebServer& server) {
  // Authentication required for config check
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing name");
    return;
  }
  DynamicJsonDocument doc(128);
  doc["exists"] = LittleFS.exists(configPathFromName(server.arg("name")));
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleConfigDelete(WebServer& server) {
  // Authentication required for deleting configurations
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!server.hasArg("name")) {
    server.send(400, "text/plain", "Missing name");
    return;
  }
  LittleFS.remove(configPathFromName(server.arg("name")));
  server.send(200, "text/plain", "Deleted");
}

// ===================================================================================
// SYSTEM API HANDLERS
// ===================================================================================
void handleApiHealth(WebServer& server) {
  addSecurityHeaders(server);

  // Update health snapshot
  HealthSnapshotManager::getInstance().update();
  auto health = HealthSnapshotManager::getInstance().getSnapshot();

  DynamicJsonDocument doc(1024);

  // Overall health
  doc["status"] = HealthSnapshotManager::getInstance().getHealthString();
  doc["overall_health"] = (int)health.overall;
  doc["boot_complete"] = health.boot_complete;
  doc["boot_degraded"] = health.boot_degraded;

  // Message counts
  doc["active_messages"] = health.active_count;
  doc["error_count"] = health.error_count;
  doc["warn_count"] = health.warn_count;
  doc["info_count"] = health.info_count;

  // Subsystem status
  JsonObject subsystems = doc.createNestedObject("subsystems");
  subsystems["filesystem"] = health.filesystem_ok;
  subsystems["sd_present"] = health.sd_present;
  subsystems["display"] = health.display_ok;
  subsystems["network"] = health.network_ok;
  subsystems["services"] = health.services_ok;

  // System metrics
  doc["uptime_ms"] = millis();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["wifi_connected"] = WiFi.isConnected();
  doc["wifi_rssi"] = WiFi.RSSI();
  doc["ip"] = WiFi.localIP().toString();

#if SD_SUPPORT_ENABLED
  if (isSDEnabled) {
    doc["sd_mounted"] = g_sdMounted;
  }
#endif

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleApiSystem(WebServer& server) {
  // Authentication required for detailed system info
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  DynamicJsonDocument doc(768);
  doc["cpu_mhz"] = ESP.getCpuFreqMHz();
  doc["flash_speed_hz"] = (uint32_t)ESP.getFlashChipSpeed();
  doc["sdk"] = String(ESP.getSdkVersion());
  doc["chip_rev"] = ESP.getChipRevision();
  doc["mac"] = WiFi.macAddress();
  doc["heap_total"] = ESP.getHeapSize();
  doc["heap_free"] = ESP.getFreeHeap();
  doc["heap_min_free"] = ESP.getMinFreeHeap();

  size_t fsTotal = LittleFS.totalBytes();
  size_t fsUsed = LittleFS.usedBytes();
  doc["fs_total"] = (uint64_t)fsTotal;
  doc["fs_used"] = (uint64_t)fsUsed;
  doc["fs_free"] = (uint64_t)(fsTotal >= fsUsed ? (fsTotal - fsUsed) : 0);

  doc["sd_enabled"] = (bool)isSDEnabled;
#if SD_SUPPORT_ENABLED
  // Lazy-mount to reflect insertion state without forcing a long init on every refresh.
  // If not mounted, attempt a quick mount. If it fails, report not mounted.
  if (!g_sdMounted && isSDEnabled) {
    sdEnsureMounted();
  }
  doc["sd_mounted"] = (bool)g_sdMounted;
  if (g_sdMounted) {
    uint64_t total = 0, used = 0;
// Arduino-ESP32 SD.h provides totalBytes/usedBytes on FS.
#  if defined(ARDUINO_ARCH_ESP32)
    total = sd_total_bytes();
    used = sd_used_bytes();
#  endif
    doc["sd_total"] = total;
    doc["sd_used"] = used;
    doc["sd_free"] = (total >= used) ? (total - used) : 0;
  } else {
    doc["sd_total"] = 0;
    doc["sd_used"] = 0;
    doc["sd_free"] = 0;
  }
#else
  doc["sd_mounted"] = false;
  doc["sd_total"] = 0;
  doc["sd_used"] = 0;
  doc["sd_free"] = 0;
#endif

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// ===================================================================================
// SD CARD API HANDLERS
// ===================================================================================
#if SD_SUPPORT_ENABLED

void handleApiSdStatus(WebServer& server) {
  // Authentication required for SD status
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  DynamicJsonDocument doc(256);
  doc["enabled"] = (bool)isSDEnabled;
  if (!isSDEnabled) {
    doc["mounted"] = false;
    doc["msg"] = "Disabled";
  } else {
    bool ok = sdEnsureMounted();
    doc["mounted"] = ok;
    doc["msg"] = g_sdMountMsg;
  }
  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

void handleApiSdList(WebServer& server) {
  // Authentication required for SD file listing
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!sdEnsureMounted()) {
    server.send(500, "text/plain", g_sdMountMsg);
    return;
  }
  String dir = server.arg("dir");
  if (!dir.length()) dir = "/";
  if (!dir.startsWith("/")) dir = "/" + dir;

  SDFile d = sd_open(dir.c_str(), O_RDONLY);
  if (!d) {
    server.send(404, "text/plain", "Dir not found");
    return;
  }

  // Iterate
  String json = "[";
  bool first = true;
  SDFile e;
  while (sdOpenNext(d, e)) {
    String name = sdFileName(e);
    bool isdir = sdFileIsDir(e);
    uint64_t size = sd_filesize(e);
    e.close();

    if (name == "." || name == "..") continue;
    if (!first) json += ",";
    first = false;
    json += "{\"name\":\"" + jsonEscape(name) + "\",\"dir\":" + String(isdir ? "true" : "false") +
            ",\"size\":" + String((unsigned long long)size) + "}";
  }
  d.close();
  json += "]";
  server.send(200, "application/json", json);
}

void handleApiSdDelete(WebServer& server) {
  // Authentication required for delete operations
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!isSDEnabled) {
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  if (!sdEnsureMounted()) {
    server.send(500, "text/plain", sanitizeError(g_sdMountMsg));
    return;
  }

  // Validate JSON size
  if (!isJsonSizeSafe(server.arg("plain"))) {
    server.send(413, "text/plain", "Payload too large");
    return;
  }

  DynamicJsonDocument doc(256);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }

  String path = sanitizePath(doc["path"] | "");

  // Additional security validation
  if (!isPathSafe(path)) {
    server.send(400, "text/plain", "Invalid path");
    return;
  }

  if (path == "/" || path.length() < 2) {
    server.send(400, "text/plain", "Refuse");
    return;
  }
  bool ok = sdRemoveRecursive(path);
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Delete failed");
}

void handleApiSdRename(WebServer& server) {
  // Authentication required for rename operations
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!isSDEnabled) {
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  if (!sdEnsureMounted()) {
    server.send(500, "text/plain", sanitizeError(g_sdMountMsg));
    return;
  }

  // Validate JSON size
  if (!isJsonSizeSafe(server.arg("plain"))) {
    server.send(413, "text/plain", "Payload too large");
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }

  String from = sanitizePath(doc["from"] | "");
  String to = sanitizePath(doc["to"] | "");

  // Additional security validation
  if (!isPathSafe(from) || !isPathSafe(to)) {
    server.send(400, "text/plain", "Invalid path");
    return;
  }

  if (!from.length() || !to.length() || from == "/" || to == "/") {
    server.send(400, "text/plain", "Bad path");
    return;
  }

  bool ok = sd_rename(from, to);
  server.send(ok ? 200 : 500, "text/plain", ok ? "OK" : "Rename failed");
}

void handleSdDownload(WebServer& server) {
  // Authentication required for file downloads
  if (!checkAuth(server)) return;

  if (!isSDEnabled) {
    addSecurityHeaders(server);
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  if (!sdEnsureMounted()) {
    addSecurityHeaders(server);
    server.send(500, "text/plain", sanitizeError(g_sdMountMsg));
    return;
  }

  String path = server.hasArg("path") ? server.arg("path") : String("");
  path = sanitizePath(path);

  // Additional security: verify path is safe
  if (!isPathSafe(path)) {
    addSecurityHeaders(server);
    server.send(400, "text/plain", "Invalid path");
    return;
  }

  SDFile f = sd_open(path.c_str(), O_RDONLY);
  if (!f || f.isDirectory()) {
    addSecurityHeaders(server);
    server.send(404, "text/plain", "Not found");
    return;
  }

  String fn = path;
  int slash = fn.lastIndexOf('/');
  if (slash >= 0) fn = fn.substring(slash + 1);

  addSecurityHeaders(server);
  server.sendHeader("Content-Disposition", String("attachment; filename=\"") + fn + "\"");
  server.sendHeader("Cache-Control", "no-store");
  server.setContentLength((int)f.size());
  server.send(200, "application/octet-stream", "");

  WiFiClient client = server.client();
  uint8_t buf[1024];
  while (client.connected()) {
    int n = f.read(buf, sizeof(buf));
    if (n <= 0) break;
    client.write(buf, n);
    delay(0);
  }
  f.close();
}

void handleApiSdEject(WebServer& server) {
  // Authentication required for SD eject
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!isSDEnabled) {
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  sdUnmount();
  server.send(200, "text/plain", "Ejected");
}

void handleApiSdUploadPost(WebServer& server) {
  // Response is sent when upload completes (in handleUploadSD).
  server.send(200, "text/plain",
              g_sdUploadOk ? String("OK: ") + g_sdUploadMsg : String("ERR: ") + g_sdUploadMsg);
}

void handleUploadSD(WebServer& server) {
  if (!isSDEnabled) {
    g_sdUploadOk = false;
    g_sdUploadMsg = "SD disabled";
    return;
  }
  if (!sdEnsureMounted()) {
    g_sdUploadOk = false;
    g_sdUploadMsg = g_sdMountMsg;
    return;
  }

  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    // Check upload size limit
    if (!isUploadSizeSafe(upload.totalSize)) {
      g_sdUploadOk = false;
      g_sdUploadMsg = "File too large";
      return;
    }

    g_sdUploadOk = false;
    g_sdUploadMsg = "";
    String dir = server.hasArg("dir") ? server.arg("dir") : String("/");
    dir = sanitizePath(dir);

    // Validate path safety
    if (!isPathSafe(dir)) {
      g_sdUploadOk = false;
      g_sdUploadMsg = "Invalid path";
      return;
    }

    if (!dir.endsWith("/")) dir += "/";
    String fn = sanitizeBasename(upload.filename);

    // Validate filename
    if (!isFilenameSafe(fn)) {
      g_sdUploadOk = false;
      g_sdUploadMsg = "Invalid filename";
      return;
    }

    if (!fn.length()) fn = "upload.bin";
    g_sdUploadPath = dir + fn;
    g_sdUploadFile = sd_open(g_sdUploadPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC);
    if (!g_sdUploadFile) {
      g_sdUploadMsg = "Open failed";
      return;
    }
    g_sdUploadOk = true;
    g_sdUploadMsg = g_sdUploadPath;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (g_sdUploadOk && g_sdUploadFile) {
      size_t w = g_sdUploadFile.write(upload.buf, upload.currentSize);
      if (w != upload.currentSize) {
        g_sdUploadOk = false;
        g_sdUploadMsg = "Write failed";
      }
    }
    yield();
  } else if (upload.status == UPLOAD_FILE_END) {
    if (g_sdUploadFile) g_sdUploadFile.close();
    // Leave mounted; safe-eject is explicit.
    yield();
  }
}

void handleUploadSdPost(WebServer& server) {
  // Authentication required for uploads
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (g_sdUploadOk) {
    server.send(200, "text/plain", "Uploaded: " + sanitizeError(g_sdUploadMsg));
  } else {
    String msg = g_sdUploadMsg.length() ? g_sdUploadMsg : String("Upload failed");
    server.send(500, "text/plain", msg);
  }
}

void handleApiSdImportFw(WebServer& server) {
  // Authentication required for firmware import
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);

  if (!isSDEnabled) {
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  if (!sdEnsureMounted()) {
    server.send(500, "text/plain", g_sdMountMsg);
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }

  String sdPath = sanitizePath(doc["path"] | "");
  String slot = sanitizeBasename(doc["slot"] | "");
  String ver = sanitizeBasename(doc["ver"] | "");
  if (!sdPath.length() || !slot.length() || !ver.length()) {
    server.send(400, "text/plain", "Missing fields");
    return;
  }

  SDFile src = sd_open(sdPath.c_str(), O_RDONLY);
  if (!src || src.isDirectory()) {
    server.send(404, "text/plain", "SD file not found");
    return;
  }
  src.close();

  String lower = sdPath;
  lower.toLowerCase();
  String outPath = fwSlotPath(slot, ver);

  // Clean existing slot before import
  fsCleanSlot(slot);

  bool ok = false;
  String msg;

  if (lower.endsWith(".hex")) {
    // Convert HEX -> SW8B text -> .bin
    String tmpIn = ensureTempPathForUpload("sd.hex");
    String tmpOut = ensureTempPathForUpload("sd_fw.txt");

    // Copy SD hex to temp file to reuse converter's FS-agnostic begin
    SDFile inSD = sd_open(sdPath.c_str(), O_RDONLY);
    File inLF = LittleFS.open(tmpIn, "w");
    if (!inSD || !inLF) {
      if (inSD) inSD.close();
      if (inLF) inLF.close();
      server.send(500, "text/plain", "Temp open failed");
      return;
    }
    uint8_t buf[2048];
    while (true) {
      size_t r = inSD.read(buf, sizeof(buf));
      if (!r) break;
      if (inLF.write(buf, r) != r) {
        break;
      }
      yield();
    }
    inSD.close();
    inLF.close();

    IntelHexSW8B conv;
    if (!conv.begin(LittleFS, TEMP_DIR)) {
      msg = "Converter init failed";
    } else if (!conv.loadHexFile(tmpIn.c_str())) {
      msg = "HEX parse failed";
    } else if (!conv.exportFW_CH32V003_16K_Strict(tmpOut.c_str(), true, false)) {
      msg = "Text export failed";
    } else {
      // Now convert fw.txt -> bin with existing pipeline
      ok = fwTxtToBin(tmpOut, outPath, msg);
    }

    LittleFS.remove(tmpIn);
    LittleFS.remove(tmpOut);
  } else if (lower.endsWith(".txt")) {
    ok = fwTxtToBin(sdPath, outPath, msg, /*fromSD=*/true);
  } else {
    // .bin or unknown: copy raw
    ok = sdCopyToLittleFS(sdPath.c_str(), outPath.c_str());
  }

  server.send(ok ? 200 : 500, "text/plain",
              ok ? String("Imported: ") + outPath : String("Failed: ") + msg);
}

void handleApiSdConvertFw(WebServer& server) {
  if (!isSDEnabled) {
    server.send(403, "text/plain", "SD disabled");
    return;
  }
  if (!sdEnsureMounted()) {
    server.send(500, "text/plain", g_sdMountMsg);
    return;
  }

  DynamicJsonDocument doc(512);
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "text/plain", "Bad JSON");
    return;
  }
  doc["slot"] = doc["slot"] | "";
  doc["ver"] = doc["ver"] | "";
  doc["path"] = doc["path"] | "";
  // Reuse import handler
  handleApiSdImportFw(server);
}

#endif  // SD_SUPPORT_ENABLED

// ===================================================================================
// MESSAGE CENTER API HANDLERS
// ===================================================================================

#include "../../core/messages/boot_manager.h"
#include "../../core/messages/message_center.h"

// Helper: Convert severity to string
const char* severityToString(MessageSeverity sev) {
  switch (sev) {
    case MessageSeverity::INFO:
      return "INFO";
    case MessageSeverity::WARN:
      return "WARN";
    case MessageSeverity::ERROR:
      return "ERROR";
    default:
      return "UNKNOWN";
  }
}

// GET /api/messages/summary
// Returns: { active_count, history_count, highest_severity, sequence }
void handleApiMessagesSummary(WebServer& server) {
  MessageSummary summary = MessageCenter::getInstance().getSummary();

  JsonDocument doc;
  doc["active_count"] = summary.active_count;
  doc["history_count"] = summary.history_count;
  doc["highest_severity"] = severityToString(summary.highest_active_severity);
  doc["sequence"] = summary.sequence;

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// GET /api/messages/active
// Returns: [ { id, timestamp, severity, source, code, title, details, count }, ... ]
void handleApiMessagesActive(WebServer& server) {
  const auto& messages = MessageCenter::getInstance().getActiveMessages();

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for (const auto& msg : messages) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = msg.id;
    obj["timestamp"] = msg.timestamp;
    obj["last_ts"] = msg.last_ts;
    obj["severity"] = severityToString(msg.severity);
    obj["source"] = msg.source;
    obj["code"] = msg.code;
    obj["title"] = msg.title;
    obj["details"] = msg.details;
    obj["count"] = msg.count;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// GET /api/messages/history
// Returns: [ { id, timestamp, severity, source, code, title, details, count }, ... ]
void handleApiMessagesHistory(WebServer& server) {
  const auto& messages = MessageCenter::getInstance().getHistoryMessages();

  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();

  for (const auto& msg : messages) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = msg.id;
    obj["timestamp"] = msg.timestamp;
    obj["last_ts"] = msg.last_ts;
    obj["severity"] = severityToString(msg.severity);
    obj["source"] = msg.source;
    obj["code"] = msg.code;
    obj["title"] = msg.title;
    obj["details"] = msg.details;
    obj["count"] = msg.count;
  }

  String json;
  serializeJson(doc, json);
  server.send(200, "application/json", json);
}

// POST /api/messages/ack
// Body: { "msg_id": <id> }
// Returns: { "success": true/false }
void handleApiMessagesAck(WebServer& server) {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, server.arg("plain"));
  if (err) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
    return;
  }

  uint32_t msg_id = doc["msg_id"] | 0;
  if (msg_id == 0) {
    server.send(400, "application/json", "{\"success\":false,\"error\":\"Missing msg_id\"}");
    return;
  }

  bool success = MessageCenter::getInstance().acknowledge(msg_id);

  if (success) {
    server.send(200, "application/json", "{\"success\":true}");
  } else {
    server.send(404, "application/json", "{\"success\":false,\"error\":\"Message not found\"}");
  }
}

// POST /api/messages/ack_all
// Returns: { "success": true }
void handleApiMessagesAckAll(WebServer& server) {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  MessageCenter::getInstance().acknowledgeAll();
  server.send(200, "application/json", "{\"success\":true}");
}

// POST /api/messages/clear_history
// Returns: { "success": true }
void handleApiMessagesClearHistory(WebServer& server) {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "Method Not Allowed");
    return;
  }

  MessageCenter::getInstance().clearHistory();
  server.send(200, "application/json", "{\"success\":true}");
}

// GET /messages
// Returns: HTML page for viewing/managing messages
void handleMessagesPage(WebServer& server) {
  server.send(200, "text/html", FPSTR(MESSAGES_HTML));
}

// ===================================================================================
// TEST/DEBUG HANDLERS
// ===================================================================================

// GET /api/test/gauntlet
// Trigger test messages for verification
void handleApiTestGauntlet(WebServer& server) {
  // Post test messages of each severity
  msg_info("test", TEST_INFO, "Gauntlet INFO Test", "This is an informational test message");
  msg_warn("test", TEST_WARN, "Gauntlet WARN Test", "This is a warning test message");
  msg_error("test", TEST_ERROR, "Gauntlet ERROR Test", "This is an error test message");

  // Post coalescing test (5 times)
  for (int i = 0; i < 5; i++) {
    msg_warn("test", TEST_COALESCE, "Coalesce Test", "Occurrence #%d", i + 1);
    delay(100);
  }

  server.send(200, "application/json",
              "{\"success\":true,\"message\":\"Gauntlet test complete. Check Messages screen.\"}");
}
