# Message System Verification & Gauntlet Tests

## Overview

This document provides detailed verification procedures to ensure the PLC-style MessageCenter system is fully functional. The "gauntlet" tests intentionally trigger various message types to validate the complete message lifecycle: post → active → acknowledge → history.

## Pre-Flight Checklist

Before running gauntlet tests, verify:
- [ ] MessageCenter builds without errors
- [ ] All subsystems include message posting
- [ ] LVGL Messages screen exists
- [ ] Web /messages route exists
- [ ] Badge indicators present (LVGL + Web)

## Gauntlet Test Suite

### Test 1: Basic Message Posting

**Objective**: Verify INFO/WARN/ERROR messages post correctly

**Procedure**:
1. Add a test menu item "Message Gauntlet" in LVGL UI
2. On press, post three messages:
   ```cpp
   msg_info("test", TEST_INFO, "Test Info", "This is an informational test message");
   msg_warn("test", TEST_WARN, "Test Warning", "This is a warning test message");
   msg_error("test", TEST_ERROR, "Test Error", "This is an error test message");
   ```
3. Check Messages screen

**Expected Results**:
- [ ] 3 messages appear in Active list
- [ ] Badge shows "3" with RED indicator (highest severity)
- [ ] Messages display correct severity icons
- [ ] Messages show timestamp, source="test", titles

**Web Verification**:
- [ ] Navigate to `/messages`
- [ ] Verify same 3 messages in Active tab
- [ ] Badge count matches LVGL

---

### Test 2: Message Coalescing

**Objective**: Verify duplicate messages increment count instead of creating new entries

**Procedure**:
1. Post the same message 5 times:
   ```cpp
   for (int i = 0; i < 5; i++) {
     msg_warn("test", TEST_COALESCE, "Duplicate Test", "Message #%d", i+1);
     delay(500);
   }
   ```
2. Check Messages screen

**Expected Results**:
- [ ] Only ONE message in Active list
- [ ] Message shows count="5" or "Occurred 5 times"
- [ ] Last timestamp updates with each occurrence
- [ ] Details update to "Message #5" (latest)
- [ ] Badge count = 1 (only 1 unique message)

**Web Verification**:
- [ ] Confirm count field = 5
- [ ] Confirm only 1 entry with same code

---

### Test 3: Acknowledgment Flow

**Objective**: Verify ACK moves message from Active to History

**Procedure**:
1. Ensure Active has 3 test messages (from Test 1)
2. Tap first message to open detail view
3. Tap "ACK" button
4. Return to Active list

**Expected Results**:
- [ ] Active list now shows 2 messages
- [ ] Badge count updates to "2"
- [ ] Badge color still RED (ERROR remaining)
- [ ] History tab shows 1 message (the acked one)
- [ ] Acked message is dimmed in History

**Web Verification**:
- [ ] POST /api/messages/ack with msg_id
- [ ] Response: 200 OK
- [ ] Active list updates (polling or WS)

---

### Test 4: ACK ALL

**Objective**: Verify ACK ALL clears all active messages

**Procedure**:
1. Ensure Active has 2+ messages
2. Tap "ACK ALL" button
3. Confirm dialog (if present)

**Expected Results**:
- [ ] Active list is empty
- [ ] Badge disappears or shows "0"
- [ ] All messages moved to History
- [ ] History shows all previous messages

**Web Verification**:
- [ ] POST /api/messages/ack_all
- [ ] Active list empties

---

### Test 5: Clear History

**Objective**: Verify CLEAR HISTORY removes history entries

**Procedure**:
1. Switch to History tab
2. Verify history has messages
3. Tap "CLEAR HISTORY" button
4. Confirm dialog

**Expected Results**:
- [ ] History tab is empty
- [ ] Message: "History cleared"
- [ ] Persistent history file deleted or emptied

**Web Verification**:
- [ ] POST /api/messages/clear_history
- [ ] History list empties

---

### Test 6: Message Persistence Across Reboot

**Objective**: Verify history persists after reboot

