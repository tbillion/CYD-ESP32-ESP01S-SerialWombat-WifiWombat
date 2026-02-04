/*
 * RGB LED HAL - Implementation
 *
 * Hardware abstraction for RGB LED control with PWM support.
 */

#include "led_rgb.h"

#include <Arduino.h>

// PWM configuration
#define PWM_FREQ 5000
#define PWM_RESOLUTION 8  // 8-bit (0-255)

// Static storage for configuration
static RGBLEDConfig g_ledConfig;
static const int RED_CHANNEL = 0;
static const int GREEN_CHANNEL = 1;
static const int BLUE_CHANNEL = 2;

void rgbLedInit(const RGBLEDConfig& config) {
  g_ledConfig = config;

  // Configure pins as outputs
  pinMode(config.redPin, OUTPUT);
  pinMode(config.greenPin, OUTPUT);
  pinMode(config.bluePin, OUTPUT);

  // Setup PWM channels
  ledcSetup(RED_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(GREEN_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
  ledcSetup(BLUE_CHANNEL, PWM_FREQ, PWM_RESOLUTION);

  // Attach pins to PWM channels
  ledcAttachPin(config.redPin, RED_CHANNEL);
  ledcAttachPin(config.greenPin, GREEN_CHANNEL);
  ledcAttachPin(config.bluePin, BLUE_CHANNEL);

  // Turn off LED initially
  rgbLedOff();
}

void rgbLedSetColor(uint8_t red, uint8_t green, uint8_t blue) {
  // Invert values for common anode configuration
  if (g_ledConfig.commonAnode) {
    red = 255 - red;
    green = 255 - green;
    blue = 255 - blue;
  }

  ledcWrite(RED_CHANNEL, red);
  ledcWrite(GREEN_CHANNEL, green);
  ledcWrite(BLUE_CHANNEL, blue);
}

void rgbLedSetColorHex(uint32_t color) {
  uint8_t red = (color >> 16) & 0xFF;
  uint8_t green = (color >> 8) & 0xFF;
  uint8_t blue = color & 0xFF;
  rgbLedSetColor(red, green, blue);
}

// Predefined colors
void rgbLedRed() {
  rgbLedSetColor(255, 0, 0);
}
void rgbLedGreen() {
  rgbLedSetColor(0, 255, 0);
}
void rgbLedBlue() {
  rgbLedSetColor(0, 0, 255);
}
void rgbLedYellow() {
  rgbLedSetColor(255, 255, 0);
}
void rgbLedCyan() {
  rgbLedSetColor(0, 255, 255);
}
void rgbLedMagenta() {
  rgbLedSetColor(255, 0, 255);
}
void rgbLedWhite() {
  rgbLedSetColor(255, 255, 255);
}
void rgbLedOff() {
  rgbLedSetColor(0, 0, 0);
}

void rgbLedBlink(uint8_t red, uint8_t green, uint8_t blue, uint16_t onTimeMs, uint16_t offTimeMs,
                 uint8_t count) {
  for (uint8_t i = 0; i < count; i++) {
    rgbLedSetColor(red, green, blue);
    delay(onTimeMs);
    rgbLedOff();
    if (i < count - 1) {  // Don't delay after last blink
      delay(offTimeMs);
    }
  }
}
