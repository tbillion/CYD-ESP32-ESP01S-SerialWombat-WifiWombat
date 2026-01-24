# Security Hardening Plan

**Repository:** tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat  
**Date:** 2026-01-24  
**Version:** 1.0

---

## Executive Summary

This document outlines the comprehensive security hardening plan for the CYD-ESP32-ESP01S firmware project. The current baseline assessment reveals **CRITICAL security vulnerabilities** that must be addressed before deployment in any production or public-facing environment.

**Current Risk Level:** 🔴 **CRITICAL**  
**Target Risk Level:** 🟢 **ACCEPTABLE** (with documented residual risks)

---

## 1. Threat Model

### 1.1 System Overview

**What is this system?**
An ESP32-based IoT device that:
- Provides a web-based UI for managing Serial Wombat I2C devices
- Bridges I2C to TCP for remote access
- Supports local display (TFT + LVGL)
- Enables firmware flashing to connected hardware
- Accepts OTA firmware updates

**Deployment Context:**
- Local network deployment (home, lab, industrial)
- May be exposed to internet via port forwarding (HIGH RISK)
- Physical access assumed in some scenarios (device configuration)
- Multi-user environment possible (shared network)

### 1.2 Assets

**Critical Assets:**
1. **ESP32 Firmware** - The device's operating software
2. **WiFi Credentials** - Network access secrets
3. **Serial Wombat Devices** - Connected hardware being managed
4. **Firmware Images** - Stored in LittleFS or SD card
5. **Configuration Data** - System settings and device state
6. **User Data** - Uploaded files, logs (if any)

**Asset Classification:**
- **Confidentiality:** Medium (WiFi creds, internal network topology)
- **Integrity:** High (firmware modifications = arbitrary code execution)
- **Availability:** Medium (device should remain operational)

### 1.3 Trust Boundaries

```
┌─────────────────────────────────────────────────────────┐
│  UNTRUSTED NETWORK (Internet / Local LAN)              │
│  └─> HTTP Client (browser, curl, malicious actor)      │
└─────────────────┬───────────────────────────────────────┘
                  │ NO AUTHENTICATION
                  ▼
┌─────────────────────────────────────────────────────────┐
│  ESP32 Device (TRUSTED ZONE)                            │
│  ├─> WebServer (port 80) ◀─ BOUNDARY VIOLATION         │
│  ├─> TCP Bridge (port 23) ◀─ BOUNDARY VIOLATION        │
│  ├─> ArduinoOTA          ◀─ BOUNDARY VIOLATION         │
│  ├─> LittleFS (config, firmware)                       │
│  └─> I2C Bus                                            │
└─────────────────┬───────────────────────────────────────┘
                  │
                  ▼
┌─────────────────────────────────────────────────────────┐
│  Serial Wombat Devices (SEMI-TRUSTED)                   │
│  └─> Hardware being managed                             │
└─────────────────────────────────────────────────────────┘
```

**Current Problem:** 🔴 **NO TRUST BOUNDARIES ENFORCED**
- Any network client can access all functionality
- No distinction between admin and regular users
- No authentication at network boundary

### 1.4 Attacker Capabilities

**Threat Actors:**

1. **Remote Network Attacker (Internet)**
   - **Access:** HTTP/TCP to device IP
   - **Capabilities:**
     - Port scanning
     - HTTP requests to all endpoints
     - Credential brute forcing (if auth added)
     - DoS attacks
   - **Goals:** Device compromise, data theft, lateral movement

2. **Local Network Attacker (LAN)**
   - **Access:** Full network access, same subnet
   - **Capabilities:**
     - All remote attacker capabilities
     - ARP spoofing / MITM
     - Packet sniffing (WiFi or wired)
     - mDNS/SSDP discovery
   - **Goals:** Device takeover, network pivot, IoT botnet

3. **Malicious User (Authorized Network Access)**
   - **Access:** Legitimate network user with bad intent
   - **Capabilities:**
     - Abuse authorized access
     - Social engineering
     - Persistent access
   - **Goals:** Sabotage, data theft, persistent backdoor