**Procedure**:
1. Post 3 messages, acknowledge all
2. Verify History has 3 entries
3. Reboot system (power cycle or ESP.restart())
4. After boot, navigate to Messages > History

**Expected Results**:
- [ ] History shows 3 messages from before reboot
- [ ] Timestamps preserved
- [ ] Details preserved

**Optional**: Verify active message persistence
- Post 1 message, do NOT ack
- Reboot
- Expected: Message reappears in Active (if active persistence enabled)

---

### Test 7: Re-Latching After ACK

**Objective**: Verify same fault re-occurs after acknowledgment

**Procedure**:
1. Post message: `msg_warn("test", TEST_RELATCH, "Re-Latch Test", "Fault #1")`
2. ACK the message
3. Verify Active is empty
4. Post same message again: `msg_warn("test", TEST_RELATCH, "Re-Latch Test", "Fault #2")`

**Expected Results**:
- [ ] Message reappears in Active (new instance)
- [ ] Count resets to 1 (not coalesced with acked message)
- [ ] Badge reappears with count=1
- [ ] History retains first occurrence

---

### Test 8: Boot Message Verification

**Objective**: Verify all boot stages post messages

**Procedure**:
1. Reboot system
2. Watch serial console for boot messages
3. After boot complete, open Messages > History

**Expected Results**:
- [ ] Serial console shows all boot stages (BOOT_01, BOOT_02, ..., BOOT_OK_READY)
- [ ] History contains boot messages:
  - [ ] BOOT_START
  - [ ] BOOT_01_EARLY_OK
  - [ ] BOOT_02_CONFIG_OK (or WARN if defaults)
  - [ ] BOOT_03_FS_OK
  - [ ] BOOT_04_SD_* (OK, NOT_PRESENT, or FAIL)
  - [ ] BOOT_05_DISPLAY_OK
  - [ ] BOOT_06_TOUCH_OK (or WARN if no touch)
  - [ ] BOOT_07_NET_OK (or AP_FALLBACK)
  - [ ] BOOT_08_TIME_OK (or FAIL)
  - [ ] BOOT_09_SERVICES_OK
  - [ ] BOOT_OK_READY or BOOT_DEGRADED
- [ ] If any WARN/ERROR during boot, Active list shows them

---

### Test 9: Boot Failure Simulation

**Objective**: Test system behavior when boot stages fail

**Procedure** (requires dev mode or manual code injection):
1. Enable "BOOT_GAUNTLET_MODE" in config
2. Gauntlet mode simulates failures:
   - Force SD_NOT_PRESENT (unplug SD before boot)
   - Simulate Touch failure (disconnect touch controller)
   - Simulate WiFi failure (set wrong SSID/password)
3. Boot system

**Expected Results**:
- [ ] Boot continues despite failures
- [ ] BOOT_DEGRADED message posted
- [ ] Active list shows failures:
  - [ ] BOOT_04_SD_NOT_PRESENT (WARN)
  - [ ] BOOT_06_TOUCH_FAIL (WARN)
  - [ ] BOOT_07_NET_AP_FALLBACK (WARN)
- [ ] Badge shows "3" with YELLOW (no ERROR)
- [ ] System functional in degraded mode
- [ ] Web UI accessible (via AP or Ethernet)

**Critical Failure Test**:
1. Simulate LittleFS mount failure (corrupt flash partition)
2. Expected: BOOT_03_FS_FAIL (ERROR), system enters RAM-only mode

---

### Test 10: Runtime Subsystem Messages

**Objective**: Verify subsystems post runtime messages

**Procedure**:
1. **SD Card**: Eject SD via web UI → expect `SD_EJECTED` (INFO)
2. **WiFi**: Disconnect WiFi AP → expect `NET_WIFI_DISCONNECTED` (WARN)
3. **Web Auth**: Attempt login with wrong password → expect `WEB_AUTH_FAIL` (WARN)
4. **Config**: Save config via web UI → expect `CFG_SAVE_OK` (INFO)
5. **OTA**: Initiate OTA update → expect `OTA_UPDATE_START` (INFO)

