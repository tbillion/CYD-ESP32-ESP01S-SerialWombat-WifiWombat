# Boot Sequence Specification

## Overview

The boot sequence is implemented as a PLC-style startup procedure where every stage emits standardized messages (INFO/WARN/ERROR) and updates a BootSummary that tracks overall boot health. This document defines each boot stage, success/warning/failure criteria, and operator actions.

## Boot Stages

### Stage 0: BOOT_START
**Code**: `BOOT_START`  
**Message**: "System boot initiated"  
**Timing**: First thing after power-on  
**Severity**: INFO  
**Purpose**: Mark beginning of boot sequence  
**Failure Mode**: N/A (cannot fail)  
**Operator Action**: None (informational)

---

### Stage 1: BOOT_01_EARLY_INIT
**Code**: `BOOT_01_EARLY_BEGIN`, `BOOT_01_EARLY_OK`  
**Message**: "Early initialization" → "Serial and core systems ready"  
**Components**:
- Serial console @ 115200 baud
- Core system variables
- Watchdog timer setup (if enabled)

**Success Criteria**: Serial.begin() succeeds  
**Warning Conditions**: None  
**Failure Conditions**: N/A (Serial init rarely fails)  
**Operator Action**: None

---

### Stage 2: BOOT_02_CONFIG_LOAD
**Codes**: `BOOT_02_CONFIG_BEGIN`, `BOOT_02_CONFIG_OK`, `BOOT_02_CONFIG_WARN`, `BOOT_02_CONFIG_FAIL`

**Messages**:
- BEGIN: "Loading system configuration"
- OK: "Configuration loaded successfully"
- WARN: "Config file missing, using defaults"
- FAIL: "Config parse error, cannot continue"

**Components**:
- Load `/config.json` from LittleFS
- Parse JSON
- Apply defaults if missing
- Validate config structure

**Success Criteria**: Config loaded and validated  
**Warning Conditions**:
- Config file missing → use defaults, emit WARN `BOOT_02_CONFIG_WARN`
- Non-critical fields invalid → use defaults for those fields only

**Failure Conditions**:
- Config exists but JSON is malformed → emit ERROR `BOOT_02_CONFIG_FAIL`
- Critical fields invalid (e.g., display_type out of range) → emit ERROR

**Recovery**:
- WARN: System continues with defaults; operator should configure via UI
- ERROR: System may enter safe mode or continue with minimal defaults; operator must fix/delete config.json

**Operator Action**:
- WARN: Review configuration in Settings UI
- ERROR: Delete config.json via web UI or serial, reboot

---

### Stage 3: BOOT_03_FILESYSTEM_LFS
**Codes**: `BOOT_03_FS_BEGIN`, `BOOT_03_FS_OK`, `BOOT_03_FS_FAIL`, `FS_LFS_FORMAT_BEGIN`, `FS_LFS_FORMAT_OK`

**Messages**:
- BEGIN: "Mounting LittleFS"
- OK: "LittleFS mounted successfully"
- FORMAT_BEGIN: "Formatting LittleFS (first boot or corrupted)"
- FORMAT_OK: "LittleFS formatted and mounted"
- FAIL: "LittleFS mount and format failed"

**Components**:
- Mount LittleFS with auto-format on first failure
- Create required directories: `/fw`, `/config`, `/temp`, `/hexcache`

**Success Criteria**: LittleFS mounted and directories created  
**Warning Conditions**: Auto-format triggered (WARN `FS_LFS_FORMAT_BEGIN` → INFO `FS_LFS_FORMAT_OK`)  
**Failure Conditions**: Format fails → ERROR `BOOT_03_FS_FAIL`

**Critical**: YES - Without LittleFS, system cannot persist config, firmware, or messages  
**Recovery**: If failure, system may run in RAM-only mode (no persistence)  
**Operator Action**:
- WARN (format): None, informational
- ERROR: Check flash integrity; may require hardware repair

---

### Stage 4: BOOT_04_SD_MOUNT
**Codes**: `BOOT_04_SD_BEGIN`, `BOOT_04_SD_OK`, `BOOT_04_SD_NOT_PRESENT`, `BOOT_04_SD_FAIL`

**Messages**:
- BEGIN: "Checking for SD card"
- OK: "SD card mounted successfully"
- NOT_PRESENT: "No SD card detected, continuing without SD"
- FAIL: "SD card present but mount failed"

**Components**:
- Detect SD card presence
- Mount SD (FAT32) if present
- Initialize SdFat library

