#pragma once

#include <Arduino.h>
#include "../../config/defaults.h"

/**
 * Validate I2C address (7-bit address range)
 */
bool isValidI2CAddress(uint8_t addr);

/**
 * Validate pin number for ESP32
 */
bool isValidPinNumber(int pin);

/**
 * Validate integer is within range
 */
bool isValidRange(int value, int min_val, int max_val);

/**
 * Enhanced path traversal protection
 * Prevents: .., absolute path escape, null bytes, control chars
 */
bool isPathSafe(const String& path);

/**
 * Validate filename for filesystem safety
 * Only allows alphanumeric, underscore, dash, dot
 */
bool isFilenameSafe(const String& filename);

/**
 * Sanitize error messages to prevent info disclosure
 */
String sanitizeError(const String& error);

/**
 * Validate JSON size before parsing
 */
bool isJsonSizeSafe(const String& json);

/**
 * Check if upload size is within limits
 */
bool isUploadSizeSafe(size_t size);
