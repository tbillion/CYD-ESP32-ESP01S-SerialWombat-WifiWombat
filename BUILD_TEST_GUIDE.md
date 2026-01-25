# PLC-Style MessageCenter - Build & Test Guide

## Quick Start

### Prerequisites

- PlatformIO Core installed
- ESP32-S3 or ESP32 board
- USB cable for flashing/serial monitoring

### Build Instructions

```bash
# Clone repository (if not already)
git clone https://github.com/tbillion/CYD-ESP32-ESP01S-SerialWombat-WifiWombat.git
cd CYD-ESP32-ESP01S-SerialWombat-WifiWombat

# Checkout message system branch
git checkout copilot/integrate-message-ack-system

# Build for ESP32-S3 (default)
platformio run -e esp32-s3-devkit

# Or build for other targets
platformio run -e esp32-devkit        # Standard ESP32
platformio run -e esp32-2432S028      # CYD 2.8" (ILI9341)
platformio run -e esp32-8048S070      # CYD 7" (RGB 800x480)
```

### Flash Instructions

```bash
# Flash firmware to device
platformio run -e esp32-s3-devkit -t upload

# Monitor serial output
platformio device monitor -b 115200
```

### Expected Serial Output

On boot, you should see:

```
[INFO] boot: System Boot - Boot sequence initiated
[INFO] boot: Early Initialization - Serial console ready @ 115200 baud
[INFO] boot: Filesystem Mount - LittleFS mounted successfully
[INFO] boot: Configuration Load - Configuration loaded successfully
[INFO] boot: Display Initialization - Display ready
[INFO] boot: Network Initialization - WiFi connected: YourNetwork (IP: 192.168.1.100)
[INFO] boot: Services Initialization - Web server (port 80) and TCP bridge (port 3000) started
[INFO] boot: Boot Complete - System ready for operation
```

If default password detected:
```
[ERROR] security: Default Password Detected - CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY - System is NOT secure
```

## Web UI Testing

### Access Messages UI

1. **Find Device IP**:
   - Check serial monitor for IP address after boot
   - Or connect to "Wombat-Setup" AP if WiFi failed

2. **Open Web Browser**:
   ```
   http://<device-ip>/
   ```

3. **Navigate to Messages**:
   - Click "Messages" link in navigation
   - Or go directly to `http://<device-ip>/messages`

### Test Gauntlet (Verification)

Trigger test messages:

```bash
# Via curl
curl http://<device-ip>/api/test/gauntlet

# Or open in browser
http://<device-ip>/api/test/gauntlet
```

Expected result:
```json
{
  "success": true,
  "message": "Gauntlet test complete. Check Messages screen."
}
```

Go to Messages screen (`/messages`) and verify:
- ✅ 3 individual test messages (INFO, WARN, ERROR)
- ✅ 1 coalesced message with count=5
- ✅ Badge shows count=4 with RED indicator (highest severity)

### Test Message Acknowledgment

1. **View Active Messages**:
   - Active tab should show test messages + any boot warnings

2. **Acknowledge Single Message**:
   - Click a message to open detail view
   - Click "Acknowledge" button
   - Message moves to History tab
   - Badge count decreases

3. **Acknowledge All**:
   - Click "Acknowledge All" button in Active tab
   - Confirm dialog
   - All messages move to History
   - Badge disappears

4. **Clear History**:
   - Switch to History tab
   - Click "Clear History" button
   - Confirm dialog
   - History empties

### Test Message Persistence

1. **Create Messages**:
   - Run gauntlet test: `http://<device-ip>/api/test/gauntlet`
   - Acknowledge all messages (moves to History)

2. **Reboot Device**:
   ```bash
   # Power cycle or soft reset
   curl http://<device-ip>/api/system/reboot  # (if implemented)
   # Or press reset button
   ```

3. **Verify Persistence**:
   - After reboot, go to Messages > History
   - Previous messages should still be present
   - Timestamps preserved

## API Testing

### cURL Examples

