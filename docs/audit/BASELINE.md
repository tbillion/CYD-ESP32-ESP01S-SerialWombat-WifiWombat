# Baseline Assessment Report

**Date:** 2026-01-24  
**Repository:** tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat  
**Commit:** 9915ef6 (HEAD)

## Executive Summary

This is an **Arduino/ESP32 IoT firmware project** for CYD (Cheap Yellow Display) hardware that provides a web-based interface for managing Serial Wombat I2C devices with display support via LovyanGFX and LVGL.

**Critical Finding:** This is a single-file Arduino sketch (4256 lines) with NO existing security hardening, NO CI/CD, NO tests, NO documentation, and NO dependency management.

---

## 1. Repository Inventory

### 1.1 Language & Framework
- **Primary Language:** C++ (Arduino framework)
- **Target Platform:** ESP32 (ESP32-S3 variants supported)
- **Build System:** Arduino IDE / arduino-cli / PlatformIO
- **Framework:** Arduino Core for ESP32 v3.x

### 1.2 Codebase Structure
```
.
├── CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino  (4256 lines - main application)
├── cyd_predecls.h                                        (51 lines - forward declarations)
├── patch1.patch                                          (SdFat integration patch)
└── .git/                                                 (version control)
```

**Key Observations:**
- **Single monolithic file:** All code in one .ino file (poor modularity)
- **No directory structure:** No src/, lib/, test/, or docs/ directories
- **No build configuration:** No platformio.ini, arduino-cli.yaml, or CMakeLists.txt
- **No documentation:** No README, LICENSE, or usage guide
- **No CI/CD:** No .github/workflows/ directory
- **No tests:** No test framework or test files

### 1.3 Entrypoints
- **Main Application:** `CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino`
  - `setup()` at line 4110
  - `loop()` at line 4234
- **Web Server:** HTTP server on port 80
- **TCP Bridge:** TCP server on configurable port (default 23)
- **OTA Updates:** ArduinoOTA enabled

### 1.4 Dependencies (from #include statements)

#### Core ESP32 Libraries (built-in)
- Arduino.h
- WiFi.h
- WebServer.h
- ArduinoOTA.h
- Wire.h (I2C)
- FS.h / LittleFS.h
- SPI.h

#### External Libraries (require manual installation)
1. **WiFiManager** - WiFi credential management
2. **ArduinoJson** - JSON parsing/serialization
3. **LovyanGFX** - Display driver abstraction
4. **LVGL** (v8 or v9) - GUI framework
5. **SdFat** - SD card file system
6. **SerialWombat** - Hardware abstraction library for Serial Wombat devices
   - SerialWombatAnalogInput
   - SerialWombatDebouncedInput
   - SerialWombatHBridge
   - SerialWombatPWM
   - SerialWombatPulseTimer
   - SerialWombatQuadEnc
   - SerialWombatServo
   - SerialWombatTM1637
   - SerialWombatUltrasonicDistanceSensor

**Dependency Management Issues:**
- ❌ No library.json or library.properties
- ❌ No version pinning
- ❌ No lockfile
- ❌ No vulnerability scanning
- ❌ No license documentation

### 1.5 Configuration Files
- **Runtime Config:** `/config.json` (stored in LittleFS)
  - System configuration (I2C pins, display settings, WiFi, etc.)
  - Created at first boot with defaults
- **No .env files** (configuration is JSON-based)
- **No YAML/INI configs**

### 1.6 CI/CD Status
- ❌ **NO CI/CD PRESENT**
- No GitHub Actions workflows
- No GitLab CI, CircleCI, Travis, Jenkins, etc.
- No automated builds
- No automated tests
- No security scanning
- No code quality checks

---

## 2. Build & Test Methods

### 2.1 Build System Status

**Expected Build Methods:**
1. **Arduino IDE:** Manual compilation via GUI
2. **arduino-cli:** Command-line compilation
3. **PlatformIO:** Alternative build system with platformio.ini

**Current State:**
- ❌ No build configuration files present
- ❌ Arduino CLI unavailable in environment (network restrictions)
- ❌ Cannot execute baseline build without:
  - Board definition (esp32:esp32:esp32s3 or similar)
  - Library installation
  - Proper toolchain setup

**Recommendation:** Add platformio.ini or arduino-cli.yaml with full dependency specification.