4. **Physical Attacker (Device Access)**
   - **Access:** Physical access to device
   - **Capabilities:**
     - USB/Serial console access
     - Flash chip reading
     - Hardware debugging (JTAG)
     - SD card swap
   - **Goals:** Firmware extraction, credential theft, hardware mod

### 1.5 Top Risks (Prioritized)

| Risk ID | Threat | Impact | Likelihood | Severity |
|---------|--------|--------|------------|----------|
| R-01 | **Unauthenticated RCE via OTA** | CRITICAL | HIGH | 🔴 CRITICAL |
| R-02 | **Unauthenticated firmware flash to Serial Wombat** | HIGH | HIGH | 🔴 CRITICAL |
| R-03 | **Path traversal in file download/upload** | HIGH | MEDIUM | 🟠 HIGH |
| R-04 | **No HTTPS - credentials in cleartext** | HIGH | HIGH | 🟠 HIGH |
| R-05 | **CSRF on state-changing endpoints** | MEDIUM | HIGH | 🟠 HIGH |
| R-06 | **DoS via large file uploads** | MEDIUM | MEDIUM | 🟡 MEDIUM |
| R-07 | **Open WiFi AP with no password** | MEDIUM | MEDIUM | 🟡 MEDIUM |
| R-08 | **No rate limiting on endpoints** | MEDIUM | MEDIUM | 🟡 MEDIUM |
| R-09 | **Information disclosure in error messages** | LOW | HIGH | 🟡 MEDIUM |
| R-10 | **No firmware signature validation** | HIGH | LOW | 🟡 MEDIUM |

---

## 2. Attack Surface Review

### 2.1 Network Attack Surface

#### HTTP Web Server (Port 80)

**Endpoints Identified (25+ total):**

| Endpoint | Method | Function | Auth | Input Validation | Severity |
|----------|--------|----------|------|------------------|----------|
| `/` | GET | Dashboard | ❌ | N/A | LOW |
| `/scanner` | GET | I2C Scanner UI | ❌ | N/A | LOW |
| `/scan-data` | GET | I2C Scan Results | ❌ | ❌ | MEDIUM |
| `/deepscan` | GET | Deep I2C Scan | ❌ | ❌ | MEDIUM |
| `/connect` | POST | Connect to I2C device | ❌ | ❌ | HIGH |
| `/setpin` | POST | Set device pin mode | ❌ | ❌ | HIGH |
| `/changeaddr` | POST | Change I2C address | ❌ | ❌ | HIGH |
| `/flashfw` | POST | Flash firmware | ❌ | ❌ | 🔴 CRITICAL |
| `/upload_fw` | POST | Upload firmware file | ❌ | ❌ | 🔴 CRITICAL |
| `/upload_hex` | POST | Upload hex file | ❌ | ❌ | 🔴 CRITICAL |
| `/resetwombat` | POST | Reset target device | ❌ | N/A | MEDIUM |
| `/resetwifi` | POST | Reset WiFi settings | ❌ | N/A | MEDIUM |
| `/formatfs` | POST | Format filesystem | ❌ | N/A | 🔴 CRITICAL |
| `/clean_slot` | POST | Clean firmware slot | ❌ | ❌ | MEDIUM |
| `/configure` | GET | Config UI | ❌ | N/A | LOW |
| `/settings` | GET | Settings UI | ❌ | N/A | LOW |
| `/api/system` | GET | System info | ❌ | N/A | MEDIUM |
| `/api/variant` | GET | Device variant | ❌ | N/A | LOW |
| `/api/apply` | POST | Apply config | ❌ | ❌ | HIGH |
| `/api/config/save` | POST | Save config | ❌ | ❌ | HIGH |
| `/api/config/load` | GET | Load config | ❌ | ❌ | MEDIUM |
| `/api/config/list` | GET | List configs | ❌ | ❌ | LOW |
| `/api/config/exists` | GET | Check config | ❌ | ❌ | LOW |
| `/api/config/delete` | GET | Delete config | ❌ | ❌ | MEDIUM |
| `/api/sd/*` | GET/POST | SD card operations | ❌ | ❌ | HIGH |
| `/sd/download` | GET | Download file | ❌ | ❌ | 🔴 CRITICAL |