**Success Criteria**: SD mounted (if present)  
**Warning Conditions**:
- No SD card → WARN `BOOT_04_SD_NOT_PRESENT` (not critical, can continue)
- SD present but mount fails → WARN `BOOT_04_SD_FAIL` (format issue, continue without SD)

**Failure Conditions**: None (SD is optional)  
**Critical**: NO - System can operate without SD  
**Operator Action**:
- NOT_PRESENT: Insert SD card if firmware storage/logging needed
- FAIL: Format SD card as FAT32, or replace card

---

### Stage 5: BOOT_05_DISPLAY_INIT
**Codes**: `BOOT_05_DISPLAY_BEGIN`, `BOOT_05_DISPLAY_OK`, `BOOT_05_DISPLAY_DISABLED`, `BOOT_05_DISPLAY_FAIL`

**Messages**:
- BEGIN: "Initializing display"
- OK: "Display ready"
- DISABLED: "Display disabled (headless mode)"
- FAIL: "Display initialization failed"

**Components**:
- Check if display enabled in config
- Initialize LovyanGFX panel
- Initialize LVGL (v8/v9)
- Allocate framebuffer (PSRAM or internal RAM)
- Set up partial rendering

**Success Criteria**: Display initialized, LVGL ready, `g_lvgl_ready = true`  
**Warning Conditions**:
- Display disabled in config → WARN `BOOT_05_DISPLAY_DISABLED` (operator chose headless)

**Failure Conditions**:
- Display hardware not responding → ERROR `BOOT_05_DISPLAY_FAIL`
- Framebuffer allocation failed → ERROR `DISP_FB_ALLOC_FAIL`

**Critical**: SEMI - If display fails, system can run headless (web UI only)  
**Recovery**: System continues in headless mode; messages accessible via web UI  
**Operator Action**:
- DISABLED: Enable display in config if UI needed
- FAIL: Check display connections (SPI wiring, power); access via web UI

---

### Stage 6: BOOT_06_TOUCH_INIT
**Codes**: `BOOT_06_TOUCH_BEGIN`, `BOOT_06_TOUCH_OK`, `BOOT_06_TOUCH_FAIL`, `BOOT_06_TOUCH_CAL_REQ`

**Messages**:
- BEGIN: "Initializing touch controller"
- OK: "Touch controller ready"
- FAIL: "Touch controller not found"
- CAL_REQ: "Touch calibration required"

**Components**:
- Initialize touch controller (capacitive or XPT2046 resistive)
- Load calibration data
- Register LVGL input device

**Success Criteria**: Touch controller responds, calibration data valid  
**Warning Conditions**:
- Touch controller not found → WARN `BOOT_06_TOUCH_FAIL` (can use mouse/web, continue)
- First boot, no calibration → WARN `BOOT_06_TOUCH_CAL_REQ` (run wizard)

**Failure Conditions**: None (touch is not critical)  
**Critical**: NO - System usable with web UI or USB mouse  
**Operator Action**:
- FAIL: Check touch controller connections; use web UI or USB mouse
- CAL_REQ: Run touch calibration wizard (auto-starts on first boot)

---

### Stage 7: BOOT_07_NETWORK_WIFI
**Codes**: `BOOT_07_NET_BEGIN`, `BOOT_07_NET_OK`, `BOOT_07_NET_AP_FALLBACK`, `BOOT_07_NET_FAIL`

**Messages**:
- BEGIN: "Connecting to WiFi"
- OK: "WiFi connected: <SSID>"
- AP_FALLBACK: "WiFi failed, AP mode active: 'Wombat-Setup'"
- FAIL: "WiFi initialization failed"

**Components**:
- WiFiManager auto-connect (180s timeout)
- Fallback to AP mode "Wombat-Setup"
- Set WiFi to no-sleep for responsiveness

**Success Criteria**: Connected to WiFi, IP assigned  
**Warning Conditions**:
- Auto-connect fails, AP mode enabled → WARN `BOOT_07_NET_AP_FALLBACK`

**Failure Conditions**:
- WiFi hardware failure → ERROR `BOOT_07_NET_FAIL` (rare)

**Critical**: SEMI - Without WiFi, no web UI or OTA; system can run standalone  
**Recovery**: AP mode allows configuration; system functional for local I2C operations  
**Operator Action**:
- AP_FALLBACK: Connect to "Wombat-Setup" AP and configure WiFi
- FAIL: Check ESP32 WiFi hardware

---

### Stage 8: BOOT_08_TIME_SYNC
**Codes**: `BOOT_08_TIME_BEGIN`, `BOOT_08_TIME_OK`, `BOOT_08_TIME_FAIL`

**Messages**:
- BEGIN: "Synchronizing time via NTP"
- OK: "Time synchronized"
- FAIL: "NTP sync failed, using local time"