```bash
# Get message summary
curl http://<device-ip>/api/messages/summary

# Expected:
{
  "active_count": 4,
  "history_count": 0,
  "highest_severity": "ERROR",
  "sequence": 5
}

# Get active messages
curl http://<device-ip>/api/messages/active

# Expected:
[
  {
    "id": 1,
    "timestamp": 12345,
    "last_ts": 12345,
    "severity": "INFO",
    "source": "test",
    "code": "TEST_INFO",
    "title": "Gauntlet INFO Test",
    "details": "This is an informational test message",
    "count": 1
  },
  ...
]

# Acknowledge a message
curl -X POST -H "Content-Type: application/json" \
  -d '{"msg_id":1}' \
  http://<device-ip>/api/messages/ack

# Expected:
{"success":true}

# Acknowledge all
curl -X POST http://<device-ip>/api/messages/ack_all

# Clear history
curl -X POST http://<device-ip>/api/messages/clear_history
```

## Boot Message Verification

### Successful Boot (All OK)

Serial output should show:
```
[INFO] BOOT_START
[INFO] BOOT_01_EARLY_OK
[INFO] BOOT_02_CONFIG_OK
[INFO] BOOT_03_FS_OK
[INFO] BOOT_05_DISPLAY_OK
[INFO] BOOT_07_NET_OK
[INFO] BOOT_09_SERVICES_OK
[INFO] BOOT_OK_READY
```

Web UI Messages > History should contain all boot messages.

### Degraded Boot (Warnings)

**Scenario: No SD card inserted**

Serial output:
```
[INFO] BOOT_START
...
[WARN] BOOT_04_SD_NOT_PRESENT: No SD card detected, continuing without SD
...
[WARN] BOOT_DEGRADED: System operational with 0 errors, 1 warnings
```

**Scenario: Display disabled (headless)**

```
[WARN] BOOT_05_DISPLAY_DISABLED: Display disabled (headless mode)
```

**Scenario: WiFi failed (AP mode)**

```
[WARN] BOOT_07_NET_AP_FALLBACK: WiFi failed, AP mode active: 'Wombat-Setup'
```

### Boot with Errors

**Scenario: Config parse error**

```
[ERROR] BOOT_02_CONFIG_FAIL: Config parse error, cannot continue
```

**Scenario: Display init failed**

```
[ERROR] BOOT_05_DISPLAY_FAIL: Display initialization failed
```

## Security Message Verification

### Default Password Warning

**Condition**: `AUTH_PASSWORD == "CHANGE_ME_NOW"` in `app.cpp`

Serial output:
```
[ERROR] security: Default Password Detected - CHANGE AUTH_PASSWORD IN CODE IMMEDIATELY
```

Web UI:
- Active messages shows ERROR with SEC_DEFAULT_PASSWORD
- Badge shows "1" with RED indicator

**Fix**: Change AUTH_PASSWORD in `src/app/app.cpp` line 43:
```cpp
#define AUTH_PASSWORD "YourSecurePassword123"
```

Rebuild and reflash. Error should not appear.

### Security Disabled Warning

**Condition**: `SECURITY_ENABLED == 0`

Serial output:
```
[WARN] security: Security Disabled - Authentication is DISABLED - Enable for production use
```

## OTA Message Verification

### Trigger OTA Update

```bash
# Using Arduino OTA (requires Arduino IDE or espota.py)
platformio run -e esp32-s3-devkit -t upload --upload-port <device-ip>
```

Expected messages:
```
[INFO] ota: OTA Update Started - Type: firmware
[INFO] ota: OTA Update Complete - Rebooting...
```

On failure:
```
[ERROR] ota: OTA Update Failed - Authentication Failed
```

## Troubleshooting

### Issue: No messages in web UI

**Check**:
1. Serial console for boot messages
2. API endpoint directly: `http://<device-ip>/api/messages/active`
3. JavaScript console for errors (F12 in browser)

**Solution**:
- Ensure MessageCenter::begin() called in App::begin()
- Check for compilation errors in message_center.cpp

### Issue: Messages not persisting

**Check**:
1. LittleFS mounted successfully: Look for `BOOT_03_FS_OK`
2. /messages directory exists: Check via file explorer (if available)

**Solution**:
- Format LittleFS: `http://<device-ip>/formatfs` (⚠️ erases all data)
- Or manually format via serial: `LittleFS.format()`

### Issue: Gauntlet test fails

**Check**:
1. Route registered: Look for `/api/test/gauntlet` in App::initWebServer()
2. TEST_* codes defined in message_codes.h

**Solution**:
- Rebuild firmware with latest code
- Check serial for error messages

### Issue: Badge not updating

**Check**:
1. JavaScript console for errors
2. Sequence number changing: `/api/messages/summary`

**Solution**:
- Hard refresh browser (Ctrl+Shift+R)
- Check auto-refresh timer (5 seconds)