**Issues:**
- ❌ No authentication on ANY endpoint
- ❌ No HTTPS (cleartext HTTP only)
- ❌ No CORS policy
- ❌ No rate limiting
- ❌ No request size limits
- ❌ No input validation
- ❌ No CSRF protection

#### TCP Bridge (Port 23 or configurable)

**Function:** Bridges I2C to TCP for remote Serial Wombat access

**Issues:**
- ❌ No authentication
- ❌ No encryption
- ❌ No connection limits
- ❌ Arbitrary I2C access

#### ArduinoOTA (Port 3232)

**Function:** Over-the-air firmware updates for ESP32

**Issues:**
- ❌ No authentication visible in code
- ❌ No firmware signature validation
- ❌ Remote code execution vector

#### mDNS/SSDP Discovery

**Function:** Device discovery on network

**Issues:**
- ⚠️ Makes device discoverable (info disclosure)

### 2.2 File System Attack Surface

#### LittleFS Operations

**Accessible Paths:**
- `/config.json` - System configuration
- `/fw/*` - Firmware images
- `/cfg/*` - Saved configurations
- `/temp/*` - Temporary files
- `/hexcache/*` - Cached hex files

**Issues:**
- ❌ No path traversal protection
- ❌ Arbitrary file upload/download
- ❌ No file size limits
- ❌ No filename sanitization

#### SD Card Operations (if enabled)

**Functions:**
- File browser
- File upload/download
- Firmware import
- File rename/delete

**Issues:**
- ❌ Path traversal vulnerabilities
- ❌ Arbitrary file operations
- ❌ No access control

### 2.3 I2C/Hardware Attack Surface

#### Direct Hardware Access

**Functions:**
- I2C scan and connect
- Pin mode configuration
- Device addressing
- Firmware flashing

**Issues:**
- ❌ No access control
- ❌ No operation restrictions
- ❌ Arbitrary hardware manipulation

### 2.4 Code Execution Vectors

1. **OTA Firmware Update** → ESP32 code execution
2. **Serial Wombat Firmware Flash** → Connected device code execution
3. **Configuration Injection** → May lead to crashes or exploitation
4. **Path Traversal** → Read/write arbitrary files → potential code execution via config manipulation

---

## 3. Dependency Supply Chain Risks

### 3.1 Current State

**Problems:**
- ❌ No dependency manifest
- ❌ No version pinning
- ❌ No vulnerability scanning
- ❌ No license compliance tracking
- ❌ No SBOM generation

### 3.2 Dependency Risk Matrix

| Dependency | Version | Known CVEs | Update Cadence | Trust Level |
|------------|---------|------------|----------------|-------------|
| Arduino ESP32 Core | Unknown | Check advisories | Active | HIGH |
| WiFiManager | Unknown | Unknown | Active | MEDIUM |
| ArduinoJson | Unknown | Check advisories | Active | HIGH |
| LovyanGFX | Unknown | Unknown | Active | MEDIUM |
| LVGL | Unknown | Check CVEs | Active | MEDIUM |
| SdFat | Unknown | Unknown | Active | MEDIUM |
| SerialWombat | Unknown | Unknown | Unknown | LOW |

### 3.3 Supply Chain Hardening Plan

**Actions:**
1. Create `library.json` or `platformio.ini` with pinned versions
2. Enable Dependabot for automated vulnerability alerts
3. Add OSV-Scanner to CI pipeline
4. Generate SBOM (CycloneDX format)
5. Document all licenses
6. Set up automated dependency updates with testing

---

## 4. Secrets Handling Plan

### 4.1 Current Secrets

**Identified Secrets:**
1. WiFi SSID and password (WiFiManager)
2. OTA password (if configured)
3. API keys (none currently, but may be added)

