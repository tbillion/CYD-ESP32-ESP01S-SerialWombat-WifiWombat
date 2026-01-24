# Build Instructions

## Overview
This document provides comprehensive instructions for building, flashing, and deploying the refactored CYD ESP32 Serial Wombat WiFi Bridge firmware.

## System Requirements

### Development Environment
- **Operating System**: Linux, macOS, or Windows 10/11
- **Python**: 3.7 or newer
- **Git**: For cloning the repository
- **USB Drivers**: CH340/CP2102 drivers for ESP32 programming

### Supported Hardware Targets
- ESP32 classic (ESP32-WROOM, ESP32-WROVER)
- ESP32-S3 (ESP32-S3-WROOM, ESP32-S3-DevKitC)
- CYD 2.8" (ESP32 + ILI9341 320x240 display)
- CYD 7" (ESP32-S3 + RGB 800x480 display)
- **LCDWIKI 3.5" ESP32-32E** (ESP32 + ST7796U 320x480 display) - Default target

---

## Installation

### 1. Install PlatformIO Core

PlatformIO is the recommended build system (Arduino IDE also supported but not documented here).

**Linux/macOS:**
```bash
# Install via pip
pip install platformio==6.1.16

# Or via package manager (Ubuntu/Debian)
sudo apt install platformio

# Verify installation
pio --version
```

**Windows:**
```powershell
# Install via pip (PowerShell or CMD)
pip install platformio==6.1.16

# Verify installation
pio --version
```

**Expected Output:**
```
PlatformIO Core, version 6.1.16
```

### 2. Clone Repository

```bash
git clone https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat.git
cd CYD-ESP32-ESP01S-SerialWombat-WifiWombat

# Switch to refactored branch (if applicable)
git checkout copilot/refactor-modularize-firmware
```

### 3. Install USB Drivers (if needed)

**Windows:**
- CH340: https://learn.sparkfun.com/tutorials/how-to-install-ch340-drivers
- CP2102: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

**Linux:**
- Usually built-in; may need to add user to `dialout` group:
  ```bash
  sudo usermod -a -G dialout $USER
  # Log out and log back in
  ```

**macOS:**
- CH340: https://github.com/adrianmihalko/ch340g-ch34g-ch34x-mac-os-x-driver

---

## Configuration

### Security Configuration (CRITICAL)

Before deploying, you **MUST** change the default passwords in the code:

**File:** `src/config/defaults.h` (or original .ino if not yet refactored)

```cpp
// CHANGE THESE BEFORE DEPLOYMENT!
#define AUTH_USERNAME "admin"
#define AUTH_PASSWORD "YourStrongPasswordHere"  // NOT "CHANGE_ME_NOW"
#define CORS_ALLOW_ORIGIN "https://yourdomain.com"  // NOT "*"
```

**OTA Password:**
The OTA password will match `AUTH_PASSWORD` by default. Change it in `src/app/app.cpp`:

```cpp
ArduinoOTA.setPassword(AUTH_PASSWORD);
```

### Hardware Pin Configuration

For custom hardware, edit pin definitions in:
- `src/config/defaults.h` - Compile-time defaults
- `src/config/system_config.h` - Runtime configuration structure
- `src/hal/targets/lcdwiki_35_esp32_32e.h` - Hardware profile for LCDWIKI board

**LCDWIKI 3.5" ESP32-32E Default Pins:**
```cpp
// Display (SPI)
#define LCD_SCK  14
#define LCD_MOSI 13
#define LCD_MISO 12
#define LCD_CS   15
#define LCD_DC   2
#define LCD_BL   27  // Backlight
#define LCD_RST  EN  // Shared with ESP32 reset

// Touch (XPT2046, shared SPI)
#define TP_CS    33
#define TP_IRQ   36

// SD Card (separate SPI)
#define SD_CS    5
#define SD_MOSI  23
#define SD_MISO  19
#define SD_SCK   18

// I2C
#define I2C_SDA  32
#define I2C_SCL  25

// RGB LED (common anode)
#define LED_R    22
#define LED_G    16
#define LED_B    17

// Battery ADC
#define BAT_ADC  34
```

---

## Build Process

### Available Build Targets

PlatformIO environments are defined in `platformio.ini`:

