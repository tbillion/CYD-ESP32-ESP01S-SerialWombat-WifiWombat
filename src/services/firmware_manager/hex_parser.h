#pragma once

#include <Arduino.h>

#include <FS.h>

// ===================================================================================
// IntelHexSW8B - embedded library (single-file integration)
// Source: IntelHexSW8B (zip provided)
// ===================================================================================
class IntelHexSW8B {
 public:
  IntelHexSW8B();

  // Initialize with a filesystem (LittleFS or SD). Creates cacheDir if needed.
  // Cache is flash-backed so it works with or without PSRAM.
  bool begin(fs::FS& fs, const char* cacheDir = "/hexcache");

  // Clear cache files (data + validity) and warnings/bounds.
  void clearCache();

  // Load and parse an Intel HEX file from the filesystem.
  // Supports record types: 00 (data), 01 (EOF), 04 (extended linear address).
  // If enforceChecksum is true, lines with invalid checksum are ignored (like the original C#
  // code).
  bool loadHexFile(const char* hexPath, bool enforceChecksum = false);

  // Bounds observed during parse (highest/lowest written absolute byte address).
  bool hasBounds() const { return _boundsSet; }
  uint32_t minAddress() const { return _minAddr; }
  uint32_t maxAddress() const { return _maxAddr; }

  // Warnings/errors collected (duplicate addresses, out-of-range writes, missing bytes in strict
  // export, etc.)
  const String& warnings() const { return _warnings; }

  // Strict export for CH32V003 16KB firmware window.
  // - Fixed range: [0x00000000, 0x00004000)
  // - Little-endian uint16 packing: word = b0 | (b1<<8)
  // - Output: 0xXXXX comma-separated, no whitespace/comments.
  // - STRICT: If any byte in the 16KB window is missing, export fails and warnings() will report
  // the first missing address.
  bool exportFW_CH32V003_16K_Strict(const char* outPath, bool trailingComma = true,
                                    bool newlineAtEnd = false);

  // Optional: CRC16-CCITT over a byte range, treating missing bytes as an error when strict=true.
  // This mirrors the original algorithm (poly 0x1021, init 0xFFFF).
  // If strict is true and any byte is missing, returns 0 and appends an error to warnings().
  uint16_t crc16ccitt(uint32_t start, uint32_t exclusiveEnd, bool strict = true,
                      uint8_t fillValue = 0xFF);

 private:
  fs::FS* _fs = nullptr;
  String _cacheDir;
  String _dataPath;
  String _validPath;

  bool _boundsSet = false;
  uint32_t _minAddr = 0;
  uint32_t _maxAddr = 0;

  String _warnings;

  static constexpr uint32_t PAGE_SIZE = 256;
  static constexpr uint32_t VALID_BYTES = 32;  // 256 bits => 32 bytes

  bool ensureCacheFiles_();

  bool readPageValid_(uint32_t pageIndex, uint8_t* valid32);
  bool writePageValid_(uint32_t pageIndex, const uint8_t* valid32);

  bool readPageData_(uint32_t pageIndex, uint8_t* data256);
  bool writePageData_(uint32_t pageIndex, const uint8_t* data256);

  bool setByte_(uint32_t addr, uint8_t value);
  bool getByte_(uint32_t addr, uint8_t& value, bool& isValid);

  // HEX parsing helpers
  static inline uint8_t hexNibble_(char c);
  static bool parseHexByte_(const char* s, uint8_t& out);
  static bool parseHexU16_(const char* s, uint16_t& out);

  bool parseLine_(const String& raw, bool enforceChecksum, uint32_t& extHigh);
};