**Current Storage:**
- WiFi credentials: WiFiManager (ESP32 NVS storage)
- Configuration: `/config.json` in LittleFS (plaintext)

### 4.2 Secrets Hardening

**Actions:**
1. Document .gitignore entries to prevent secret commits
2. Add secret scanning (gitleaks) to CI
3. Provide .env.example template
4. Document secure credential management
5. Warn against committing WiFi credentials
6. Consider ESP32 secure storage (NVS encryption) for sensitive data
7. Add logging sanitization (never log secrets)

**Residual Risk:**
- Physical attacker can extract flash contents
- Mitigation: Document physical security requirements

---

## 5. Logging & Monitoring Plan

### 5.1 Current State

**Logging:**
- ⚠️ Minimal Serial.println() statements
- ❌ No structured logging
- ❌ No log levels (DEBUG, INFO, WARN, ERROR)
- ❌ No security event logging
- ❌ No audit trail

**Monitoring:**
- ❌ No health checks
- ❌ No metrics
- ❌ No alerting

### 5.2 Logging Implementation Plan

**Required Logging:**
1. **Security Events:**
   - Authentication attempts (when implemented)
   - Failed requests
   - File access (upload/download)
   - Configuration changes
   - Firmware flash operations
   - OTA update attempts

2. **Operational Events:**
   - Boot/restart
   - WiFi connection status
   - I2C errors
   - File system errors

**Implementation:**
- Add structured logging macros
- Use Serial output (115200 baud)
- Add syslog support (optional, for remote logging)
- Sanitize logs (no secrets)

### 5.3 Monitoring Implementation Plan

**Health Checks:**
- Add `/api/health` endpoint (GET, public)
  - Returns: uptime, memory, WiFi status, I2C status
- Add `/api/metrics` endpoint (GET, auth required)
  - Returns: detailed system metrics

**Metrics to Track:**
- Request count per endpoint
- Error rate
- Memory usage
- I2C transaction count
- WiFi signal strength
- Uptime

---

## 6. Security Testing Plan

### 6.1 Static Analysis

**Tools to Add:**
1. **cppcheck** - C++ static analysis
2. **clang-tidy** - Modern C++ linter
3. **CodeQL** - Semantic code analysis (GitHub Advanced Security)

**Focus Areas:**
- Buffer overflows
- Uninitialized variables
- Memory leaks
- Integer overflows
- Format string vulnerabilities

### 6.2 Dependency Scanning

**Tools:**
1. **Dependabot** - GitHub native dependency scanning
2. **OSV-Scanner** - Open Source Vulnerability scanner
3. **arduino-lint** - Arduino-specific linting

### 6.3 Secret Scanning

**Tools:**
1. **gitleaks** - Secret detection in git history
2. **GitHub Secret Scanning** - Native secret detection

### 6.4 Dynamic Testing (Limited)

**Constraints:** Embedded system, no emulator available

**Possible Tests:**
1. Manual security testing checklist
2. Fuzzing inputs (when unit tests added)
3. Static analysis of network code

### 6.5 Manual Security Review

**Checklist:**
- [x] Baseline assessment complete
- [ ] Code review for vulnerabilities (PHASE 3)
- [ ] Endpoint security review
- [ ] File operation security review
- [ ] Input validation review
- [ ] Error handling review

---

## 7. Implementation Roadmap

### Phase 2: Dependency Hardening (2-3 hours)

1. ✅ Create `platformio.ini` with board + library versions
2. ✅ Add Dependabot config
3. ✅ Add OSV-Scanner to CI
4. ✅ Document licenses

### Phase 3: Code Hardening (8-12 hours)

#### Priority 1: Authentication (CRITICAL)
- [ ] Add HTTP Basic Auth or API key to all endpoints
- [ ] Add OTA password
- [ ] Add admin/user role separation (optional)

#### Priority 2: Input Validation (HIGH)
- [ ] Validate all POST parameters
- [ ] Sanitize filenames
- [ ] Add path traversal protection
- [ ] Add request size limits
- [ ] Add JSON schema validation

