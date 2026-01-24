/*
 * Battery ADC HAL - Implementation
 * 
 * Battery voltage monitoring with percentage calculation.
 */

#include "battery_adc.h"
#include <Arduino.h>

// ADC configuration
#define ADC_RESOLUTION 12       // 12-bit ADC
#define ADC_MAX_VALUE 4095      // 2^12 - 1
#define ADC_VREF_MV 3300        // Reference voltage in millivolts (3.3V)
#define ADC_SAMPLES 10          // Number of samples for averaging

// Static storage for configuration
static BatteryADCConfig g_batteryConfig;
static bool g_initialized = false;

void batteryAdcInit(const BatteryADCConfig& config) {
  g_batteryConfig = config;
  
  // Configure ADC pin
  pinMode(config.adcPin, INPUT);
  
  // Set ADC resolution
  analogReadResolution(ADC_RESOLUTION);
  
  // Set ADC attenuation for full range (0-3.3V)
  analogSetAttenuation(ADC_11db);  // 0-3.6V range
  
  g_initialized = true;
}

uint16_t batteryAdcReadRaw() {
  if (!g_initialized) {
    return 0;
  }
  
  // Read multiple samples and average for stability
  uint32_t sum = 0;
  for (int i = 0; i < ADC_SAMPLES; i++) {
    sum += analogRead(g_batteryConfig.adcPin);
    delay(1);  // Small delay between samples
  }
  
  return sum / ADC_SAMPLES;
}

uint16_t batteryAdcReadVoltage() {
  if (!g_initialized) {
    return 0;
  }
  
  uint16_t rawValue = batteryAdcReadRaw();
  
  // Convert ADC value to voltage (in millivolts)
  uint32_t adcVoltage = (rawValue * ADC_VREF_MV) / ADC_MAX_VALUE;
  
  // Apply voltage divider correction if configured
  if (g_batteryConfig.r1 > 0 && g_batteryConfig.r2 > 0) {
    // V_battery = V_adc * (R1 + R2) / R2
    adcVoltage = (adcVoltage * (g_batteryConfig.r1 + g_batteryConfig.r2)) / g_batteryConfig.r2;
  }
  
  return adcVoltage;
}

uint8_t batteryAdcGetPercentage() {
  if (!g_initialized) {
    return 0;
  }
  
  uint16_t voltage = batteryAdcReadVoltage();
  
  // Clamp voltage to configured range
  if (voltage <= g_batteryConfig.minVoltage) {
    return 0;
  }
  if (voltage >= g_batteryConfig.maxVoltage) {
    return 100;
  }
  
  // Linear interpolation between min and max voltage
  uint32_t range = g_batteryConfig.maxVoltage - g_batteryConfig.minVoltage;
  uint32_t position = voltage - g_batteryConfig.minVoltage;
  uint8_t percentage = (position * 100) / range;
  
  return percentage;
}

bool batteryAdcIsCharging() {
  // This requires additional hardware (e.g., charging status pin)
  // Return false as default - can be enhanced with hardware-specific detection
  return false;
}

const char* batteryAdcGetStatus() {
  if (!g_initialized) {
    return "Unknown";
  }
  
  uint8_t percentage = batteryAdcGetPercentage();
  
  if (percentage >= 90) {
    return "Full";
  } else if (percentage >= 50) {
    return "Good";
  } else if (percentage >= 20) {
    return "Low";
  } else if (percentage >= 5) {
    return "Critical";
  } else {
    return "Empty";
  }
}
