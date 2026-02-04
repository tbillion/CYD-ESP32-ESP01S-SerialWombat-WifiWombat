#pragma once

#include <Arduino.h>

#include <ArduinoJson.h>
#include <SerialWombat.h>
#include <WebServer.h>
#include <Wire.h>

// ===================================================================================
// I2C Manager Service
// ===================================================================================
// Provides I2C scanning and device detection functionality
// - Fast I2C scan (handleScanData)
// - Deep scan with Serial Wombat variant detection (handleDeepScan)
// - Device capability analysis (getDeepScanInfoSingle)
// ===================================================================================

/**
 * I2C traffic tracking helpers
 */
extern volatile uint32_t g_i2c_tx_count;
extern volatile uint32_t g_i2c_rx_count;
extern volatile bool g_i2c_tx_blink;
extern volatile bool g_i2c_rx_blink;

inline void i2cMarkTx() {
  g_i2c_tx_count++;
  g_i2c_tx_blink = true;
}
inline void i2cMarkRx() {
  g_i2c_rx_count++;
  g_i2c_rx_blink = true;
}

/**
 * Variant detection result structure
 */
struct VariantInfo {
  String variant;
  bool caps[41];
};

/**
 * Pin mode string lookup table (defined in i2c_manager.cpp)
 */
extern const char* const pinModeStrings[] PROGMEM;

/**
 * Perform fast I2C scan and return results
 * @param server WebServer instance for sending response
 */
void handleScanData(WebServer& server);

/**
 * Perform deep I2C scan with Serial Wombat variant detection
 * @param server WebServer instance for sending response
 */
void handleDeepScan(WebServer& server);

/**
 * Get detailed Serial Wombat variant info for a single address
 * @param addr I2C address to scan
 * @return VariantInfo structure with detected variant and capabilities
 */
VariantInfo getDeepScanInfoSingle(uint8_t addr);