| Environment | Board | Display | Notes |
|-------------|-------|---------|-------|
| `esp32-devkit` | ESP32 generic | None (headless) | Basic ESP32, no display |
| `esp32-s3-devkit` | ESP32-S3 | None (headless) | ESP32-S3, default target |
| `esp32-2432S028` | ESP32 | ILI9341 320x240 | CYD 2.8" |
| `esp32-8048S070` | ESP32-S3 | RGB 800x480 | CYD 7" |
| `lcdwiki-35-esp32-32e` | ESP32 | ST7796U 320x480 | **LCDWIKI 3.5" (recommended)** |

### Build Commands

**Build for specific target:**
```bash
# LCDWIKI 3.5" ESP32-32E (default for refactoring)
pio run --environment lcdwiki-35-esp32-32e

# CYD 2.8" (ILI9341)
pio run --environment esp32-2432S028

# CYD 7" (RGB)
pio run --environment esp32-8048S070

# ESP32-S3 DevKit (headless)
pio run --environment esp32-s3-devkit
```

**Build all targets:**
```bash
pio run
```

**Clean build (force recompile):**
```bash
pio run --target clean
pio run --environment lcdwiki-35-esp32-32e
```

**Verbose build output:**
```bash
pio run --environment lcdwiki-35-esp32-32e --verbose
```

### Build Output

Successful builds produce firmware files in `.pio/build/<env>/`:
- `firmware.elf` - Executable with debug symbols
- `firmware.bin` - Raw binary for flashing
- `partitions.bin` - Partition table (if using custom partitions)

**Expected Output:**
```
Linking .pio/build/lcdwiki-35-esp32-32e/firmware.elf
Retrieving maximum program size .pio/build/lcdwiki-35-esp32-32e/firmware.elf
Checking size .pio/build/lcdwiki-35-esp32-32e/firmware.elf
Building .pio/build/lcdwiki-35-esp32-32e/firmware.bin
esptool.py v4.5.1
Creating esp32 image...
Successfully created esp32 image.

========================= [SUCCESS] Took 45.67 seconds =========================
```

---

## Flashing

### 1. Connect Hardware

1. Connect ESP32 board to computer via USB
2. Put board in download mode (usually automatic with modern boards)
   - Some boards require holding BOOT button while pressing RESET
3. Identify the serial port:

**Linux:**
```bash
ls /dev/ttyUSB*    # Usually /dev/ttyUSB0
ls /dev/ttyACM*    # Or /dev/ttyACM0
```

**macOS:**
```bash
ls /dev/cu.usbserial*   # Usually /dev/cu.usbserial-*
ls /dev/cu.wchusbserial*
```

**Windows:**
```powershell
# Check Device Manager → Ports (COM & LPT)
# Usually COM3, COM4, etc.
```

### 2. Flash Firmware

**Auto-detect port:**
```bash
pio run --target upload --environment lcdwiki-35-esp32-32e
```

**Specify port explicitly:**
```bash
# Linux/macOS
pio run --target upload --environment lcdwiki-35-esp32-32e --upload-port /dev/ttyUSB0

# Windows
pio run --target upload --environment lcdwiki-35-esp32-32e --upload-port COM3
```

**Adjust upload speed (if issues):**
```bash
pio run --target upload --environment lcdwiki-35-esp32-32e --upload-speed 115200
```

**Expected Output:**
```
Configuring upload protocol...
AVAILABLE: cmsis-dap, esp-bridge, esp-builtin, esp-prog, espota, esptool, iot-bus-jtag, jlink, minimodule, olimex-arm-usb-ocd, olimex-arm-usb-ocd-h, olimex-arm-usb-tiny-h, olimex-jtag-tiny, tumpa
CURRENT: upload_protocol = esptool
Looking for upload port...
Auto-detected: /dev/ttyUSB0
Uploading .pio/build/lcdwiki-35-esp32-32e/firmware.bin
esptool.py v4.5.1
Serial port /dev/ttyUSB0
Connecting....
Chip is ESP32-D0WDQ6 (revision v1.0)
...
Writing at 0x00010000... (100 %)
Wrote 1234567 bytes (789012 compressed) at 0x00010000 in 15.6 seconds (effective 632.1 kbit/s)...
Hash of data verified.

Leaving...
Hard resetting via RTS pin...

========================= [SUCCESS] Took 18.23 seconds =========================
```

