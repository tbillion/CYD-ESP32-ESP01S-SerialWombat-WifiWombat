#include "hex_parser.h"

IntelHexSW8B::IntelHexSW8B() {}

static bool fileWriteAt_(fs::FS& fs, const char* path, uint32_t offset, const uint8_t* buf,
                         uint32_t len) {
  File f = fs.open(path, "r+");
  if (!f) f = fs.open(path, "w+");
  if (!f) return false;
  if (!f.seek(offset)) {
    f.close();
    return false;
  }
  size_t w = f.write(buf, len);
  f.close();
  return (w == len);
}

bool IntelHexSW8B::begin(fs::FS& fs, const char* cacheDir) {
  _fs = &fs;
  _cacheDir = cacheDir;
  if (!_cacheDir.startsWith("/")) _cacheDir = "/" + _cacheDir;

  _dataPath = _cacheDir + "/data.bin";
  _validPath = _cacheDir + "/valid.bin";

  _warnings = "";
  _boundsSet = false;
  _minAddr = 0;
  _maxAddr = 0;

  _fs->mkdir(_cacheDir.c_str());
  return ensureCacheFiles_();
}

bool IntelHexSW8B::ensureCacheFiles_() {
  if (!_fs) return false;
  if (!_fs->exists(_dataPath.c_str())) {
    File f = _fs->open(_dataPath.c_str(), "w");
    if (!f) return false;
    f.close();
  }
  if (!_fs->exists(_validPath.c_str())) {
    File f = _fs->open(_validPath.c_str(), "w");
    if (!f) return false;
    f.close();
  }
  return true;
}

void IntelHexSW8B::clearCache() {
  if (!_fs) return;
  if (_fs->exists(_dataPath.c_str())) _fs->remove(_dataPath.c_str());
  if (_fs->exists(_validPath.c_str())) _fs->remove(_validPath.c_str());
  ensureCacheFiles_();
  _warnings = "";
  _boundsSet = false;
}

bool IntelHexSW8B::readPageValid_(uint32_t pageIndex, uint8_t* valid32) {
  memset(valid32, 0, VALID_BYTES);
  uint32_t off = pageIndex * VALID_BYTES;

  File f = _fs->open(_validPath.c_str(), "r");
  if (!f) return false;
  uint32_t sz = (uint32_t)f.size();
  if (off >= sz) {
    f.close();
    return true;
  }

  f.seek(off);
  int r = f.read(valid32, VALID_BYTES);
  f.close();
  if (r < 0) return false;
  if (r < (int)VALID_BYTES) memset(valid32 + r, 0, VALID_BYTES - r);
  return true;
}

bool IntelHexSW8B::writePageValid_(uint32_t pageIndex, const uint8_t* valid32) {
  uint32_t off = pageIndex * VALID_BYTES;
  return fileWriteAt_(*_fs, _validPath.c_str(), off, valid32, VALID_BYTES);
}

bool IntelHexSW8B::readPageData_(uint32_t pageIndex, uint8_t* data256) {
  // default to 0xFF for readability; validity bitmap determines "present".
  memset(data256, 0xFF, PAGE_SIZE);
  uint32_t off = pageIndex * PAGE_SIZE;

  File f = _fs->open(_dataPath.c_str(), "r");
  if (!f) return false;
  uint32_t sz = (uint32_t)f.size();
  if (off >= sz) {
    f.close();
    return true;
  }

  f.seek(off);
  int r = f.read(data256, PAGE_SIZE);
  f.close();
  if (r < 0) return false;
  if (r < (int)PAGE_SIZE) memset(data256 + r, 0xFF, PAGE_SIZE - r);
  return true;
}

bool IntelHexSW8B::writePageData_(uint32_t pageIndex, const uint8_t* data256) {
  uint32_t off = pageIndex * PAGE_SIZE;
  return fileWriteAt_(*_fs, _dataPath.c_str(), off, data256, PAGE_SIZE);
}

