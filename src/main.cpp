/*
 * CYD ESP32 Serial Wombat WiFi Bridge - Main Entry Point
 *
 * This is the refactored entry point that delegates to modular components.
 * Original monolithic code has been extracted into services, HAL, and UI modules.
 */

#include <Arduino.h>

#include "app/app.h"

void setup() {
  // Delegate all initialization to the application orchestrator
  App::getInstance().begin();
}

void loop() {
  // Delegate loop processing to the application orchestrator
  App::getInstance().update();
}
