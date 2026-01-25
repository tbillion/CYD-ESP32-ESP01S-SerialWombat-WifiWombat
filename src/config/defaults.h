#pragma once

// ===================================================================================
// --- SECURITY CONFIGURATION ---
// ===================================================================================
#define SECURITY_ENABLED 1
#define AUTH_USERNAME "admin"
#define AUTH_PASSWORD "CHANGE_ME_NOW"  // MUST be changed!

// Build-time check for default password (compile warning)
#if defined(SECURITY_ENABLED) && SECURITY_ENABLED == 1
#  if !defined(AUTH_PASSWORD) || \
      (defined(AUTH_PASSWORD) && strcmp(AUTH_PASSWORD, "CHANGE_ME_NOW") == 0)
#    warning \
        "*** SECURITY WARNING: Default password detected! Change AUTH_PASSWORD before deployment ***"
#  endif
#endif

#define MAX_UPLOAD_SIZE (5 * 1024 * 1024)  // 5MB max upload
#define MAX_JSON_SIZE 8192                 // 8KB max JSON payload

// CORS Configuration
#define CORS_ALLOW_ORIGIN "*"  // CHANGE IN PRODUCTION!

// Rate limiting
static const uint16_t AUTH_LOCKOUT_MS = 5000;  // 5 second lockout after failed auth

// ===================================================================================
// --- Pre-Compilation Configuration ---
// ===================================================================================
#define SD_SUPPORT_ENABLED 1

// Pin mapping for SPI-mode SD cards
#define SD_CS 5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK 18

// ESP-IDF host enum compatibility (Arduino-ESP32 v3.x)
#ifndef VSPI_HOST
#  define VSPI_HOST SPI3_HOST
#endif
#ifndef HSPI_HOST
#  define HSPI_HOST SPI2_HOST
#endif

// ===================================================================================
// --- Display / Touch / LVGL Configuration ---
// ===================================================================================
#define DISPLAY_SUPPORT_ENABLED 1

// Default enable states (can be overridden by config.json)
#define DEFAULT_DISPLAY_ENABLE 1
#define DEFAULT_TOUCH_ENABLE 1
#define DEFAULT_LVGL_ENABLE 1

// Headless auto-config timeout on first boot
#define FIRST_BOOT_HEADLESS_TIMEOUT_MS 30000UL

// Battery ADC pin (set to -1 to disable)
#define BATTERY_ADC_PIN -1