### 3. Erase Flash (if needed)

If experiencing issues, erase the entire flash:

```bash
pio run --target erase --environment lcdwiki-35-esp32-32e
```

Then flash again:
```bash
pio run --target upload --environment lcdwiki-35-esp32-32e
```

---

## Serial Monitor

### View Serial Output

```bash
# PlatformIO monitor
pio device monitor --baud 115200

# Or specify port
pio device monitor --port /dev/ttyUSB0 --baud 115200
```

**Alternative Tools:**
```bash
# screen (Linux/macOS)
screen /dev/ttyUSB0 115200

# minicom (Linux)
minicom -D /dev/ttyUSB0 -b 115200

# PuTTY (Windows)
# Configure Serial, COM3, 115200 baud
```

**Exit Monitor:**
- PlatformIO: `Ctrl+C`
- screen: `Ctrl+A` then `K` then `Y`
- minicom: `Ctrl+A` then `X`

---

## Over-The-Air (OTA) Updates

After initial flash, you can update firmware wirelessly:

### 1. Ensure Device on Network

Device must be connected to WiFi and visible on the network.

### 2. OTA Upload

**PlatformIO:**
```bash
# Build and upload OTA
pio run --target upload --upload-port <device-ip>

# Example
pio run --target upload --upload-port 192.168.1.100 --environment lcdwiki-35-esp32-32e
```

**Python Script (Manual):**
```bash
python ~/.platformio/packages/framework-arduinoespressif32/tools/espota.py \
  -i 192.168.1.100 \
  -p 3232 \
  --auth=YourOTAPassword \
  -f .pio/build/lcdwiki-35-esp32-32e/firmware.bin
```

**Expected Output:**
```
Sending invitation to 192.168.1.100
Authentication OK
Uploading: [============================================================] 100%
Done. Resetting device...
```

### 3. Verify Update

After OTA:
1. Device reboots automatically
2. Check serial monitor for new boot messages
3. Verify new firmware version via web UI or `/api/system`

---

## Troubleshooting

### Build Errors

**Error:** `Platform Manager: Installing espressif32 @ ^6.8.1 HTTPClientError`
- **Cause:** Network timeout downloading ESP32 platform
- **Solution:** Retry the build, or manually install platform:
  ```bash
  pio pkg install --platform "espressif32@^6.8.1"
  ```

**Error:** `fatal error: <module>.h: No such file or directory`
- **Cause:** Missing or incorrect include paths
- **Solution:** Verify `platformio.ini` has `src_dir = src` and `include_dir = src`

**Error:** `undefined reference to 'function_name'`
- **Cause:** Missing implementation or linker issue
- **Solution:** Ensure all `.cpp` files are in `src/` directory tree

### Upload Errors

**Error:** `Failed to connect to ESP32: Timed out`
- **Solutions:**
  1. Hold BOOT button while connecting power
  2. Press RESET while holding BOOT
  3. Try lower upload speed: `--upload-speed 115200`
  4. Check USB cable (use data cable, not charge-only)

**Error:** `Permission denied: '/dev/ttyUSB0'`
- **Solution (Linux):** Add user to dialout group:
  ```bash
  sudo usermod -a -G dialout $USER
  newgrp dialout  # Or log out and back in
  ```

**Error:** `A fatal error occurred: MD5 of file does not match data in flash!`
- **Solution:** Erase flash and re-upload:
  ```bash
  pio run --target erase
  pio run --target upload
  ```

### Runtime Errors

