# Threat Model

**Project:** CYD-ESP32-ESP01S-SerialWombat-WifiWombat  
**Version:** 1.0  
**Date:** 2026-01-24  
**Status:** Initial Assessment

---

## 1. System Description

### 1.1 Purpose

This is an ESP32-based firmware for Cheap Yellow Display (CYD) hardware that provides:
- Web-based management interface for Serial Wombat I2C devices
- Local TFT display with LVGL GUI (optional)
- TCP bridge for I2C remote access
- Firmware flashing capabilities
- SD card file management
- Over-the-air (OTA) firmware updates

### 1.2 Deployment Environment

**Typical Deployment:**
- Home lab / maker environment
- Industrial automation (small scale)
- Educational settings
- IoT prototyping

**Network Exposure:**
- Connected to local WiFi network
- May be exposed to internet via port forwarding (HIGH RISK)
- Accessible to all devices on same network

**Physical Access:**
- Device typically in accessible location
- USB/Serial console exposed
- SD card slot accessible
- Flash chip accessible (with disassembly)

---

## 2. Assets

### 2.1 Asset Inventory

| Asset ID | Asset Name | Description | Confidentiality | Integrity | Availability |
|----------|-----------|-------------|----------------|-----------|--------------|
| A-01 | ESP32 Firmware | Device operating software | MEDIUM | CRITICAL | HIGH |
| A-02 | WiFi Credentials | Network access secrets | HIGH | MEDIUM | LOW |
| A-03 | Serial Wombat Devices | Connected I2C hardware | LOW | HIGH | MEDIUM |
| A-04 | Firmware Images | Stored .bin/.hex files | LOW | HIGH | LOW |
| A-05 | Configuration Data | System settings | MEDIUM | HIGH | MEDIUM |
| A-06 | User Files | SD card contents | MEDIUM | MEDIUM | LOW |
| A-07 | I2C Bus Access | Hardware control | LOW | CRITICAL | HIGH |
| A-08 | Network Access | LAN connectivity | MEDIUM | MEDIUM | HIGH |

### 2.2 Asset Criticality

**CRITICAL (Loss = System Compromise):**
- A-01: ESP32 Firmware (integrity)
- A-07: I2C Bus Access (integrity)

**HIGH (Loss = Significant Impact):**
- A-02: WiFi Credentials (confidentiality)
- A-03: Serial Wombat Devices (integrity)
- A-04: Firmware Images (integrity)
- A-05: Configuration Data (integrity)

**MEDIUM (Loss = Limited Impact):**
- A-06: User Files (varies by content)

---

## 3. Threat Actors

### 3.1 Attacker Profiles

#### TA-01: Remote Network Attacker

**Motivation:** Opportunistic compromise, IoT botnet recruitment  
**Skills:** Low to Medium (script kiddie to competent hacker)  
**Access:** Internet-facing IP (if port-forwarded) or VPN into network  
**Resources:** Automated scanning tools, exploit frameworks

**Capabilities:**
- Port scanning and service enumeration
- HTTP request fuzzing
- Credential brute forcing (if auth present)
- DoS attacks
- Exploit known vulnerabilities

**Goals:**
- Device compromise for botnet inclusion
- Lateral movement into network
- Data theft
- Persistent backdoor installation

#### TA-02: Local Network Attacker

**Motivation:** Targeted attack, industrial espionage, sabotage  
**Skills:** Medium to High (competent attacker)  
**Access:** Same local network (WiFi or Ethernet)  
**Resources:** Network analysis tools, custom exploits

**Capabilities:**
- All remote attacker capabilities
- Packet sniffing (WiFi or wired)
- Man-in-the-middle (MITM) attacks
- ARP spoofing
- Network DoS
- Physical proximity attacks

**Goals:**
- Device control
- Network reconnaissance
- Data interception
- Persistent access

#### TA-03: Malicious Insider

**Motivation:** Sabotage, theft, competitive advantage  
**Skills:** Low to High (varies)  
**Access:** Authorized network access (employee, guest, contractor)  
**Resources:** Legitimate credentials, internal knowledge

**Capabilities:**
- Abuse authorized access
- Social engineering
- Install malicious firmware
- Exfiltrate data
- Sabotage operations

**Goals:**
- Disrupt operations
- Steal intellectual property
- Plant backdoor
- Cover tracks

