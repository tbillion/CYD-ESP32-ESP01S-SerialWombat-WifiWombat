#ifndef API_HANDLERS_H
#define API_HANDLERS_H

#include <Arduino.h>

#include <ArduinoJson.h>
#include <WebServer.h>

// ===================================================================================
// ROOT AND MAIN HANDLERS
// ===================================================================================
void handleRoot(WebServer& server);
void handleScanner(WebServer& server);

// ===================================================================================
// WIFI AND SYSTEM HANDLERS
// ===================================================================================
void handleResetWiFi(WebServer& server);
void handleFormat(WebServer& server);

// ===================================================================================
// FIRMWARE MANAGEMENT HANDLERS
// ===================================================================================
void handleCleanSlot(WebServer& server);
void handleUploadHex(WebServer& server);
void handleUploadHexPost(WebServer& server);
void handleUploadFW(WebServer& server);
void handleFlashFW(WebServer& server);

// ===================================================================================
// TCP BRIDGE HANDLER
// ===================================================================================
void handleTcpBridge();

// ===================================================================================
// CONFIG API HANDLERS
// ===================================================================================
void handleApiVariant(WebServer& server);
void handleApiApply(WebServer& server);
void handleConfigSave(WebServer& server);
void handleConfigLoad(WebServer& server);
void handleConfigList(WebServer& server);
void handleConfigExists(WebServer& server);
void handleConfigDelete(WebServer& server);

// ===================================================================================
// SYSTEM API HANDLERS
// ===================================================================================
void handleApiHealth(WebServer& server);
void handleApiSystem(WebServer& server);

// ===================================================================================
// MESSAGE CENTER API HANDLERS
// ===================================================================================
void handleApiMessagesSummary(WebServer& server);
void handleApiMessagesActive(WebServer& server);
void handleApiMessagesHistory(WebServer& server);
void handleApiMessagesAck(WebServer& server);
void handleApiMessagesAckAll(WebServer& server);
void handleApiMessagesClearHistory(WebServer& server);
void handleMessagesPage(WebServer& server);

// ===================================================================================
// TEST/DEBUG HANDLERS
// ===================================================================================
void handleApiTestGauntlet(WebServer& server);

// ===================================================================================
// SD CARD API HANDLERS
// ===================================================================================
#if SD_SUPPORT_ENABLED
void handleApiSdStatus(WebServer& server);
void handleApiSdList(WebServer& server);
void handleApiSdDelete(WebServer& server);
void handleApiSdRename(WebServer& server);
void handleSdDownload(WebServer& server);
void handleApiSdEject(WebServer& server);
void handleApiSdUploadPost(WebServer& server);
void handleUploadSD(WebServer& server);
void handleUploadSdPost(WebServer& server);
void handleApiSdImportFw(WebServer& server);
void handleApiSdConvertFw(WebServer& server);
#endif

#endif  // API_HANDLERS_H
