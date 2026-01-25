#include "sd_storage.h"
#include "../../config/defaults.h"

#if SD_SUPPORT_ENABLED

#include <LittleFS.h>

// Shared SD state
bool   g_sdMounted = false;
String g_sdMountMsg = "";
int    g_sd_cs   = SD_CS;
int    g_sd_mosi = SD_MOSI;
int    g_sd_miso = SD_MISO;
int    g_sd_sck  = SD_SCK;

// Upload state
bool   g_sdUploadOk = false;
String g_sdUploadMsg;

// SdFat backend
static SdFat sd;

bool sdMount() {
  if (g_sdMounted) return true;
  const uint8_t SD_CS_PIN = (uint8_t)g_sd_cs;
  SPI.begin(g_sd_sck, g_sd_miso, g_sd_mosi, SD_CS_PIN);
  SdSpiConfig config(SD_CS_PIN, SHARED_SPI, SD_SCK_MHZ(16));
  if (!sd.begin(config)) {
    g_sdMounted = false;
    g_sdMountMsg = "SD mount failed";
    return false;
  }
  g_sdMounted = true;
  g_sdMountMsg = "OK";
  return true;
}

void sdUnmount() {
  // SdFat doesn't have a universal "end" across all configs; treat as logical unmount.
  g_sdMounted = false;
}

bool sdExists(const char* path) { 
  if (!g_sdMounted && !sdMount()) return false; 
  return sd.exists(path); 
}

bool sdMkdir(const char* path) { 
  if (!g_sdMounted && !sdMount()) return false; 
  return sd.mkdir(path); 
}

bool sdRemove(const char* path) { 
  if (!g_sdMounted && !sdMount()) return false; 
  return sd.remove(path); 
}

bool sdRmdir(const char* path) { 
  if (!g_sdMounted && !sdMount()) return false; 
  return sd.rmdir(path); 
}

bool sdRename(const char* f, const char* t) { 
  if (!g_sdMounted && !sdMount()) return false; 
  return sd.rename(f, t); 
}

SDFile sdOpen(const char* path, oflag_t flags) {
  if (!g_sdMounted && !sdMount()) return SDFile();
  return sd.open(path, flags);
}

bool sdIsDir(const char* path) {
  SDFile f = sdOpen(path, O_RDONLY);
  if (!f) return false;
  bool isDir = f.isDir();
  f.close();
  return isDir;
}

bool sdGetStats(uint64_t &totalBytes, uint64_t &usedBytes) {
  totalBytes = 0; usedBytes = 0;
  if (!g_sdMounted && !sdMount()) return false;
  if (!sd.vol()) return false;
  uint32_t cCount = sd.vol()->clusterCount();
  uint32_t spc = sd.vol()->blocksPerCluster();
  uint32_t bps = 512;
  totalBytes = (uint64_t)cCount * (uint64_t)spc * (uint64_t)bps;
  uint32_t freeClusters = sd.vol()->freeClusterCount();
  uint64_t freeBytes = (uint64_t)freeClusters * (uint64_t)spc * (uint64_t)bps;
  usedBytes = totalBytes - freeBytes;
  return true;
}

bool sdFileIsDir(SDFile &f) {
  return f.isDir();
}

String sdFileName(SDFile &f) {
  char nm[96];
  nm[0]=0;
  f.getName(nm, sizeof(nm));
  return String(nm);
}

bool sdOpenNext(SDFile &dir, SDFile &out) {
  SDFile tmp;
  if (!tmp.openNext(&dir, O_RDONLY)) return false;
  out = tmp;
  return (bool)out;
}

bool sdEnsureMounted() {
  return sdMount();
}

bool sdCopyToLittleFS(const char* sd_path, const char* lfs_path) {
  SDFile src = sdOpen(sd_path, O_RDONLY);
  if (!src) return false;
  
  File dst = LittleFS.open(lfs_path, "w");
  if (!dst) {
    src.close();
    return false;
  }
  
  uint8_t buf[512];
  while (src.available()) {
    size_t n = src.read(buf, sizeof(buf));
    if (n > 0) dst.write(buf, n);
  }
  
  src.close();
  dst.close();
  return true;
}

bool sdGetUsage(uint64_t &total, uint64_t &used) {
  return sdGetStats(total, used);
}

#endif // SD_SUPPORT_ENABLED