#### TA-04: Physical Attacker

**Motivation:** Targeted exploitation, firmware reverse engineering  
**Skills:** High (hardware hacking skills)  
**Access:** Physical access to device  
**Resources:** Hardware tools (JTAG, flash programmers, oscilloscope)

**Capabilities:**
- Firmware extraction via flash chip
- UART/Serial console access
- JTAG debugging
- SD card manipulation
- Hardware trojan installation
- Side-channel attacks

**Goals:**
- Firmware extraction
- Secret recovery
- Hardware modification
- Persistent implant

---

## 4. Attack Scenarios

### 4.1 High-Severity Scenarios

#### AS-01: Unauthenticated Remote Code Execution via OTA

**Threat Actor:** TA-01 (Remote) or TA-02 (Local)  
**Attack Vector:** ArduinoOTA service (port 3232)  
**Prerequisites:** Network access to device

**Attack Steps:**
1. Discover device via port scan or mDNS
2. Connect to OTA service (no authentication observed)
3. Upload malicious firmware
4. Device reboots with attacker firmware

**Impact:**
- Complete device compromise
- Potential for lateral network movement
- Persistent backdoor
- Denial of service

**Current Defenses:** ❌ NONE  
**Residual Risk:** 🔴 CRITICAL

**Mitigation:**
- Add OTA password authentication
- Implement firmware signature validation
- Add IP whitelist for OTA access
- Document secure OTA configuration

---

#### AS-02: Firmware Injection into Serial Wombat Devices

**Threat Actor:** TA-01, TA-02, or TA-03  
**Attack Vector:** `/flashfw` and `/upload_fw` endpoints  
**Prerequisites:** Network access

**Attack Steps:**
1. Access web interface (no auth)
2. Upload malicious firmware via `/upload_fw`
3. Flash to connected Serial Wombat device via `/flashfw`
4. Malicious firmware runs on hardware

**Impact:**
- Hardware device compromise
- Potential physical damage (e.g., motor control abuse)
- Loss of device functionality
- Data corruption

**Current Defenses:** ❌ NONE  
**Residual Risk:** 🔴 CRITICAL

**Mitigation:**
- Add authentication to web interface
- Implement firmware signature validation
- Add confirmation prompts
- Log all flash operations

---

#### AS-03: Arbitrary File Read/Write via Path Traversal

**Threat Actor:** TA-01, TA-02, or TA-03  
**Attack Vector:** `/sd/download` and `/api/sd/upload` endpoints  
**Prerequisites:** Network access, SD card enabled

**Attack Steps:**
1. Access web interface
2. Request `/sd/download?path=../../../../config.json`
3. Download sensitive configuration
4. Upload malicious files via path traversal
5. Overwrite system files or inject backdoor

**Impact:**
- Confidential data disclosure
- Configuration tampering
- Potential code execution
- System instability

**Current Defenses:** ⚠️ MINIMAL (basic path checks may exist)  
**Residual Risk:** 🟠 HIGH

**Mitigation:**
- Implement strict path validation
- Whitelist allowed directories
- Normalize all paths
- Add file extension restrictions
- Add authentication

---

#### AS-04: WiFi Credential Theft

**Threat Actor:** TA-02 (Local) or TA-04 (Physical)  
**Attack Vector:** Network sniffing or flash extraction  
**Prerequisites:** Network proximity or physical access

**Attack Steps:**
1. **Network:** Capture HTTP traffic (no HTTPS)
2. **Physical:** Extract flash chip, read credentials from NVS
3. Use credentials for network access

**Impact:**
- Unauthorized network access
- Lateral movement
- Persistent access
- Privacy violation

**Current Defenses:** ⚠️ PARTIAL (NVS storage, but no transport encryption)  
**Residual Risk:** 🟠 HIGH

**Mitigation:**
- Implement HTTPS for web interface
- Document physical security requirements
- Consider additional credential encryption
- Use WPA3 where supported

---

#### AS-05: Denial of Service via Resource Exhaustion

**Threat Actor:** TA-01 or TA-02  
**Attack Vector:** HTTP endpoints with large payloads  
**Prerequisites:** Network access

**Attack Steps:**
1. Send large file upload requests
2. Send rapid requests (no rate limiting)
3. Exhaust memory or storage
4. Device crashes or becomes unresponsive

