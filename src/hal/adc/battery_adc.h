/*
 * Battery ADC HAL - Header
 * 
 * Hardware abstraction for battery voltage monitoring via ADC.
 * Provides voltage reading and percentage calculation.
 * 
 * For LCDWIKI 3.5" ESP32-32E:
 *   - ADC Pin: GPIO34 (ADC1_CH6, input only)
 *   - Voltage range: Typically 3.3V - 4.2V for LiPo
 */

#pragma once

#include <stdint.h>

// Battery ADC configuration
struct BatteryADCConfig {
  int adcPin;           // ADC pin number (e.g., GPIO34)
  uint16_t minVoltage;  // Minimum voltage in millivolts (e.g., 3300 for 3.3V)
  uint16_t maxVoltage;  // Maximum voltage in millivolts (e.g., 4200 for 4.2V)
  uint16_t r1;          // Voltage divider R1 (high side) in ohms (0 if direct connection)
  uint16_t r2;          // Voltage divider R2 (low side) in ohms (0 if direct connection)
};

// Initialize battery ADC
void batteryAdcInit(const BatteryADCConfig& config);

// Read raw ADC value (0-4095 for 12-bit ADC)
uint16_t batteryAdcReadRaw();

// Read battery voltage in millivolts
uint16_t batteryAdcReadVoltage();

// Get battery percentage (0-100%)
uint8_t batteryAdcGetPercentage();

// Check if battery is charging (requires additional hardware detection)
bool batteryAdcIsCharging();

// Get battery status as string ("Full", "Good", "Low", "Critical", "Unknown")
const char* batteryAdcGetStatus();
