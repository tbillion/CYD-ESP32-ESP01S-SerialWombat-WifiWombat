# Dependency Documentation

This document lists all external dependencies used in this project with their licenses and security notes.

## Runtime Dependencies

### Core Framework

| Dependency | Version | License | Purpose | Security Notes |
|------------|---------|---------|---------|----------------|
| **ESP32 Arduino Core** | 3.1.0 | LGPL-2.1 | Core framework for ESP32 | Official Espressif framework, actively maintained |

### Libraries

| Dependency | Version | License | Repository | Purpose |
|------------|---------|---------|------------|---------|
| **ArduinoJson** | ^7.2.1 | MIT | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | JSON parsing and serialization |
| **LovyanGFX** | ^1.2.0 | BSD-3-Clause | [lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | Display driver abstraction for TFT panels |
| **LVGL** | ^9.2.0 | MIT | [lvgl/lvgl](https://github.com/lvgl/lvgl) | GUI framework for embedded displays |
| **SdFat** | ^2.2.3 | MIT | [greiman/SdFat](https://github.com/greiman/SdFat) | SD card file system library |
| **SerialWombat** | ^2.3.3 | MIT | [BroadwellConsultingInc/SerialWombat](https://github.com/BroadwellConsultingInc/SerialWombat) | Hardware abstraction for Serial Wombat I2C devices |
| **WiFiManager** | ^2.0.17 | MIT | [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) | WiFi configuration management with captive portal |

### Built-in ESP32 Libraries (included with Arduino Core)

- **WiFi.h** - WiFi connectivity
- **WebServer.h** - HTTP server
- **ArduinoOTA.h** - Over-the-air firmware updates
- **Wire.h** - I2C communication
- **FS.h** - File system abstraction
- **LittleFS.h** - Flash file system
- **SPI.h** - SPI communication

## Development Dependencies

| Tool | Version | License | Purpose |
|------|---------|---------|---------|
| **PlatformIO** | 6.1.16 | Apache-2.0 | Build system and package manager |
| **clang-format** | 18 | Apache-2.0 | Code formatting |

## CI/CD Dependencies (GitHub Actions)

All GitHub Actions are pinned to specific commit SHAs for security.

| Action | Version | Purpose |
|--------|---------|---------|
| `actions/checkout` | v4.2.1 (eef6144) | Repository checkout |
| `actions/cache` | v4.1.1 (3624ceb) | Build cache |
| `actions/setup-python` | v5.2.0 (f677139) | Python environment |
| `actions/upload-artifact` | v4.4.3 (b4b15b8) | Artifact upload |
| `jidicula/clang-format-action` | v4.13.0 (c743836) | Code format checking |
| `gitleaks/gitleaks-action` | v2.3.6 (4a20c8b) | Secret scanning |
| `google/osv-scanner-action` | v1.9.1 (5a0f5ff) | Vulnerability scanning |
| `github/codeql-action/init` | v3.27.9 (6624720) | CodeQL initialization |
| `github/codeql-action/analyze` | v3.27.9 (6624720) | CodeQL analysis |

## License Compliance

### License Summary

- **MIT License:** ArduinoJson, LVGL, SdFat, SerialWombat, WiFiManager
- **BSD-3-Clause:** LovyanGFX
- **LGPL-2.1:** ESP32 Arduino Core (framework code, not application code)
- **Apache-2.0:** PlatformIO, clang-format

### Compliance Notes

1. **MIT and BSD-3-Clause:** Permissive licenses, no restrictions on commercial use
2. **LGPL-2.1 (ESP32 Core):** Framework only, application code remains under MIT
3. **Apache-2.0:** Development tools only, not distributed with firmware

**This project is compliant with all dependency licenses.**

## Security Advisories

### Monitoring

Dependencies are monitored for security vulnerabilities via:
1. **Dependabot** - Automated GitHub security alerts
2. **OSV-Scanner** - Open Source Vulnerability scanning in CI
3. **GitHub Advisory Database** - Manual monitoring

### Known Issues

As of 2026-01-24:
- ✅ No known HIGH or CRITICAL vulnerabilities in pinned versions
- ⚠️ Regular monitoring required as new CVEs are disclosed

### Update Policy

- **Security updates:** Applied immediately when available
- **Minor updates:** Monthly review and update cycle
- **Major updates:** Quarterly review with compatibility testing

## Dependency Update Process

1. **Automated:** Dependabot creates PRs for GitHub Actions updates
2. **Manual:** PlatformIO library updates reviewed monthly
3. **Security:** Immediate updates for HIGH/CRITICAL vulnerabilities
4. **Testing:** All updates tested in CI before merge

## Supply Chain Security

### Verification Steps

1. All dependencies sourced from official repositories
2. Version pinning enforced in `platformio.ini`
3. Commit SHA pinning for GitHub Actions
4. Regular vulnerability scanning in CI
5. SBOM generation capability (via PlatformIO)

### Trust Chain

```
Developer → GitHub → PlatformIO Registry → Official Library Repos
         ↓
    CI Build → CodeQL + OSV-Scanner → Verified Artifacts
```

## Additional Resources

- **PlatformIO Registry:** https://registry.platformio.org
- **Arduino Library Index:** https://www.arduinolibraries.info
- **GitHub Advisory Database:** https://github.com/advisories
- **OSV Database:** https://osv.dev

## Maintenance

**Last Updated:** 2026-01-24  
**Review Schedule:** Monthly  
**Maintainer:** tbillion
