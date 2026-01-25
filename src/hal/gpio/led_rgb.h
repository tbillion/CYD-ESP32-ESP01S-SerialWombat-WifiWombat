/*
 * RGB LED HAL - Header
 *
 * Hardware abstraction for RGB LED control.
 * Supports common anode and common cathode configurations.
 *
 * For LCDWIKI 3.5" ESP32-32E:
 *   - RED:   GPIO22
 *   - GREEN: GPIO16
 *   - BLUE:  GPIO17
 *   - Common anode (active low)
 */

#pragma once

#include <stdint.h>

// RGB LED configuration
struct RGBLEDConfig {
  int redPin;
  int greenPin;
  int bluePin;
  bool commonAnode;  // true = common anode (active low), false = common cathode (active high)
};

// Initialize RGB LED
void rgbLedInit(const RGBLEDConfig& config);

// Set individual color components (0-255)
void rgbLedSetColor(uint8_t red, uint8_t green, uint8_t blue);

// Set color from 24-bit RGB value (0x00RRGGBB)
void rgbLedSetColorHex(uint32_t color);

// Predefined colors
void rgbLedRed();
void rgbLedGreen();
void rgbLedBlue();
void rgbLedYellow();
void rgbLedCyan();
void rgbLedMagenta();
void rgbLedWhite();
void rgbLedOff();

// Blink effect
void rgbLedBlink(uint8_t red, uint8_t green, uint8_t blue, uint16_t onTimeMs, uint16_t offTimeMs,
                 uint8_t count);