**Impact:**
- Service disruption
- Device reboot required
- Loss of availability

**Current Defenses:** ❌ NONE  
**Residual Risk:** 🟡 MEDIUM

**Mitigation:**
- Implement request size limits
- Add rate limiting per IP
- Add connection limits
- Add watchdog timer
- Add memory monitoring

---

### 4.2 Medium-Severity Scenarios

#### AS-06: Cross-Site Request Forgery (CSRF)

**Threat Actor:** TA-01 or TA-02  
**Attack Vector:** State-changing endpoints without CSRF protection  
**Prerequisites:** Victim browsing malicious site while authenticated

**Attack Steps:**
1. Victim accesses device web UI
2. Victim visits attacker's website
3. Attacker's site makes request to device (e.g., `/formatfs`)
4. Device executes request (no CSRF token)

**Impact:**
- Unintended configuration changes
- Data loss
- Service disruption

**Current Defenses:** ❌ NONE  
**Residual Risk:** 🟡 MEDIUM

**Mitigation:**
- Add CSRF tokens
- Check Origin/Referer headers
- Add CORS policy
- Use SameSite cookies (when auth added)

---

#### AS-07: Information Disclosure via Error Messages

**Threat Actor:** TA-01, TA-02, or TA-03  
**Attack Vector:** Any endpoint that may error  
**Prerequisites:** Network access

**Attack Steps:**
1. Send malformed requests
2. Trigger errors
3. Observe verbose error messages
4. Learn system internals (paths, versions, etc.)

**Impact:**
- System fingerprinting
- Reconnaissance for further attacks
- Path disclosure

**Current Defenses:** ⚠️ UNKNOWN (needs code review)  
**Residual Risk:** 🟡 MEDIUM

**Mitigation:**
- Sanitize all error messages
- Never expose stack traces
- Use generic error messages for users
- Log detailed errors internally only

---

#### AS-08: Session Hijacking (when auth added)

**Threat Actor:** TA-02 (Local)  
**Attack Vector:** HTTP traffic interception (no HTTPS)  
**Prerequisites:** Network access, authentication implemented

**Attack Steps:**
1. Sniff network traffic
2. Capture authentication tokens/cookies
3. Replay tokens to impersonate user

**Impact:**
- Unauthorized access
- Account takeover

**Current Defenses:** ❌ NONE (no auth yet, no HTTPS)  
**Residual Risk:** 🟡 MEDIUM (future concern)

**Mitigation:**
- Implement HTTPS
- Use secure, HTTP-only cookies
- Implement session expiry
- Add token rotation

---

### 4.3 Low-Severity Scenarios

#### AS-09: Device Fingerprinting

**Threat Actor:** TA-01 or TA-02  
**Attack Vector:** mDNS, HTTP headers, banner grabbing  
**Prerequisites:** Network access

**Attack Steps:**
1. Scan network for devices
2. Identify device type via HTTP response headers
3. Target device with specific exploits

**Impact:**
- Reconnaissance
- Targeted attack preparation

**Current Defenses:** ⚠️ PARTIAL (mDNS may be present)  
**Residual Risk:** 🟢 LOW

**Mitigation:**
- Remove version numbers from HTTP responses
- Minimize mDNS announcements
- Use generic server headers
- Implement network segmentation

---

#### AS-10: Unencrypted Configuration Backup

**Threat Actor:** TA-03 or TA-04  
**Attack Vector:** Configuration file download  
**Prerequisites:** Authorized access or physical access

**Attack Steps:**
1. Download `/config.json`
2. Extract sensitive settings
3. Use for further attacks or reconnaissance

**Impact:**
- Information disclosure
- Configuration replication

**Current Defenses:** ⚠️ PARTIAL (file system access control)  
**Residual Risk:** 🟢 LOW

**Mitigation:**
- Add authentication
- Encrypt sensitive config fields
- Audit configuration downloads

---

## 5. Attack Trees

### 5.1 Goal: Compromise ESP32 Device