bool IntelHexSW8B::setByte_(uint32_t addr, uint8_t value) {
  uint32_t page = addr >> 8;
  uint32_t off = addr & 0xFF;

  uint8_t valid[VALID_BYTES];
  uint8_t data[PAGE_SIZE];

  if (!readPageValid_(page, valid)) return false;
  if (!readPageData_(page, data)) return false;

  uint8_t bit = 1u << (off & 7);
  uint32_t idx = off >> 3;
  if (valid[idx] & bit) {
    _warnings += "Warning: Address 0x";
    _warnings += String(addr, HEX);
    _warnings += " is defined multiple times\n";
  }

  data[off] = value;
  valid[idx] |= bit;

  if (!writePageData_(page, data)) return false;
  if (!writePageValid_(page, valid)) return false;

  if (!_boundsSet) {
    _boundsSet = true;
    _minAddr = addr;
    _maxAddr = addr;
  } else {
    if (addr < _minAddr) _minAddr = addr;
    if (addr > _maxAddr) _maxAddr = addr;
  }
  return true;
}

bool IntelHexSW8B::getByte_(uint32_t addr, uint8_t& value, bool& isValid) {
  uint32_t page = addr >> 8;
  uint32_t off = addr & 0xFF;

  uint8_t valid[VALID_BYTES];
  uint8_t data[PAGE_SIZE];

  if (!readPageValid_(page, valid)) return false;
  if (!readPageData_(page, data)) return false;

  uint8_t bit = 1u << (off & 7);
  uint32_t idx = off >> 3;
  isValid = (valid[idx] & bit) != 0;
  value = data[off];
  return true;
}

uint8_t IntelHexSW8B::hexNibble_(char c) {
  if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
  if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
  if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
  return 0xFF;
}

bool IntelHexSW8B::parseHexByte_(const char* s, uint8_t& out) {
  uint8_t hi = hexNibble_(s[0]);
  uint8_t lo = hexNibble_(s[1]);
  if (hi == 0xFF || lo == 0xFF) return false;
  out = (uint8_t)((hi << 4) | lo);
  return true;
}

bool IntelHexSW8B::parseHexU16_(const char* s, uint16_t& out) {
  uint8_t b0, b1;
  if (!parseHexByte_(s, b0)) return false;
  if (!parseHexByte_(s + 2, b1)) return false;
  out = (uint16_t)((b0 << 8) | b1);
  return true;
}

bool IntelHexSW8B::parseLine_(const String& raw, bool enforceChecksum, uint32_t& extHigh) {
  String line = raw;
  line.trim();
  if (line.endsWith("\r")) line.remove(line.length() - 1);

  // Remove whitespace inside line (like Regex.Replace("\\s*", ""))
  {
    String out;
    out.reserve(line.length());
    for (size_t i = 0; i < line.length(); i++) {
      char c = line[i];
      if (!isspace((unsigned char)c)) out += c;
    }
    line = out;
  }

  if (line.length() < 11) return true;
  if (line[0] != ':') return true;

  // Reject illegal chars
  for (size_t i = 1; i < line.length(); i++) {
    if (!isxdigit((unsigned char)line[i])) return true;
  }

  uint8_t len;
  if (!parseHexByte_(line.c_str() + 1, len)) return true;
  if ((uint32_t)line.length() != (uint32_t)(11 + (uint32_t)len * 2)) return true;

  uint16_t addr16;
  if (!parseHexU16_(line.c_str() + 3, addr16)) return true;

  uint8_t rectype;
  {
    uint8_t rt;
    if (!parseHexByte_(line.c_str() + 7, rt)) return true;
    rectype = rt;
  }

  uint8_t indicated;
  if (!parseHexByte_(line.c_str() + line.length() - 2, indicated)) return true;

  uint32_t sum = 0;
  for (size_t i = 1; i < line.length() - 2; i += 2) {
    uint8_t b;
    if (!parseHexByte_(line.c_str() + i, b)) return true;
    sum += b;
  }
  uint8_t calc = (uint8_t)((0x100 - (sum & 0xFF)) & 0xFF);

  if (enforceChecksum && (calc != indicated)) return true;

  if (rectype == 0x00) {
    const char* p = line.c_str() + 9;
    for (uint8_t i = 0; i < len; i++) {
      uint8_t b;
      if (!parseHexByte_(p + i * 2, b)) return true;
      uint32_t absAddr = (extHigh << 16) + (uint32_t)addr16 + i;

      // If targeting CH32V003 16KB strictly, we can warn on out-of-range writes
      // but still store them in cache. The strict exporter will refuse missing bytes,
      // and you can choose to fail early if desired.
      if (absAddr >= 0x00004000) {
        _warnings += "Warning: Write beyond 16KB window at 0x";
        _warnings += String(absAddr, HEX);
        _warnings += "\n";
      }

      if (!setByte_(absAddr, b)) return false;
    }
  } else if (rectype == 0x04) {
    if (len != 2) return true;
    uint16_t high;
    if (!parseHexU16_(line.c_str() + 9, high)) return true;
    extHigh = high;
  } else if (rectype == 0x01) {
    // EOF
  } else {
    // Ignore unsupported types (matches original approach)
    _warnings += "Warning: Unsupported record type 0x";
    _warnings += String(rectype, HEX);
    _warnings += " ignored\n";
  }

  return true;
}

