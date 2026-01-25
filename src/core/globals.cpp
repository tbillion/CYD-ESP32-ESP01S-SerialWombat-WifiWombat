/*
 * Global State Variables - Implementation
 *
 * Shared state variables used across multiple modules.
 * Extracted from original .ino file (line 156).
 */

#include "globals.h"

// SD support configuration
#ifndef SD_SUPPORT_ENABLED
#  define SD_SUPPORT_ENABLED 1
#endif

// Global runtime toggle for SD card support
// (When SD_SUPPORT_ENABLED==0 this stays false)
bool isSDEnabled = (SD_SUPPORT_ENABLED != 0);