## Performance Testing

### Memory Usage

Check free heap:

```bash
curl http://<device-ip>/api/system | grep heap
```

Expected:
- Before messages: ~250KB free (ESP32-S3)
- With 50 active messages: ~230KB free (20KB used)

### Boot Time

Measure with serial timestamps:

```
[0ms] BOOT_START
[50ms] BOOT_01_EARLY_OK
[200ms] BOOT_02_CONFIG_OK
[300ms] BOOT_03_FS_OK
[1000ms] BOOT_05_DISPLAY_OK
[5000ms] BOOT_07_NET_OK
[5500ms] BOOT_09_SERVICES_OK
[5600ms] BOOT_OK_READY
```

Total boot time: ~5.6 seconds (WiFi is slowest)

### Message Throughput

Stress test:

```cpp
for (int i = 0; i < 1000; i++) {
  msg_info("stress", "TEST_STRESS", "Test", "Iteration %d", i);
}
```

Expected: 1000 messages posted in <100ms (due to coalescing, only 1 stored)

## Verification Checklist

Use this checklist to verify complete functionality:

### Core Functionality
- [ ] Build completes without errors
- [ ] Flash succeeds
- [ ] Serial shows boot messages
- [ ] Web UI loads at device IP
- [ ] /messages route accessible

### Message Posting
- [ ] Gauntlet test posts 4 messages
- [ ] Messages appear in Active tab
- [ ] Badge shows correct count (4)
- [ ] Badge color is RED (highest severity)

### Message Details
- [ ] Click message opens detail view
- [ ] Detail shows all fields (severity, source, code, timestamp, count, details)
- [ ] ACK button present for active messages

### Acknowledgment
- [ ] Single ACK moves message to History
- [ ] Badge count decreases
- [ ] ACK ALL clears all active
- [ ] History shows acked messages

### Persistence
- [ ] Reboot preserves history
- [ ] Timestamps correct
- [ ] Count preserved

### Boot Integration
- [ ] All boot stages post messages
- [ ] Boot complete message appears
- [ ] Security warnings appear (if applicable)
- [ ] Display warnings appear (if headless)

### API Endpoints
- [ ] /api/messages/summary returns JSON
- [ ] /api/messages/active returns array
- [ ] /api/messages/history returns array
- [ ] /api/messages/ack accepts POST
- [ ] /api/messages/ack_all works
- [ ] /api/messages/clear_history works

## Known Issues

1. **LVGL UI Not Yet Implemented**: Only web UI available in this phase
2. **Some Boot Stages Stubbed**: BOOT_04_SD, BOOT_06_TOUCH, BOOT_08_TIME not fully integrated
3. **No Real-Time Updates Without Polling**: WebSocket not implemented, uses 5s polling
4. **Auth Not on Message Routes**: /messages and /api/messages/* are public (should add auth)

## Next Steps

After verifying basic functionality:

1. **Add Remaining Boot Stages**:
   - Implement SD card detection (BOOT_04_SD)
   - Implement touch init tracking (BOOT_06_TOUCH)
   - Add NTP time sync (BOOT_08_TIME)

2. **Deep Subsystem Integration**:
   - Config save/validate messages
   - I2C scan messages
   - Auth failure tracking
   - Firmware flash progress

3. **LVGL UI**:
   - Messages screen in LVGL
   - Status bar badge
   - Boot progress overlay

4. **Health Snapshot**:
   - Overall system health derivation
   - Dashboard integration

5. **Production Hardening**:
   - Add auth to message routes
   - Implement WebSocket for real-time updates
   - Add message rate limiting
   - Optimize persistence (batch writes)

## Documentation

For detailed information:

- **System Architecture**: `docs/messages/SPEC.md`
- **All Message Origins**: `docs/messages/ORIGINS_MATRIX.md`
- **Boot Sequence Details**: `docs/messages/BOOT_SEQUENCE.md`
- **Complete Test Suite**: `docs/messages/GAUNTLET.md`
- **Implementation Changes**: `MESSAGECENTER_CHANGES.md`

## Support

If you encounter issues:

1. Check serial console output
2. Review `/messages` web UI
3. Run gauntlet test for diagnostics
4. Check documentation in `docs/messages/`
5. Review code comments in `src/core/messages/`

---

**Last Updated**: 2026-01-24  
**Branch**: copilot/integrate-message-ack-system  
**Status**: Ready for Testing
