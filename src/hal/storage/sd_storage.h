#pragma once

#include <Arduino.h>

#if SD_SUPPORT_ENABLED

#include <SPI.h>
#include <SdFat.h>

// Type alias for SdFat backend
typedef FsFile SDFile;

// SD card management functions
bool sdMount();
void sdUnmount();
bool sdExists(const char* path);
bool sdMkdir(const char* path);
bool sdRemove(const char* path);
bool sdRmdir(const char* path);
bool sdRename(const char* from, const char* to);
SDFile sdOpen(const char* path, oflag_t flags);
bool sdIsDir(const char* path);
bool sdGetStats(uint64_t &totalBytes, uint64_t &usedBytes);

// Unified helpers for file operations
bool sdFileIsDir(SDFile &f);
String sdFileName(SDFile &f);
bool sdOpenNext(SDFile &dir, SDFile &out);

// Helper functions for usage calculation
static inline uint64_t sd_total_bytes() {
  uint64_t t, u;
  return sdGetStats(t, u) ? t : 0;
}

static inline uint64_t sd_used_bytes() {
  uint64_t t, u;
  return sdGetStats(t, u) ? u : 0;
}

// SD state accessors
bool sdEnsureMounted();
bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path);
bool sdGetUsage(uint64_t &total, uint64_t &used);

// Global SD state (extern - defined in .cpp)
extern bool g_sdMounted;
extern String g_sdMountMsg;
extern int g_sd_cs;
extern int g_sd_mosi;
extern int g_sd_miso;
extern int g_sd_sck;
extern bool g_sdUploadOk;
extern String g_sdUploadMsg;

#endif // SD_SUPPORT_ENABLED