```
[Compromise ESP32 Device]
├── [Remote Network Attack]
│   ├── [Exploit OTA] ✓ (AS-01)
│   ├── [Exploit Web Endpoint]
│   │   ├── [Path Traversal] ✓ (AS-03)
│   │   ├── [File Upload RCE] ✓ (AS-02)
│   │   └── [CSRF] ✓ (AS-06)
│   └── [Exploit TCP Bridge]
│       └── [Arbitrary I2C Commands]
├── [Local Network Attack]
│   ├── [MITM + Session Hijack] ✓ (AS-08)
│   ├── [WiFi Credential Sniff] ✓ (AS-04)
│   └── [DoS → Reboot → Attack] ✓ (AS-05)
└── [Physical Attack]
    ├── [Flash Extraction] ✓ (AS-04)
    ├── [Serial Console Access]
    └── [JTAG Debug]
```

### 5.2 Goal: Compromise Serial Wombat Hardware

```
[Compromise Serial Wombat Device]
├── [Malicious Firmware Flash] ✓ (AS-02)
│   ├── [Via Web Interface (No Auth)]
│   └── [Via Compromised ESP32]
├── [Malicious I2C Commands]
│   ├── [Via TCP Bridge]
│   └── [Via Web API]
└── [Physical Hardware Attack]
    └── [Direct I2C Access]
```

---

## 6. Data Flow Diagram (DFD)

```
┌────────────────────────────────────────────────────────────────┐
│  External Actors                                               │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐                │
│  │ Web User │    │ TCP User │    │   OTA    │                │
│  │ (Browser)│    │ (Client) │    │ (Tool)   │                │
│  └─────┬────┘    └─────┬────┘    └─────┬────┘                │
└────────┼───────────────┼───────────────┼────────────────────────┘
         │               │               │
         │ HTTP          │ TCP           │ OTA Protocol
         │ (Port 80)     │ (Port 23)     │ (Port 3232)
         ▼               ▼               ▼
┌────────────────────────────────────────────────────────────────┐
│  ESP32 Device (Trust Boundary)                                 │
│  ┌──────────────────────────────────────────────────────────┐ │
│  │  Network Layer (WiFi)                                     │ │
│  │  ├─> WebServer     [NO AUTH] ⚠️                          │ │
│  │  ├─> TCP Bridge    [NO AUTH] ⚠️                          │ │
│  │  └─> ArduinoOTA    [NO AUTH] ⚠️                          │ │
│  └──────────────┬───────────────────────────┬────────────────┘ │
│                 │                           │                   │
│  ┌──────────────▼───────────┐  ┌───────────▼────────────────┐ │
│  │  Storage Layer           │  │  I2C Layer                 │ │
│  │  ├─> LittleFS            │  │  └─> Wire (I2C Master)     │ │
│  │  │   ├─> /config.json    │  └─────────────┬──────────────┘ │
│  │  │   ├─> /fw/*.bin       │                │                │
│  │  │   └─> /temp/*         │                │                │
│  │  └─> SD Card (optional)  │                │                │
│  │      ├─> User files       │                │                │
│  │      └─> Firmware images  │                │                │
│  └────────────────────────────┘                │                │
└────────────────────────────────────────────────┼────────────────┘
                                                 │ I2C Bus
                                                 ▼
                                    ┌────────────────────────┐
                                    │ Serial Wombat Devices  │
                                    │ (Target Hardware)      │
                                    └────────────────────────┘

Legend:
  ⚠️  = Vulnerability / Weakness
  🔒 = Security Control (none currently)
```

**Trust Boundaries:**
1. **Network → ESP32:** ⚠️ NO AUTHENTICATION, NO ENCRYPTION
2. **ESP32 → Storage:** ⚠️ NO ACCESS CONTROL
3. **ESP32 → I2C Devices:** ⚠️ NO AUTHORIZATION

---

## 7. Security Controls (Current State)

### 7.1 Existing Controls

| Control ID | Control Name | Status | Effectiveness |
|------------|-------------|--------|---------------|
| C-01 | WiFiManager authentication | ✅ Partial | LIMITED (initial setup only) |
| C-02 | LittleFS file system | ✅ Yes | LIMITED (no access control) |
| C-03 | NVS credential storage | ✅ Yes | LIMITED (no encryption) |
| C-04 | Compile-time feature flags | ✅ Yes | LOW (SD/Display optional) |

### 7.2 Missing Controls (Critical)