#### Priority 3: Network Security (HIGH)
- [ ] Add CORS policy
- [ ] Add security headers (CSP, X-Frame-Options, etc.)
- [ ] Add rate limiting
- [ ] Add HTTPS guidance (with self-signed cert example)

#### Priority 4: Error Handling (MEDIUM)
- [ ] Centralized error handler
- [ ] Sanitize error messages
- [ ] Add structured logging
- [ ] Never expose stack traces

#### Priority 5: File Operations (HIGH)
- [ ] Path normalization
- [ ] Whitelist allowed directories
- [ ] Filename sanitization
- [ ] File size limits

#### Priority 6: Observability (MEDIUM)
- [ ] Add `/api/health` endpoint
- [ ] Add structured logging
- [ ] Add security event logging

### Phase 4: Quality Gates (2-3 hours)

1. ✅ Add clang-format configuration
2. ✅ Add arduino-lint checks
3. ✅ Add pre-commit hooks
4. ✅ Add basic unit tests (if time allows)

### Phase 5: CI/CD (3-4 hours)

1. ✅ GitHub Actions build workflow
2. ✅ Security scanning workflow
3. ✅ Pin action versions (SHA)
4. ✅ Add release workflow

### Phase 6: Documentation (4-6 hours)

1. ✅ Complete README.md
2. ✅ Add SECURITY.md
3. ✅ Add RUNBOOK.md
4. ✅ Add architecture diagram
5. ✅ Document all config options
6. ✅ Add troubleshooting guide

**Total Estimated Effort:** 20-30 hours

---

## 8. Acceptance Criteria

### Security Gates

**Minimum Requirements:**
- ✅ Authentication on all state-changing endpoints
- ✅ Input validation on all user inputs
- ✅ Path traversal protection
- ✅ Request size limits
- ✅ Rate limiting
- ✅ CORS policy
- ✅ Security headers
- ✅ Secret scanning in CI
- ✅ Dependency scanning in CI
- ✅ No known HIGH/CRITICAL vulnerabilities

**Nice-to-Have:**
- HTTPS support (with documentation)
- Firmware signature validation
- mTLS for OTA
- Hardware security module integration

### Documentation Gates

**Minimum Requirements:**
- ✅ Complete README.md
- ✅ SECURITY.md
- ✅ RUNBOOK.md
- ✅ All config options documented
- ✅ Deployment guide
- ✅ Troubleshooting guide

### CI/CD Gates

**Minimum Requirements:**
- ✅ Build passes
- ✅ Security scans pass (no HIGH/CRITICAL)
- ✅ Linting passes
- ✅ Dependency scan passes

---

## 9. Residual Risks (Post-Hardening)

**Accepted Risks:**

1. **Physical Security**
   - **Risk:** Physical attacker can extract firmware
   - **Mitigation:** Document physical security requirements
   - **Acceptance:** Cannot fully protect against physical access

2. **HTTP (not HTTPS)**
   - **Risk:** Traffic interception on local network
   - **Mitigation:** Provide HTTPS setup guide, recommend VPN/tunnel
   - **Acceptance:** HTTPS on ESP32 has performance/memory tradeoffs

3. **No Firmware Signing**
   - **Risk:** Malicious firmware can be uploaded (if auth bypassed)
   - **Mitigation:** Strong authentication required
   - **Acceptance:** Implementing secure boot is complex for Arduino

4. **Embedded Platform Constraints**
   - **Risk:** Limited memory prevents some hardening techniques
   - **Mitigation:** Document limitations, use best practices within constraints
   - **Acceptance:** Embedded platform inherent limitations

---

## 10. Next Steps

1. **Proceed to Phase 2:** Dependency hardening
2. **Update this document** as implementation progresses
3. **Track findings** in security issue tracker
4. **Review and approve** each phase before proceeding

**Status:** ✅ PLAN APPROVED - PROCEED WITH IMPLEMENTATION
