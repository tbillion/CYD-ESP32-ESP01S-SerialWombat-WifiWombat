#pragma once

#include "system_config.h"

// Apply model preset to configuration
void applyModelPreset(SystemConfig& cfg);

// Set safe default configuration
void setConfigDefaults(SystemConfig& cfg);

// Load configuration from LittleFS
bool loadConfig(SystemConfig& cfg);

// Save configuration to LittleFS
bool saveConfig(const SystemConfig& cfg);

// Sync SD runtime pins from configuration
void syncSdRuntimePins(const SystemConfig& cfg);