**Expected Results**:
- [ ] Each action posts corresponding message
- [ ] Messages appear in real-time (no page refresh)
- [ ] Severity and codes match ORIGINS_MATRIX.md

---

### Test 11: Badge and Severity Indicator

**Objective**: Verify badge reflects highest active severity

**Setup**:
1. Clear all active messages
2. Post INFO → badge BLUE (or no color)
3. Post WARN → badge YELLOW
4. Post ERROR → badge RED
5. ACK ERROR → badge returns to YELLOW
6. ACK WARN → badge returns to BLUE
7. ACK INFO → badge disappears

**Expected Results**:
- [ ] Badge color always reflects highest active severity
- [ ] Badge count accurate at each step
- [ ] Color transitions smooth

---

### Test 12: Web API Polling/WebSocket

**Objective**: Verify web UI updates without full page refresh

**Procedure**:
1. Open `/messages` in browser
2. In another tab, trigger gauntlet test (post 3 messages)
3. Return to `/messages` tab (do NOT refresh)

**Expected Results (if WebSocket enabled)**:
- [ ] New messages appear automatically
- [ ] Badge updates without refresh

**Expected Results (if polling)**:
- [ ] Messages appear within polling interval (e.g., 5s)
- [ ] Sequence-based polling efficient (only fetches if seq changed)

---

### Test 13: Health Snapshot Integration

**Objective**: Verify dashboard derives health from active messages

**Procedure**:
1. Clear all active messages
2. Check dashboard: overall_health = "OK"
3. Post WARN message
4. Check dashboard: overall_health = "WARN"
5. Post ERROR message
6. Check dashboard: overall_health = "ERROR"
7. ACK ERROR, leave WARN
8. Check dashboard: overall_health = "WARN"

**Expected Results**:
- [ ] Dashboard health status updates correctly
- [ ] LVGL dashboard shows status
- [ ] Web /api/health or /api/messages/summary returns health

---

### Test 14: Message Limits and Overflow

**Objective**: Ensure system handles large message counts

**Procedure**:
1. Post 150 unique messages (different codes)
2. Check Active list
3. Post 50 more (total 200)

**Expected Results**:
- [ ] System handles gracefully (no crash)
- [ ] If limit enforced (e.g., 100 active max):
  - [ ] Oldest INFO messages auto-acked or dropped
  - [ ] WARN/ERROR always retained
- [ ] Performance remains acceptable (list scrolling smooth)

---

### Test 15: Cross-Subsystem Message Origins

**Objective**: Verify messages originate from real subsystems (not just test module)

**Required Message Origins** (sample):
- [ ] **boot**: Any boot stage message
- [ ] **fs**: LittleFS mount message
- [ ] **sd**: SD card status message
- [ ] **config**: Config load/save message
- [ ] **display**: Display init message
- [ ] **touch**: Touch init message
- [ ] **net**: WiFi connect/disconnect message
- [ ] **web**: Web server start message
- [ ] **ota**: OTA event message
- [ ] **i2c**: I2C scan message
- [ ] **serialwombat**: SerialWombat status message

**Verification**:
- Trigger each subsystem event
- Confirm message posted with correct source
- Confirm code matches ORIGINS_MATRIX.md

---

## Automated Test Script (Optional)

Create a test script accessible via serial or web:

```cpp
void runMessageGauntlet() {
  Serial.println("=== MESSAGE GAUNTLET START ===");
  
  // Test 1: Basic posting
  msg_info("test", TEST_INFO, "Gauntlet INFO", "Test info message");
  msg_warn("test", TEST_WARN, "Gauntlet WARN", "Test warning message");
  msg_error("test", TEST_ERROR, "Gauntlet ERROR", "Test error message");
  delay(1000);
  
  // Test 2: Coalescing
  for (int i = 0; i < 5; i++) {
    msg_warn("test", TEST_COALESCE, "Coalesce Test", "Occurrence #%d", i+1);
    delay(100);
  }
  
  // Test 3: Verify counts
  Serial.printf("Active count: %d\n", MessageCenter::getInstance().getActiveCount());
  Serial.printf("Highest severity: %d\n", MessageCenter::getInstance().getHighestActiveSeverity());
  
  Serial.println("=== MESSAGE GAUNTLET COMPLETE ===");
  Serial.println("Check LVGL Messages screen and Web /messages");
}
```