**WiFi won't connect:**
- Ensure 2.4GHz network (ESP32 doesn't support 5GHz)
- Check WPA2 encryption (WPA3 not widely supported)
- Reset WiFi via `/resetwifi` endpoint or button

**Display stays black:**
- Verify `DISPLAY_SUPPORT_ENABLED=1` in code
- Check backlight pin (GPIO27 for LCDWIKI)
- Try different panel configuration in `config.json`

**OTA fails with "Authentication Failed":**
- Verify OTA password matches `AUTH_PASSWORD` in code
- Check firewall isn't blocking port 3232
- Ensure device and computer on same network/VLAN

---

## Advanced Build Options

### Custom Partitions

To use custom partition scheme (more space for app or filesystem):

1. Create `partitions_custom.csv`:
   ```csv
   # Name,   Type, SubType, Offset,  Size, Flags
   nvs,      data, nvs,     0x9000,  0x5000,
   otadata,  data, ota,     0xe000,  0x2000,
   app0,     app,  ota_0,   0x10000, 0x1E0000,
   app1,     app,  ota_1,   0x1F0000,0x1E0000,
   spiffs,   data, spiffs,  0x3D0000,0x30000,
   ```

2. Update `platformio.ini`:
   ```ini
   [env:lcdwiki-35-esp32-32e]
   board_build.partitions = partitions_custom.csv
   ```

### Optimization Flags

For production builds with optimizations:

```ini
[env:lcdwiki-35-esp32-32e]
build_flags = 
    ${env.build_flags}
    -O2                      ; Optimize for speed
    -DNDEBUG                 ; Disable assertions
    -DCORE_DEBUG_LEVEL=0     ; Disable debug logging
```

For debugging:
```ini
build_flags = 
    ${env.build_flags}
    -Og                      ; Optimize for debugging
    -g3                      ; Full debug info
    -DCORE_DEBUG_LEVEL=5     ; Verbose logging
```

### Build Without Display

To compile headless (no display support):

**Option 1:** Use `esp32-devkit` environment (already configured)

**Option 2:** Add custom environment:
```ini
[env:headless]
board = esp32dev
build_flags = 
    ${env.build_flags}
    -D DISPLAY_SUPPORT_ENABLED=0
```

### Build Without SD Card

To compile without SD card support:

Edit `src/config/defaults.h`:
```cpp
#define SD_SUPPORT_ENABLED 0
```

---

## Continuous Integration (CI)

### GitHub Actions Workflow

The repository includes CI workflows (`.github/workflows/`):
- `build.yml` - Automated builds on push
- `security.yml` - Security scanning with CodeQL

**Trigger Build:**
```bash
git add .
git commit -m "Update firmware"
git push origin copilot/refactor-modularize-firmware
```

GitHub Actions will automatically:
1. Build for all environments
2. Run security scans
3. Report results in PR checks

---

## Binary Distribution

### Creating Release Binaries

```bash
# Build all targets
pio run

# Collect binaries
mkdir -p release/
cp .pio/build/lcdwiki-35-esp32-32e/firmware.bin release/firmware_lcdwiki_35.bin
cp .pio/build/esp32-2432S028/firmware.bin release/firmware_cyd_28.bin
cp .pio/build/esp32-8048S070/firmware.bin release/firmware_cyd_70.bin

# Create checksums
cd release/
sha256sum *.bin > SHA256SUMS
```

### Flashing Pre-built Binaries

Users without PlatformIO can flash pre-built binaries:

**esptool.py (Python):**
```bash
pip install esptool

esptool.py --chip esp32 --port /dev/ttyUSB0 \
  --baud 921600 --before default_reset --after hard_reset \
  write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect \
  0x1000 bootloader.bin \
  0x8000 partitions.bin \
  0x10000 firmware.bin
```

**ESP Flash Download Tool (Windows):**
1. Download from Espressif: https://www.espressif.com/en/support/download/other-tools
2. Select ESP32, SPIDownload mode
3. Add binaries at correct offsets:
   - `0x1000`: bootloader
   - `0x8000`: partitions
   - `0x10000`: firmware
4. Click Start

---

## Additional Resources

- **PlatformIO Docs**: https://docs.platformio.org/
- **ESP32 Arduino Core**: https://github.com/espressif/arduino-esp32
- **LovyanGFX**: https://github.com/lovyan03/LovyanGFX
- **LVGL**: https://docs.lvgl.io/
- **SerialWombat**: https://broadwell-consulting.com/serialwombat

---

## Support

For build issues:
1. Check this document first
2. Review existing GitHub issues: https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/issues
3. Join discussions: https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/discussions
4. File new issue with:
   - PlatformIO version (`pio --version`)
   - Operating system
   - Target environment
   - Full build output (use `--verbose`)

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-24  
**Applies To:** Refactored firmware structure
