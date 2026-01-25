# CYD ESP32 Serial Wombat WiFi Bridge

> ESP32 firmware for managing Serial Wombat I2C devices via web interface with optional TFT display

[![Build](https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/workflows/Build%20and%20Test/badge.svg)](https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/actions)
[![Security](https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/workflows/Security%20Scanning/badge.svg)](https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat/actions)
[![License](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)

## Overview

Transform CYD (Cheap Yellow Display) ESP32 boards into powerful bridges for Serial Wombat I2C devices with web management, local display, and comprehensive security.

**Key Features:**
- 🔒 Secure web interface with HTTP Basic Auth
- 📡 I2C device scanning and management  
- 🔌 TCP-to-I2C bridge for remote access
- 💾 SD card file management
- 📺 Optional TFT display with LVGL GUI
- 🔄 Over-the-air (OTA) firmware updates
- 📦 Firmware flashing to Serial Wombat devices

## Quick Start

### 1. Install

```bash
git clone https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat.git
cd CYD-ESP32-ESP01S-SerialWombat-WifiWombat
pip install platformio==6.1.16
pio run --environment esp32-s3-devkit
pio run --target upload
```

### 2. Configure WiFi

1. Connect to AP: `Wombat-Setup`
2. Enter your WiFi credentials
3. Device reboots and connects

### 3. Access Web Interface

```bash
# Find device IP in serial monitor or router
# Then access: http://<device-ip>/

# Default credentials (MUST CHANGE):
# Username: admin
# Password: CHANGE_ME_NOW
```

## ⚠️ Security First Deployment

**BEFORE deploying, you MUST change default passwords:**

Edit `src/config/defaults.h`:
```cpp
#define AUTH_PASSWORD "YourStrongPassword"  // Used for web auth AND OTA
#define CORS_ALLOW_ORIGIN "https://yourdomain.com"
```

Recompile and upload. See [SECURITY.md](SECURITY.md) for complete hardening guide.

> **Note:** The firmware has been refactored into a modular architecture. The legacy monolithic `.ino` file is preserved as `.ino.legacy` for reference. See [LEGACY_INO_README.md](LEGACY_INO_README.md) for details.

## Architecture

```
[Web Browser] ──HTTP──> [ESP32 WebServer] ──Auth──> [I2C Manager]
                              │                          │
                              ├──> [SD Card]             │
                              ├──> [LVGL Display]        │
                              └──> [OTA Updater]         ▼
                                                    [Serial Wombat
                                                     I2C Devices]
```

**Security Model:**
- HTTP Basic Auth on all sensitive endpoints
- Input validation (I2C, pins, paths)
- Path traversal protection
- Rate limiting (3 failures = 5s lockout)
- Security headers (CSP, CORS, X-Frame-Options)
- Request size limits (5MB uploads)

See [docs/security/THREAT_MODEL.md](docs/security/THREAT_MODEL.md)

## Repository Structure

```
├── CYD_Framework_LGFX_LVGL_Final_COMPILE_OK_FIXED6.ino  # Main firmware
├── cyd_predecls.h                                        # Forward decls
├── platformio.ini                                        # Build config
├── SECURITY.md                                           # Security guide
├── DEPENDENCIES.md                                       # Dependencies
├── .github/workflows/                                    # CI/CD
│   ├── build.yml                                        # Build pipeline
│   └── security.yml                                     # Security scans
└── docs/
    ├── audit/                                           # Security audit
    ├── security/                                        # Threat model
    └── ops/                                             # Operations
```

## Configuration

### Compile-Time (Required)

```cpp
// Security (lines 25-30, 47-48)
#define AUTH_USERNAME "admin"
#define AUTH_PASSWORD "CHANGE_ME_NOW"       // ⚠️ MUST CHANGE
#define OTA_PASSWORD "CHANGE_ME_NOW"        // ⚠️ MUST CHANGE  
#define CORS_ALLOW_ORIGIN "*"               // ⚠️ MUST CHANGE

// Features (lines 23-24, 44)
#define SD_SUPPORT_ENABLED 1                // 0=disable SD
#define DISPLAY_SUPPORT_ENABLED 1           // 0=headless mode

// Hardware (lines 26-30)
#define SD_CS   5
#define SD_MOSI 23
#define SD_MISO 19
#define SD_SCK  18
```

### Runtime (config.json)

Created on first boot at `/config.json`. Edit via `/settings` endpoint.

```json
{
  "i2c_sda": 21,
  "i2c_scl": 22,
  "display_enable": true,
  "panel": 1,
  "touch": 1
}
```

## Web API

All endpoints require HTTP Basic Auth except `/` and `/api/health`.

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Dashboard (public) |
| `/api/health` | GET | Health check (public) |
| `/scan-data` | GET | I2C scan results |
| `/connect` | POST | Connect to I2C device |
| `/flashfw` | POST | Flash firmware |
| `/upload_fw` | POST | Upload firmware file |
| `/api/system` | GET | System info |
| `/api/sd/*` | GET/POST | SD operations |
| `/resetwifi` | POST | Reset WiFi |

**Example:**
```bash
curl -u admin:password http://device-ip/scan-data
curl -u admin:password -F "file=@fw.bin" http://device-ip/upload_fw
```

## Supported Hardware

### Boards
- ESP32-S3 DevKit (`esp32-s3-devkit`)
- ESP32 DevKit (`esp32-devkit`)
- CYD 2.8" ILI9341 (`esp32-2432S028`)
- CYD 7" RGB 800x480 (`esp32-8048S070`)

### Displays
- ILI9341 (320x240 SPI)
- ST7789 (320x240 SPI)
- ST7796 (320x480 SPI)
- RGB panels (800x480)

### Touch
- XPT2046 (resistive)
- GT911 (capacitive)

## Dependencies

All dependencies pinned in `platformio.ini`:

```ini
lib_deps = 
    bblanchon/ArduinoJson @ ^7.2.1
    lovyan03/LovyanGFX @ ^1.2.0
    lvgl/lvgl @ ^9.2.0
    greiman/SdFat @ ^2.2.3
    broadwellconsulting/SerialWombat @ ^2.3.3
    tzapu/WiFiManager @ ^2.0.17
```

See [DEPENDENCIES.md](DEPENDENCIES.md) for licenses and security notes.

## Development

### Build

```bash
# PlatformIO (recommended)
pio run --environment esp32-devkit

# Arduino IDE
# Install libraries via Library Manager
# Open .ino file and upload
```

### Format Code

```bash
clang-format -i *.ino *.h
```

### Security Scans

```bash
# Secret scanning
docker run -v $(pwd):/repo gitleaks/gitleaks:latest detect

# Vulnerability scanning  
osv-scanner --lockfile=platformio.ini
```

## Production Deployment

### Pre-Deployment Checklist

- [ ] Change `AUTH_PASSWORD`
- [ ] Change `OTA_PASSWORD`
- [ ] Configure `CORS_ALLOW_ORIGIN` to your domain
- [ ] Read [SECURITY.md](SECURITY.md)
- [ ] Test authentication works
- [ ] Verify compile warnings addressed
- [ ] Place on isolated network/VLAN
- [ ] **Never** expose to public internet without VPN

### Monitoring

```bash
# Health check (no auth)
curl http://device-ip/api/health
# Returns: {"status":"ok","uptime":12345,"heap":200000}

# System info (requires auth)
curl -u admin:password http://device-ip/api/system
```

### Backup

```bash
# Config backup
curl -u admin:password http://device-ip/api/config/load > backup.json

# SD files
curl -u admin:password http://device-ip/sd/download?path=/file.txt -o file.txt
```

## Troubleshooting

**WiFi not connecting:**
- Use 2.4GHz network (ESP32 doesn't support 5GHz)
- Reset WiFi: `/resetwifi` or hold reset button
- Check serial monitor for errors

**Can't access web interface:**
- Find IP via serial monitor or router
- Use `http://` not `https://`
- Check authentication credentials

**SD card not detected:**
- Format as FAT32
- Verify pin definitions
- Check `/api/sd/status`

**Display not working:**
- Verify `DISPLAY_SUPPORT_ENABLED=1`
- Check panel type in config
- Review serial logs

See [docs/ops/RUNBOOK.md](docs/ops/RUNBOOK.md) for detailed troubleshooting.

## Documentation

- [SECURITY.md](SECURITY.md) - Security guide and best practices
- [SECURITY_CHANGES.md](SECURITY_CHANGES.md) - Security implementation details
- [DEPENDENCIES.md](DEPENDENCIES.md) - Dependency licenses and CVEs
- [docs/audit/BASELINE.md](docs/audit/BASELINE.md) - Initial security assessment
- [docs/audit/HARDENING_PLAN.md](docs/audit/HARDENING_PLAN.md) - Security roadmap
- [docs/security/THREAT_MODEL.md](docs/security/THREAT_MODEL.md) - Threat analysis
- [docs/ops/RUNBOOK.md](docs/ops/RUNBOOK.md) - Operations guide

## Contributing

1. Read [SECURITY.md](SECURITY.md) for security guidelines
2. Fork the repository
3. Create feature branch
4. Make changes with clear commits
5. Ensure code passes `clang-format`
6. Test thoroughly
7. Submit pull request

**Security vulnerabilities:** See [SECURITY.md](SECURITY.md) for responsible disclosure.

## License

MIT License - see [LICENSE](LICENSE) file.

Copyright (c) 2026 tbillion

## Acknowledgments

- ESP32 Arduino Core - Espressif Systems
- LovyanGFX - lovyan03
- LVGL - LVGL Team
- SerialWombat - Broadwell Consulting Inc.
- WiFiManager - tzapu
- ArduinoJson - Benoit Blanchon

---

**Status:** ✅ Production Ready (with security hardening)  
**Last Updated:** 2026-01-24  
**Maintainer:** [@tbillion](https://github.com/tbillion)