**Components**:
- NTP client (pool.ntp.org)
- Set system RTC

**Success Criteria**: Time synced with NTP server  
**Warning Conditions**: NTP sync fails → WARN `BOOT_08_TIME_FAIL` (continue with local time)  
**Failure Conditions**: None (time sync is not critical)  
**Critical**: NO  
**Operator Action**: Check network connectivity; timestamps may be inaccurate

---

### Stage 9: BOOT_09_SERVICES_START
**Codes**: `BOOT_09_SERVICES_BEGIN`, `BOOT_09_SERVICES_OK`, `BOOT_09_WEB_FAIL`, `BOOT_09_OTA_FAIL`

**Messages**:
- BEGIN: "Starting services (Web, OTA, TCP)"
- SERVICES_OK: "All services started"
- WEB_FAIL: "Web server failed to start"
- OTA_FAIL: "OTA service failed to start"

**Components**:
- Web server (port 80)
- OTA service
- TCP bridge (port 3000)
- SerialWombat initialization

**Success Criteria**: All services started  
**Warning Conditions**: OTA fails → WARN `BOOT_09_OTA_FAIL` (continue without OTA)  
**Failure Conditions**: Web server fails → ERROR `BOOT_09_WEB_FAIL` (no web UI access)  
**Critical**: Web server is critical for configuration; OTA is not  
**Recovery**: Web fail → use serial console and LVGL UI  
**Operator Action**: Check port availability (80, 3000)

---

### Stage 10: BOOT_10_SELFTESTS (Optional)
**Codes**: `BOOT_10_SELFTEST_BEGIN`, `BOOT_10_SELFTEST_OK`, `BOOT_10_SELFTEST_FAIL`

**Messages**:
- BEGIN: "Running self-tests"
- OK: "Self-tests passed"
- FAIL: "Self-test failures detected"

**Components** (if implemented):
- Memory test
- I2C bus test
- Flash integrity test

**Success Criteria**: All tests pass  
**Warning Conditions**: Some tests fail → WARN `BOOT_10_SELFTEST_FAIL`  
**Failure Conditions**: Critical test fails → ERROR  
**Critical**: NO (optional feature)  
**Operator Action**: Review test results in Messages

---

### Stage 11: BOOT_OK_READY
**Code**: `BOOT_OK_READY`, `BOOT_DEGRADED`

**Messages**:
- BOOT_OK_READY: "System ready for operation"
- BOOT_DEGRADED: "System operational with warnings"

**Conditions**:
- **BOOT_OK_READY** (INFO): All critical stages passed, no ERROR messages
- **BOOT_DEGRADED** (WARN): System functional but with WARN messages (e.g., no SD, no touch, AP mode)

**Operator Action**:
- BOOT_OK_READY: None, acknowledge when reviewed
- BOOT_DEGRADED: Review active messages, address warnings if needed

---

## Boot Flow Summary

```
BOOT_START (INFO)
  ↓
BOOT_01_EARLY_INIT (INFO)
  ↓
BOOT_02_CONFIG_LOAD
  ├─ OK (INFO): Config loaded
  ├─ WARN: Config missing, defaults used
  └─ FAIL (ERROR): Config parse error → Safe mode
  ↓
BOOT_03_FILESYSTEM_LFS
  ├─ OK (INFO): LittleFS mounted
  ├─ FORMAT (WARN→INFO): Auto-formatted
  └─ FAIL (ERROR): Cannot mount → RAM-only mode
  ↓
BOOT_04_SD_MOUNT
  ├─ OK (INFO): SD mounted
  ├─ NOT_PRESENT (WARN): No SD, continue
  └─ FAIL (WARN): Mount failed, continue
  ↓
BOOT_05_DISPLAY_INIT
  ├─ OK (INFO): Display ready
  ├─ DISABLED (WARN): Headless mode
  └─ FAIL (ERROR): Display hardware issue → Headless
  ↓
BOOT_06_TOUCH_INIT
  ├─ OK (INFO): Touch ready
  ├─ FAIL (WARN): No touch, use web/mouse
  └─ CAL_REQ (WARN): Run calibration wizard
  ↓
BOOT_07_NETWORK_WIFI
  ├─ OK (INFO): WiFi connected
  ├─ AP_FALLBACK (WARN): AP mode active
  └─ FAIL (ERROR): WiFi hardware issue
  ↓
BOOT_08_TIME_SYNC
  ├─ OK (INFO): Time synced
  └─ FAIL (WARN): NTP failed, local time
  ↓
BOOT_09_SERVICES_START
  ├─ SERVICES_OK (INFO): All services up
  ├─ WEB_FAIL (ERROR): No web UI
  └─ OTA_FAIL (WARN): No OTA
  ↓
BOOT_10_SELFTESTS (optional)
  ├─ OK (INFO): Tests passed
  └─ FAIL (WARN): Some tests failed
  ↓
BOOT_READY
  ├─ BOOT_OK_READY (INFO): All clear
  └─ BOOT_DEGRADED (WARN): Warnings present
```