| Control ID | Control Name | Priority | Status |
|------------|-------------|----------|--------|
| C-10 | HTTP Authentication | 🔴 CRITICAL | ❌ NOT IMPLEMENTED |
| C-11 | HTTPS/TLS | 🟠 HIGH | ❌ NOT IMPLEMENTED |
| C-12 | OTA Authentication | 🔴 CRITICAL | ❌ NOT IMPLEMENTED |
| C-13 | Input Validation | 🟠 HIGH | ❌ NOT IMPLEMENTED |
| C-14 | Path Traversal Protection | 🟠 HIGH | ❌ NOT IMPLEMENTED |
| C-15 | Rate Limiting | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |
| C-16 | Request Size Limits | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |
| C-17 | CSRF Protection | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |
| C-18 | CORS Policy | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |
| C-19 | Security Headers | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |
| C-20 | Firmware Signing | 🟠 HIGH | ❌ NOT IMPLEMENTED |
| C-21 | Audit Logging | 🟡 MEDIUM | ❌ NOT IMPLEMENTED |

---

## 8. Risk Summary

### 8.1 Risk Matrix

| Risk ID | Threat Scenario | Likelihood | Impact | Risk Level | Priority |
|---------|----------------|-----------|--------|-----------|----------|
| R-01 | Unauthenticated OTA RCE (AS-01) | HIGH | CRITICAL | 🔴 CRITICAL | P0 |
| R-02 | Firmware injection (AS-02) | HIGH | HIGH | 🔴 CRITICAL | P0 |
| R-03 | Path traversal (AS-03) | MEDIUM | HIGH | 🟠 HIGH | P1 |
| R-04 | WiFi cred theft (AS-04) | HIGH | HIGH | 🟠 HIGH | P1 |
| R-05 | DoS (AS-05) | MEDIUM | MEDIUM | 🟡 MEDIUM | P2 |
| R-06 | CSRF (AS-06) | MEDIUM | MEDIUM | 🟡 MEDIUM | P2 |
| R-07 | Info disclosure (AS-07) | HIGH | LOW | 🟡 MEDIUM | P2 |
| R-08 | Session hijack (AS-08) | MEDIUM | MEDIUM | 🟡 MEDIUM | P3 |
| R-09 | Fingerprinting (AS-09) | HIGH | LOW | 🟢 LOW | P4 |
| R-10 | Config disclosure (AS-10) | LOW | LOW | 🟢 LOW | P4 |

### 8.2 Overall Risk Rating

**Current Risk Posture:** 🔴 **CRITICAL - UNSAFE FOR DEPLOYMENT**

**Primary Concerns:**
1. Unauthenticated remote code execution vectors
2. No encryption of sensitive data in transit
3. No access control on any functionality
4. Multiple high-severity vulnerabilities

**Recommendation:** ❌ **DO NOT DEPLOY** until P0 and P1 risks mitigated.

---

## 9. Mitigation Priorities

### Priority 0 (CRITICAL - MUST FIX)

1. **Implement Authentication**
   - Add HTTP Basic Auth or API key to web server
   - Add OTA password
   - Add IP whitelist capability

2. **Fix Remote Code Execution**
   - Secure OTA with password + optional firmware signing
   - Secure firmware flash endpoints with auth
   - Add operation confirmation

### Priority 1 (HIGH - SHOULD FIX)

3. **Input Validation**
   - Validate all POST parameters
   - Sanitize filenames and paths
   - Add path traversal protection
   - Add request size limits

4. **Transport Security**
   - Document HTTPS setup
   - Provide self-signed cert generation guide
   - Add secure WiFi configuration guidance

### Priority 2 (MEDIUM - NICE TO FIX)

5. **Defense in Depth**
   - Add CORS policy
   - Add CSRF protection
   - Add rate limiting
   - Add security headers

6. **Monitoring & Response**
   - Add structured logging
   - Add security event logging
   - Add health check endpoint

### Priority 3 (LOW - FUTURE)

7. **Advanced Security**
   - Firmware signature validation
   - mTLS for OTA
   - Hardware security module integration

---

## 10. Review and Maintenance

**Review Schedule:**
- Initial review: Completed 2026-01-24
- Post-hardening review: After Phase 3 implementation
- Ongoing reviews: Quarterly or after major changes

**Change Management:**
- Update threat model when new features added
- Re-assess risk after security incidents
- Document new threats as discovered

**Stakeholders:**
- Development team
- Security team (if applicable)
- End users (for awareness)

---

**Document Version:** 1.0  
**Last Updated:** 2026-01-24  
**Next Review:** Post-hardening implementation