Invoke via:
- Serial command: `gauntlet`
- Web endpoint: `GET /api/test/gauntlet`
- LVGL button: "Run Gauntlet"

---

## Regression Testing

After any MessageCenter changes, re-run:
1. **Basic Posting** (Test 1)
2. **Coalescing** (Test 2)
3. **ACK Flow** (Test 3)
4. **Boot Messages** (Test 8)
5. **Runtime Messages** (Test 10)

These 5 tests cover 80% of functionality.

---

## Performance Benchmarks

### Post Latency
- **Target**: < 1ms per post (without persistence)
- **Measure**: Time from `msg_*()` call to message in active list

### ACK Latency
- **Target**: < 5ms per ack (with persistence)
- **Measure**: Time from ACK button press to active list update

### UI Update Latency
- **Target**: < 100ms for badge/list refresh
- **Measure**: Time from message post to UI reflecting change

### Memory Usage
- **Baseline**: Free heap before MessageCenter init
- **Loaded**: Free heap with 50 active messages
- **Target**: < 20KB RAM for message storage

---

## Known Issues / Edge Cases

### Issue 1: Rapid Message Posting
**Scenario**: 100 messages/second  
**Expected**: Coalescing prevents overflow, only unique messages retained  
**Test**: Stress test with loop posting same message 1000x

### Issue 2: Persistence During OTA
**Scenario**: OTA update in progress, message posted  
**Expected**: Message saved to RAM, persisted after OTA completes (or lost if reboot during write)  
**Test**: Post message, immediately trigger OTA, verify message after update

### Issue 3: Concurrent Access
**Scenario**: Web UI ACK + LVGL ACK at same time  
**Expected**: Mutex prevents race condition, one succeeds first  
**Test**: Automated script ACKs same message from two threads

---

## Success Criteria

The MessageCenter system is considered **production-ready** when:
- [x] All 15 gauntlet tests pass
- [x] Boot sequence posts messages for all stages
- [x] All subsystems emit messages (per ORIGINS_MATRIX.md)
- [x] LVGL UI fully functional (badge, Messages screen, ACK)
- [x] Web UI fully functional (API, badge, messages page)
- [x] History persists across reboots
- [x] Performance benchmarks met
- [x] No memory leaks over 24h continuous operation

---

## Reporting Results

Document test results in this format:

```
Gauntlet Test Results - <Date>
Firmware Version: <version>
Board: <ESP32-S3 / ESP32>

Test 1 (Basic Posting):       ✅ PASS
Test 2 (Coalescing):           ✅ PASS
Test 3 (ACK Flow):             ✅ PASS
Test 4 (ACK ALL):              ✅ PASS
Test 5 (Clear History):        ✅ PASS
Test 6 (Persistence):          ✅ PASS
Test 7 (Re-Latching):          ✅ PASS
Test 8 (Boot Messages):        ✅ PASS
Test 9 (Boot Failures):        ⚠️  PARTIAL (SD test skipped)
Test 10 (Runtime Messages):    ✅ PASS
Test 11 (Badge/Severity):      ✅ PASS
Test 12 (Web Updates):         ✅ PASS
Test 13 (Health Snapshot):     ✅ PASS
Test 14 (Message Limits):      ✅ PASS
Test 15 (Cross-Subsystem):     ✅ PASS

Overall: ✅ PASS (1 partial)
Notes: Test 9 partial due to hardware unavailability (no SD card)
```

---

## References

- Message Origins Matrix: `ORIGINS_MATRIX.md`
- System Specification: `SPEC.md`
- Boot Sequence: `BOOT_SEQUENCE.md`