### 2.2 Test Infrastructure
- ❌ **NO TESTS PRESENT**
- No unit tests
- No integration tests
- No test framework (no Unity, no ArduinoUnit, no PlatformIO native)
- No mocking framework
- No CI test runs

### 2.3 Linting & Formatting
- ❌ No linter configuration (no .clang-format, .cpplint)
- ❌ No code formatting enforcement
- ❌ No pre-commit hooks
- ❌ No static analysis (no cppcheck, clang-tidy, etc.)

### 2.4 Type Checking
- **N/A for C++** (compiled language with type checking at compile time)
- No additional static analysis configured

---

## 3. Baseline Runtime Analysis

### 3.1 Functionality Overview

**Core Features Identified:**
1. **WiFi Management**
   - WiFiManager-based captive portal setup
   - AP mode fallback (SSID: "Wombat-Setup")
   - WiFi reset capability

2. **Web Interface (HTTP Server on port 80)**
   - Dashboard UI at `/`
   - I2C scanner at `/scanner`, `/scan-data`, `/deepscan`
   - Device management: `/connect`, `/setpin`, `/changeaddr`
   - Firmware flash: `/flashfw`, `/upload_fw`, `/upload_hex`
   - System operations: `/resetwombat`, `/formatfs`, `/resetwifi`
   - Configuration UI: `/configure`, `/settings`
   - API endpoints under `/api/...`

3. **SD Card Support (optional, compile-time)**
   - File browser and management
   - Firmware import from SD
   - Upload/download files
   - Uses SdFat library

4. **Display Support (optional, compile-time)**
   - LovyanGFX driver for multiple panels (ILI9341, ST7789, ST7796, RGB)
   - LVGL GUI with status bar, WiFi/I2C indicators
   - Touch support (XPT2046, GT911)

5. **Serial Wombat I2C Device Management**
   - Scan and connect to Serial Wombat devices
   - Flash firmware to devices
   - Bridge I2C to TCP

6. **OTA Updates**
   - ArduinoOTA enabled for wireless firmware updates

### 3.2 Attack Surface (Initial Assessment)

**Network Attack Vectors:**
1. **Unauthenticated HTTP endpoints** - ALL endpoints accessible without auth
2. **OTA Updates** - No password/authentication visible in code
3. **TCP Bridge** - Arbitrary TCP connections allowed
4. **WiFi AP mode** - Open AP "Wombat-Setup" with no password

**File System Attack Vectors:**
1. SD card file operations (upload, download, delete, rename)
2. LittleFS filesystem access
3. Path traversal risks in `/sd/download` and file handlers
4. Arbitrary firmware upload and flash

**I2C/Hardware Attack Vectors:**
1. Arbitrary I2C device access
2. Direct hardware pin manipulation via web API
3. Firmware flashing to connected devices

**Code Execution Vectors:**
1. Firmware upload allows arbitrary code execution on target device
2. OTA allows arbitrary firmware on ESP32 itself
3. No input validation on filenames, paths, or parameters

### 3.3 Security Issues Identified

#### HIGH SEVERITY
1. **No Authentication/Authorization** - ALL endpoints open to anyone on network
2. **No HTTPS/TLS** - All traffic in cleartext
3. **No CORS policy** - Web UI vulnerable to CSRF
4. **No request size limits** - DoS via large uploads
5. **No rate limiting** - Brute force and DoS attacks possible
6. **Path traversal risks** - File download/upload endpoints
7. **OTA with no authentication** - Remote code execution

#### MEDIUM SEVERITY
1. **No input validation** - Parameters not sanitized
2. **No secrets management** - WiFi credentials stored in plaintext
3. **No logging** - No security event logging
4. **No firmware signing** - Malicious firmware can be flashed
5. **Open WiFi AP** - "Wombat-Setup" has no password

#### LOW SEVERITY
1. **No security headers** - Missing HSTS, X-Frame-Options, CSP
2. **No timeout on operations** - Long-running tasks can hang
3. **No error message sanitization** - Stack traces may leak info

---

## 4. Baseline Test Results

### 4.1 Build Test
```
STATUS: ❌ CANNOT BUILD
REASON: Build environment unavailable (arduino-cli installation failed, network restrictions)
NEXT STEPS: Add platformio.ini with board + library specifications
```

