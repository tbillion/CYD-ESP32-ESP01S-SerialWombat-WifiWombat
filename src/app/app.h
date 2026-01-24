/*
 * Application Orchestrator - Header
 * 
 * Manages the overall application lifecycle, initialization, and coordination
 * between services, HAL, and UI components.
 */

#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include <WiFiClient.h>

class App {
public:
  // Singleton access
  static App& getInstance();
  
  // Application lifecycle
  void begin();
  void update();
  
  // Service accessors (for modules that need them)
  WebServer& getWebServer() { return server; }
  
private:
  App(); // Private constructor for singleton
  App(const App&) = delete;
  App& operator=(const App&) = delete;
  
  // Core services
  WebServer server;
  WiFiServer tcpServer;
  
  // Initialization phases
  void initSerial();
  void initFileSystem();
  void initConfiguration();
  void initHardware();
  void initDisplay();
  void initNetwork();
  void initWebServer();
  void initOTA();
  
  // Runtime update phases
  void updateOTA();
  void updateWebServer();
  void updateTCPBridge();
  void updateDisplay();
};
