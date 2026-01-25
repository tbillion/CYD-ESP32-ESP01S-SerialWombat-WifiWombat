#include "validators.h"

/**
 * Validate I2C address (7-bit address range)
 */
bool isValidI2CAddress(uint8_t addr) {
  return (addr >= 0x08 && addr <= 0x77);
}

/**
 * Validate pin number for ESP32
 */
bool isValidPinNumber(int pin) {
  // ESP32 valid GPIO pins (exclude reserved/strapping pins)
  // Allow common GPIO pins, exclude flash pins (6-11)
  if (pin < 0 || pin > 39) return false;
  if (pin >= 6 && pin <= 11) return false;  // Flash pins
  return true;
}

/**
 * Validate integer is within range
 */
bool isValidRange(int value, int min_val, int max_val) {
  return (value >= min_val && value <= max_val);
}

/**
 * Enhanced path traversal protection
 * Prevents: .., absolute path escape, null bytes, control chars
 */
bool isPathSafe(const String& path) {
  // Check for null bytes
  if (path.indexOf('\0') >= 0) return false;

  // Check for control characters
  for (size_t i = 0; i < path.length(); i++) {
    char c = path[i];
    if (c < 0x20 && c != '\n' && c != '\r' && c != '\t') return false;
  }

  // Check for parent directory references
  if (path.indexOf("..") >= 0) return false;

  // Must start with / (absolute within filesystem)
  if (path.length() > 0 && path[0] != '/') return false;

  return true;
}

/**
 * Validate filename for filesystem safety
 * Only allows alphanumeric, underscore, dash, dot
 */
bool isFilenameSafe(const String& filename) {
  if (filename.length() == 0 || filename.length() > 255) return false;

  // Check for null bytes
  if (filename.indexOf('\0') >= 0) return false;

  // Must not start with dot (hidden files)
  if (filename[0] == '.') return false;

  // Check each character
  for (size_t i = 0; i < filename.length(); i++) {
    char c = filename[i];
    bool valid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                 c == '_' || c == '-' || c == '.';
    if (!valid) return false;
  }

  return true;
}

/**
 * Sanitize error messages to prevent info disclosure
 */
String sanitizeError(const String& error) {
  // Remove filesystem paths
  String safe = error;
  safe.replace("/littlefs/", "[FS]/");
  safe.replace("/sd/", "[SD]/");
  safe.replace("/temp/", "[TEMP]/");
  safe.replace("/fw/", "[FW]/");
  safe.replace("/config/", "[CFG]/");

  // Limit length
  if (safe.length() > 128) {
    safe = safe.substring(0, 125) + "...";
  }

  return safe;
}

/**
 * Validate JSON size before parsing
 */
bool isJsonSizeSafe(const String& json) {
  return (json.length() <= MAX_JSON_SIZE);
}

/**
 * Check if upload size is within limits
 */
bool isUploadSizeSafe(size_t size) {
  return (size <= MAX_UPLOAD_SIZE);
}