### 4.2 Unit Tests
```
STATUS: ❌ NO TESTS EXIST
COVERAGE: 0%
NEXT STEPS: Add test framework (PlatformIO native testing)
```

### 4.3 Lint/Format
```
STATUS: ❌ NO LINTER CONFIGURED
NEXT STEPS: Add clang-format configuration
```

### 4.4 Type Check
```
STATUS: N/A (C++ is statically typed)
```

### 4.5 Runtime Smoke Test
```
STATUS: ❌ CANNOT RUN (requires ESP32 hardware)
ALTERNATIVE: Static analysis and code review performed
```

---

## 5. Immediate Priorities (Before Hardening)

### Critical Path Items
1. ✅ **Document baseline state** (this document)
2. 🔄 **Create build configuration** (platformio.ini or arduino-cli.yaml)
3. 🔄 **Add dependency manifest** with versions
4. 🔄 **Create CI pipeline** for builds and security scans
5. 🔄 **Implement authentication** on all HTTP endpoints
6. 🔄 **Add input validation** on all user inputs
7. 🔄 **Fix path traversal** vulnerabilities

### Structural Improvements Needed
1. Break monolithic .ino into modular files
2. Add proper directory structure (src/, include/, lib/, test/, docs/)
3. Add configuration management (platformio.ini)
4. Add README and documentation
5. Add tests (unit + integration)
6. Add CI/CD pipeline

---

## 6. Dependency Audit (Manual Review)

### Library Risk Assessment

| Library | Risk Level | Concerns |
|---------|-----------|----------|
| WiFiManager | MEDIUM | No version pinned, credential storage |
| ArduinoJson | LOW | Well-maintained, but no version pinned |
| LovyanGFX | MEDIUM | Large codebase, display driver vulnerabilities |
| LVGL | MEDIUM | GUI framework, potential memory issues |
| SdFat | MEDIUM | File system operations, path traversal risks |
| SerialWombat | LOW | Custom hardware library |
| Arduino Core ESP32 | MEDIUM | Core framework, ensure v3.x usage |

**Action Required:**
- Pin all library versions
- Run vulnerability scans (Dependabot, OSV-Scanner)
- Review each library's security advisories
- Document acceptable versions

---

## 7. Compliance & Licensing

### Current State
- ❌ **No LICENSE file**
- ❌ **No copyright notices**
- ❌ **No dependency license documentation**
- ❌ **No SBOM (Software Bill of Materials)**

### Requirements
1. Add LICENSE file (MIT, Apache 2.0, GPL, or proprietary)
2. Document all dependency licenses
3. Generate SBOM for supply chain transparency
4. Add copyright headers to source files

---

## 8. Conclusion

**Baseline Summary:**
- **Build Status:** ❌ Cannot build (no configuration)
- **Test Status:** ❌ No tests exist
- **Security Posture:** ❌ Critical vulnerabilities present
- **Documentation:** ❌ None present
- **CI/CD:** ❌ Not present

**Risk Assessment:** 🔴 **HIGH RISK**
- No authentication on network-accessible device
- Multiple code execution vectors
- No security testing or validation

**Next Phase:** Proceed to Phase 1 (Security & Hardening Plan)

---

## Appendix A: Code Statistics

```
File: CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino
Lines: 4256
Size: 143,578 bytes
Includes: 18 libraries
Functions: ~60+ (estimated from code review)
Global Variables: ~30+
Configuration Options: ~50+ fields in SystemConfig struct
HTTP Endpoints: 25+
```

## Appendix B: Feature Flags

```c
#define SD_SUPPORT_ENABLED 1           // Compile-time SD support
#define DISPLAY_SUPPORT_ENABLED 1      // Compile-time display support
#define DEFAULT_DISPLAY_ENABLE 1       // Runtime display enable
#define DEFAULT_TOUCH_ENABLE 1         // Runtime touch enable
#define DEFAULT_LVGL_ENABLE 1          // Runtime LVGL enable
#define CYD_USE_SDFAT 1               // Use SdFat instead of SD.h
```

## Appendix C: Hardware Pin Definitions

```c
// SD Card (SPI mode)
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18

// Battery monitoring
#define BATTERY_ADC_PIN -1  // Disabled by default

// Display, touch, I2C pins are runtime-configurable via config.json
```