bool IntelHexSW8B::loadHexFile(const char* hexPath, bool enforceChecksum) {
  if (!_fs) return false;
  if (!ensureCacheFiles_()) return false;

  File f = _fs->open(hexPath, "r");
  if (!f) return false;

  _warnings = "";
  _boundsSet = false;

  uint32_t extHigh = 0;
  while (f.available()) {
    String line = f.readStringUntil('\n');
    if (!parseLine_(line, enforceChecksum, extHigh)) {
      f.close();
      return false;
    }
  }
  f.close();
  return true;
}

bool IntelHexSW8B::exportFW_CH32V003_16K_Strict(const char* outPath, bool trailingComma,
                                                bool newlineAtEnd) {
  if (!_fs) return false;

  const uint32_t start = 0x00000000;
  const uint32_t end = 0x00004000;  // 16KB

  File out = _fs->open(outPath, "w");
  if (!out) return false;

  bool first = true;

  for (uint32_t a = start; a < end; a += 2) {
    uint8_t b0, b1;
    bool v0, v1;

    if (!getByte_(a, b0, v0)) {
      out.close();
      return false;
    }
    if (!getByte_(a + 1, b1, v1)) {
      out.close();
      return false;
    }

    if (!v0 || !v1) {
      out.close();
      uint32_t miss = !v0 ? a : (a + 1);
      _warnings += "ERROR: Missing byte at 0x";
      _warnings += String(miss, HEX);
      _warnings += " within required 16KB window\n";
      return false;
    }

    uint16_t w = (uint16_t)(b0 | ((uint16_t)b1 << 8));  // little-endian word packing

    char buf[8];
    snprintf(buf, sizeof(buf), "0x%04X", (unsigned)w);

    if (!first) out.print(",");
    first = false;
    out.print(buf);
  }

  if (trailingComma) out.print(",");
  if (newlineAtEnd) out.print("\n");
  out.close();
  return true;
}

uint16_t IntelHexSW8B::crc16ccitt(uint32_t start, uint32_t exclusiveEnd, bool strict,
                                  uint8_t fillValue) {
  uint16_t crc = 0xFFFF;
  for (uint32_t a = start; a < exclusiveEnd; a++) {
    uint8_t b;
    bool valid;
    if (!getByte_(a, b, valid)) {
      _warnings += "ERROR: Read failed at 0x";
      _warnings += String(a, HEX);
      _warnings += "\n";
      return 0;
    }
    if (!valid) {
      if (strict) {
        _warnings += "ERROR: Missing byte at 0x";
        _warnings += String(a, HEX);
        _warnings += " during CRC (strict)\n";
        return 0;
      }
      b = fillValue;
    }

    crc ^= (uint16_t)b << 8;
    for (int j = 0; j < 8; j++) {
      if (crc & 0x8000)
        crc = (uint16_t)((crc << 1) ^ 0x1021);
      else
        crc <<= 1;
    }
  }
  return crc;
}
