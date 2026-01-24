# Smoke Test Procedures

## Overview
This document provides step-by-step procedures to validate the refactored firmware functions correctly on the LCDWIKI 3.5" ESP32-32E display board and other supported hardware.

## Prerequisites

### Hardware Required
- ESP32 development board (LCDWIKI 3.5" ESP32-32E recommended, or CYD 2.8"/7")
- USB cable for programming and serial monitor
- MicroSD card (optional, FAT32 formatted)
- SerialWombat I2C device (optional, for full testing)
- WiFi network (2.4GHz)

### Software Required
- PlatformIO Core 6.1.16+
- Serial terminal program (Arduino IDE Serial Monitor, or `screen`/`minicom`)
- Web browser (Chrome, Firefox, Edge)

## Build & Flash Instructions

### 1. Build Firmware

```bash
cd /path/to/CYD-ESP32-ESP01S-SerialWombat-WifiWombat

# For LCDWIKI 3.5" ESP32-32E (default target)
pio run --environment lcdwiki-35-esp32-32e

# For CYD 2.8" (ILI9341)
pio run --environment esp32-2432S028

# For CYD 7" (RGB 800x480)
pio run --environment esp32-8048S070

# For generic ESP32-S3
pio run --environment esp32-s3-devkit
```

**Expected Output:**
```
SUCCESS: Linking .pio/build/lcdwiki-35-esp32-32e/firmware.elf
Building .pio/build/lcdwiki-35-esp32-32e/firmware.bin
```

### 2. Flash to Device

```bash
# Auto-detect port and flash
pio run --target upload

# Or specify port explicitly
pio run --target upload --upload-port /dev/ttyUSB0   # Linux
pio run --target upload --upload-port COM3            # Windows
```

**Expected Output:**
```
Writing at 0x00010000... (100%)
Wrote 1234567 bytes (789012 compressed) at 0x00010000
Hash of data verified.
Leaving...
Hard resetting via RTS pin...
```

### 3. Monitor Serial Output

```bash
pio device monitor --baud 115200
```

## Smoke Test Checklist

### Test 1: Boot & Initialization ✅

**Procedure:**
1. Power on the device or press the reset button
2. Open serial monitor at 115200 baud
3. Observe boot messages

**Expected Results:**
- [ ] Serial output begins immediately
- [ ] LittleFS mounts successfully (or formats on first boot)
- [ ] Configuration loads (or creates default on first boot)
- [ ] I2C initialized at configured pins
- [ ] SerialWombat library initialized
- [ ] Security warning displayed if default password detected
- [ ] WiFi connects or AP mode starts ("Wombat-Setup")
- [ ] Web server starts on port 80
- [ ] OTA service starts
- [ ] No errors or crashes

**Sample Serial Output:**
```
*** SECURITY WARNING ***
Authentication is ENABLED
Username: admin
*** DEFAULT PASSWORD DETECTED ***
*** CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY ***
*** SYSTEM IS NOT SECURE WITH DEFAULT PASSWORD ***
************************

WiFi connected
IP address: 192.168.1.100
HTTP server started on port 80
OTA service ready
```

---

### Test 2: Display & Touch Response ✅

**Procedure:**
1. Observe the display after boot
2. Touch the screen at various locations
3. Try swiping gestures

**Expected Results (if display enabled):**
- [ ] Display initializes and shows content
- [ ] Backlight turns on (GPIO27 high for LCDWIKI board)
- [ ] First-boot wizard appears if not configured
- [ ] Status bar visible with WiFi/time/battery indicators
- [ ] Touch responds to taps (cursor/highlight moves)
- [ ] Touch responds to swipes (scroll/navigation)
- [ ] No screen flickering or artifacts
- [ ] Frame rate is acceptable (>20 FPS)

**Note:** If headless mode, display will be disabled and test should be skipped.

---

### Test 3: WiFi Configuration ✅

**Procedure:**
1. If first boot, connect to "Wombat-Setup" AP with phone/laptop
2. WiFi manager portal should appear (http://192.168.4.1)
3. Select your WiFi network and enter password
4. Device reboots and connects

**Expected Results:**
- [ ] AP "Wombat-Setup" visible in WiFi scan
- [ ] Captive portal redirects to configuration page
- [ ] Network list populated
- [ ] Credentials save successfully
- [ ] Device connects to WiFi after reboot
- [ ] IP address displayed in serial monitor
- [ ] Device accessible at assigned IP

---

### Test 4: Web UI Access ✅

**Procedure:**
1. Note the device IP address from serial monitor
2. Open web browser
3. Navigate to `http://<device-ip>/`
4. Try accessing different pages

**Expected Results:**
- [ ] Dashboard loads (HTML with device info)
- [ ] IP address displayed correctly
- [ ] System uptime shown
- [ ] Navigation links functional
- [ ] `/scanner` page loads (I2C scanner UI)
- [ ] `/configure` page loads (device config UI)
- [ ] `/settings` page loads (system settings UI)
- [ ] No 404 errors on valid routes
- [ ] No JavaScript console errors

---

### Test 5: HTTP Authentication ✅

**Procedure:**
1. Try accessing `/api/system` without credentials
2. Provide incorrect username/password
3. Provide correct credentials (default: admin/CHANGE_ME_NOW)
4. Try multiple failed attempts (>3)

**Expected Results:**
- [ ] `/api/health` accessible without auth (public endpoint)
- [ ] `/api/system` prompts for authentication
- [ ] Incorrect credentials return 401 Unauthorized
- [ ] Correct credentials return data (JSON)
- [ ] Rate limiting triggers after 3 failed attempts (5s lockout)
- [ ] Security headers present (CSP, X-Frame-Options, CORS)

**Test Command:**
```bash
# Public endpoint (should work)
curl http://<device-ip>/api/health

# Protected endpoint (should fail)
curl http://<device-ip>/api/system

# With auth (should work)
curl -u admin:CHANGE_ME_NOW http://<device-ip>/api/system
```

---

### Test 6: I2C Scanning ✅

**Procedure:**
1. Navigate to `/scanner` or `/scan-data`
2. Click "Scan" button
3. Observe results
4. Connect a SerialWombat I2C device (optional)
5. Rescan and verify device detected

**Expected Results:**
- [ ] Scan completes within 2-3 seconds
- [ ] JSON response with device list
- [ ] If no devices: `{"devices": []}`
- [ ] If devices found: `{"devices": [{"addr": "0x6B", "variant": "..."}]}`
- [ ] Deep scan provides detailed info (pin modes, variant)
- [ ] No I2C bus crashes or hangs
- [ ] Traffic counters increment in status bar (if display enabled)

**Test Command:**
```bash
curl -u admin:CHANGE_ME_NOW http://<device-ip>/scan-data
```

---

### Test 7: Storage Operations ✅

**Procedure:**
1. Navigate to `/api/sd/status`
2. Insert SD card (if not already)
3. Try mounting SD
4. List files on SD
5. Upload a small test file
6. Download the file
7. Delete the file
8. Safely eject

**Expected Results:**
- [ ] SD status returns mounted/unmounted state
- [ ] SD card mounts successfully (if present)
- [ ] File listing returns directory contents (JSON)
- [ ] File upload succeeds (200 OK)
- [ ] File download returns correct content
- [ ] File deletion works
- [ ] Eject unmounts cleanly
- [ ] LittleFS operations work independently

**Test Commands:**
```bash
# SD status
curl -u admin:CHANGE_ME_NOW http://<device-ip>/api/sd/status

# List files
curl -u admin:CHANGE_ME_NOW http://<device-ip>/api/sd/list

# Upload file
curl -u admin:CHANGE_ME_NOW -F "file=@test.txt" http://<device-ip>/api/sd/upload

# Download file
curl -u admin:CHANGE_ME_NOW "http://<device-ip>/sd/download?path=/test.txt" -o downloaded.txt
```

---

### Test 8: Firmware Upload & Flash ✅

**Procedure:**
1. Prepare a SerialWombat firmware file (.bin or .hex)
2. Navigate to firmware manager
3. Upload firmware to a slot
4. Connect SerialWombat device via I2C
5. Flash firmware to device

**Expected Results:**
- [ ] Binary firmware uploads successfully
- [ ] HEX firmware converts and uploads
- [ ] Firmware metadata displayed (size, version)
- [ ] Flash operation completes without errors
- [ ] SerialWombat device boots with new firmware
- [ ] Device responds to I2C commands

**Test Commands:**
```bash
# Upload binary firmware
curl -u admin:CHANGE_ME_NOW -F "file=@firmware.bin" http://<device-ip>/upload_fw

# Upload HEX firmware (auto-converts)
curl -u admin:CHANGE_ME_NOW -F "file=@firmware.hex" http://<device-ip>/upload_hex

# Flash to device (requires SerialWombat on I2C)
curl -u admin:CHANGE_ME_NOW -X POST http://<device-ip>/flashfw
```

---

### Test 9: Configuration Save/Load ✅

**Procedure:**
1. Navigate to `/api/config/list`
2. Load current config
3. Modify a setting (e.g., display brightness)
4. Save config with a name
5. Reboot device
6. Verify settings persisted

**Expected Results:**
- [ ] Config list returns saved profiles
- [ ] Config load returns current settings (JSON)
- [ ] Config save succeeds (200 OK)
- [ ] Settings persist across reboots
- [ ] Invalid config rejected (schema validation)
- [ ] Config files stored in `/config/` (LittleFS)

**Test Commands:**
```bash
# List configs
curl -u admin:CHANGE_ME_NOW http://<device-ip>/api/config/list

# Load config
curl -u admin:CHANGE_ME_NOW http://<device-ip>/api/config/load?name=default

# Save config
curl -u admin:CHANGE_ME_NOW -X POST \
  -H "Content-Type: application/json" \
  -d '{"display_enable": true, "brightness": 200}' \
  http://<device-ip>/api/config/save?name=my_config
```

---

### Test 10: OTA Firmware Update ✅

**Procedure:**
1. Build a new firmware version
2. Identify device IP and OTA password
3. Use PlatformIO or Arduino IDE to upload OTA

**Expected Results:**
- [ ] OTA advertised on network (mDNS: wombat-bridge.local)
- [ ] Password authentication works
- [ ] Upload progress shown
- [ ] Device reboots after successful upload
- [ ] New firmware boots successfully
- [ ] No configuration lost

**Test Command:**
```bash
# Using PlatformIO
pio run --target upload --upload-port <device-ip>

# Or using espota.py
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i <device-ip> -p 3232 --auth=CHANGE_ME_NOW -f firmware.bin
```

---

### Test 11: LVGL UI Navigation (if display enabled) ✅

**Procedure:**
1. Power on device with display
2. Navigate through UI screens using touch
3. Access each screen from the home dashboard
4. Verify all controls respond

**Expected Results:**
- [ ] Home dashboard displays feature tiles
- [ ] Status bar updates (WiFi, time, battery, I2C)
- [ ] System Status screen shows memory/CPU/uptime
- [ ] Network screen shows WiFi info
- [ ] Storage screen shows SD/LittleFS usage
- [ ] I2C Tools screen can trigger scans
- [ ] Settings screen allows config changes
- [ ] Back/Home navigation works
- [ ] Touch calibration screen functional
- [ ] No UI freezes or crashes

---

### Test 12: Stress Test ✅

**Procedure:**
1. Run continuous I2C scans (10 scans back-to-back)
2. Upload/download large files (5MB)
3. Rapid web page navigation
4. Let device run for 1+ hours

**Expected Results:**
- [ ] No crashes or reboots
- [ ] Memory usage stable (no leaks)
- [ ] Web server remains responsive
- [ ] I2C operations remain fast
- [ ] Display updates smoothly
- [ ] Heap free space doesn't degrade significantly
- [ ] Uptime counter increases correctly

---

## Test Results Summary

### Passing Criteria
- **Critical Tests (1-10)**: Must pass 100%
- **LVGL UI Test (11)**: Must pass if display enabled
- **Stress Test (12)**: Must pass 80% (some degradation acceptable)

### Test Execution Record

| Test | Date | Tester | Hardware | Result | Notes |
|------|------|--------|----------|--------|-------|
| 1. Boot & Init | | | | ⬜ Pass / ⬜ Fail | |
| 2. Display & Touch | | | | ⬜ Pass / ⬜ Fail / ⬜ N/A | |
| 3. WiFi Config | | | | ⬜ Pass / ⬜ Fail | |
| 4. Web UI Access | | | | ⬜ Pass / ⬜ Fail | |
| 5. HTTP Auth | | | | ⬜ Pass / ⬜ Fail | |
| 6. I2C Scanning | | | | ⬜ Pass / ⬜ Fail | |
| 7. Storage Ops | | | | ⬜ Pass / ⬜ Fail | |
| 8. FW Upload/Flash | | | | ⬜ Pass / ⬜ Fail | |
| 9. Config Save/Load | | | | ⬜ Pass / ⬜ Fail | |
| 10. OTA Update | | | | ⬜ Pass / ⬜ Fail | |
| 11. LVGL UI Nav | | | | ⬜ Pass / ⬜ Fail / ⬜ N/A | |
| 12. Stress Test | | | | ⬜ Pass / ⬜ Fail | |

---

## Known Issues & Limitations

### Current Limitations (as of refactoring)
- **Compilation**: Network errors may occur during platform package download (retry resolves)
- **Display**: First-boot wizard only works if `configured=false` in config
- **SD Card**: Some SD cards may require slower SPI speeds (adjust `SD_SCK_MHZ` if issues)
- **Touch**: Resistive touch (XPT2046) requires calibration for accurate positioning
- **OTA**: Password must match `AUTH_PASSWORD` in code

### Troubleshooting

**Device won't boot:**
- Check power supply (5V, 500mA minimum)
- Verify correct board selection in platformio.ini
- Try erasing flash: `pio run --target erase`

**WiFi won't connect:**
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check WPA2 encryption (WPA3 not supported)
- Reset WiFi settings via `/resetwifi` endpoint

**Display is black:**
- Verify `DISPLAY_SUPPORT_ENABLED=1` in code
- Check backlight connection (GPIO27 for LCDWIKI)
- Try different panel type in config

**Touch not working:**
- Run touch calibration
- Verify XPT2046 wiring (CS=33, IRQ=36 for LCDWIKI)
- Check for SPI bus conflicts

**I2C devices not detected:**
- Verify pull-up resistors on SDA/SCL (4.7kΩ recommended)
- Check I2C pin configuration in config.json
- Try slower I2C speed (100kHz instead of 400kHz)

---

## Demo Flow (Quick Validation)

For rapid validation, follow this condensed flow:

1. **Flash firmware** → Device boots, serial output OK ✅
2. **Connect to "Wombat-Setup" AP** → Configure WiFi ✅
3. **Access web dashboard** at device IP → Page loads ✅
4. **Run I2C scan** via `/scan-data` → Returns JSON ✅
5. **Upload test file to SD** (if SD card present) → Upload succeeds ✅
6. **Trigger OTA update** → Firmware updates successfully ✅

**Expected Time:** ~10 minutes for full demo flow

---

## Reporting Issues

If any test fails, please report with:
- Test number and name
- Hardware used (board model, display type)
- Build environment (env name from platformio.ini)
- Serial output (relevant portions)
- Web browser console errors (if web UI issue)
- Steps to reproduce

File issues at: https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/issues

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-24  
**Applies To:** Refactored firmware (post-modularization)