## Critical vs. Non-Critical Stages

### Critical (ERROR stops normal operation)
1. **LittleFS**: Without it, no persistence → RAM-only mode
2. **Display** (if not headless): Without it, no local UI → Force headless
3. **Web Server**: Without it, no remote config → Serial-only

### Non-Critical (WARN but continue)
1. **SD Card**: Optional storage
2. **Touch**: Can use mouse or web UI
3. **WiFi**: Can run standalone for local I2C ops
4. **NTP**: Can use local time
5. **OTA**: Can update via serial

## Failure Recovery Matrix

| Stage | Failure Severity | System Mode | Operator Access |
|-------|------------------|-------------|-----------------|
| Config parse | ERROR | Safe defaults | Web + LVGL + Serial |
| LittleFS | ERROR | RAM-only | Web + LVGL + Serial (no persistence) |
| Display | ERROR | Headless | Web + Serial |
| Touch | WARN | Display + web | Web + Display (no touch input) |
| WiFi | WARN | AP mode | AP web portal + LVGL + Serial |
| Web server | ERROR | Local only | LVGL + Serial |

## Boot Progress UI

### LVGL Boot Screen
- Show progress bar or stage list
- Display current stage name
- Color-code stages: ✓ green (ok), ⚠ yellow (warn), ✗ red (fail)
- Auto-close on success, stay open if WARN/ERROR

### Web UI Boot Indicator
- Loading spinner during boot
- Redirect to /messages if WARN/ERROR present
- Show boot summary in dashboard

## Post-Boot Behavior

1. **Boot Success (no WARN/ERROR)**:
   - Auto-transition to normal UI
   - Optionally show "Boot complete" toast

2. **Boot with WARN**:
   - Show Messages screen with badge
   - Operator can acknowledge and continue
   - System is operational

3. **Boot with ERROR**:
   - Force Messages screen to front
   - Badge shows ERROR count
   - Operator must acknowledge and address issues
   - System may be degraded

## Message Examples

### Successful Boot (All OK)
```
[INFO] BOOT_START: System boot initiated
[INFO] BOOT_01_EARLY_OK: Serial and core systems ready
[INFO] BOOT_02_CONFIG_OK: Configuration loaded successfully
[INFO] BOOT_03_FS_OK: LittleFS mounted successfully
[INFO] BOOT_04_SD_OK: SD card mounted successfully
[INFO] BOOT_05_DISPLAY_OK: Display ready
[INFO] BOOT_06_TOUCH_OK: Touch controller ready
[INFO] BOOT_07_NET_OK: WiFi connected: MyNetwork
[INFO] BOOT_08_TIME_OK: Time synchronized
[INFO] BOOT_09_SERVICES_OK: All services started
[INFO] BOOT_OK_READY: System ready for operation
```

### Degraded Boot (WARNs present)
```
[INFO] BOOT_START: System boot initiated
...
[WARN] BOOT_04_SD_NOT_PRESENT: No SD card detected, continuing without SD
[WARN] BOOT_06_TOUCH_FAIL: Touch controller not found
[INFO] BOOT_07_NET_OK: WiFi connected: MyNetwork
...
[WARN] BOOT_DEGRADED: System operational with warnings (2 warnings)
```

### Failed Boot (ERROR present)
```
[INFO] BOOT_START: System boot initiated
...
[ERROR] BOOT_05_DISPLAY_FAIL: Display initialization failed (timeout)
[WARN] BOOT_05_DISPLAY_DISABLED: Entering headless mode
[INFO] BOOT_07_NET_OK: WiFi connected: MyNetwork
...
[WARN] BOOT_DEGRADED: System operational in headless mode (1 error, 1 warning)
```

## Debugging Boot Issues

1. **Serial Console**: All boot messages also logged to Serial @ 115200
2. **Web API**: GET /api/messages/active shows boot messages
3. **Persistent History**: Boot messages saved to history for post-mortem analysis
4. **Boot Gauntlet**: Dev mode can simulate failures for testing

## References

- Message Origins Matrix: `ORIGINS_MATRIX.md`
- System Specification: `SPEC.md`
- Verification Plan: `GAUNTLET.md`
