# MessageCenter Verification Test Suite

## Overview
This document provides a comprehensive test suite to validate the complete PLC-style MessageCenter implementation.

## Test Categories

### 1. Core MessageCenter Tests

#### Test 1.1: Message Posting
**Objective:** Verify messages can be posted with all severity levels

**Steps:**
1. Call gauntlet test: `GET /api/test/gauntlet`
2. Check response: `{"success": true}`
3. Query active messages: `GET /api/messages/active`

**Expected Results:**
- ✅ 4 messages in active list
- ✅ 1 INFO message (TEST_INFO)
- ✅ 2 WARN messages (TEST_WARN, TEST_COALESCE with count=5)
- ✅ 1 ERROR message (TEST_ERROR)

#### Test 1.2: Message Coalescing
**Objective:** Verify duplicate messages increment count

**Expected Results:**
- ✅ TEST_COALESCE message has count=5
- ✅ Only ONE message instance with that code
- ✅ Last timestamp updated
- ✅ Details show "Occurrence #5"

#### Test 1.3: Message Acknowledgment
**Objective:** Verify ACK moves message to history

**Steps:**
1. Get first message ID from active list
2. ACK: `POST /api/messages/ack {"msg_id": <id>}`
3. Query active and history

**Expected Results:**
- ✅ Active count decreased by 1
- ✅ Message moved to history
- ✅ Sequence number incremented

#### Test 1.4: Acknowledge All
**Objective:** Verify ACK ALL clears all active messages

**Steps:**
1. Ensure some active messages exist
2. Call: `POST /api/messages/ack_all`
3. Query summary and active

**Expected Results:**
- ✅ Active count = 0
- ✅ All messages in history
- ✅ Badge disappears (web) or shows 0 (LVGL)

#### Test 1.5: Clear History
**Objective:** Verify history can be cleared

**Steps:**
1. Ensure history has messages
2. Call: `POST /api/messages/clear_history`
3. Query history

**Expected Results:**
- ✅ History count = 0
- ✅ History list empty
- ✅ File `/messages/history.json` deleted or empty

#### Test 1.6: Message Persistence
**Objective:** Verify history persists across reboots

**Steps:**
1. Post gauntlet messages
2. ACK all (move to history)
3. Reboot device
4. Query history after boot

**Expected Results:**
- ✅ History messages still present
- ✅ Timestamps preserved
- ✅ Counts preserved

---

### 2. Boot Stage Tests

#### Test 2.1: Normal Boot Sequence
**Objective:** Verify all boot stages post messages

**Steps:**
1. Perform clean boot
2. Monitor serial output
3. Check Messages > History

**Expected Results:**
- ✅ BOOT_01_EARLY_OK
- ✅ BOOT_02_CONFIG_OK (or WARN)
- ✅ BOOT_03_FS_OK
- ✅ BOOT_04_SD_OK or BOOT_04_SD_NOT_PRESENT
- ✅ BOOT_05_DISPLAY_OK (or DISABLED)
- ✅ BOOT_06_TOUCH_OK (or DISABLED)
- ✅ BOOT_07_NET_OK (or AP_FALLBACK)
- ✅ BOOT_08_TIME_OK (or FAIL if no internet)
- ✅ BOOT_09_SERVICES_OK
- ✅ BOOT_OK_READY

#### Test 2.2: Boot with Missing SD Card
**Objective:** Verify degraded boot with SD warning

**Steps:**
1. Remove SD card (if applicable)
2. Reboot
3. Check messages

**Expected Results:**
- ✅ BOOT_04_SD_NOT_PRESENT (WARN)
- ✅ BOOT_DEGRADED message
- ✅ System continues to operate

#### Test 2.3: Boot with WiFi Failure
**Objective:** Verify AP fallback mode

**Steps:**
1. Configure invalid WiFi credentials
2. Reboot
3. Check messages

**Expected Results:**
- ✅ BOOT_07_NET_AP_FALLBACK (WARN)
- ✅ AP mode "Wombat-Setup" active
- ✅ System continues to operate

#### Test 2.4: Boot in Headless Mode
**Objective:** Verify display disabled warning

**Steps:**
1. Set `display_enable = false` in config
2. Reboot
3. Check messages via web UI

**Expected Results:**
- ✅ BOOT_05_DISPLAY_DISABLED (WARN)
- ✅ LVGL not initialized
- ✅ Web UI still accessible

---

### 3. Subsystem Integration Tests

#### Test 3.1: Config Save Messages
**Objective:** Verify config save posts message

**Steps:**
1. Modify configuration via web UI
2. Save config
3. Check messages

**Expected Results:**
- ✅ CFG_SAVE_OK (INFO) if successful
- ✅ CFG_SAVE_FAIL (ERROR) if failure

#### Test 3.2: I2C and SerialWombat Init
**Objective:** Verify hardware init messages

**Steps:**
1. Check serial output during boot
2. Look for I2C and SW messages

**Expected Results:**
- ✅ I2C_INIT_BEGIN
- ✅ I2C_BUS_OK
- ✅ SW_INIT_BEGIN
- ✅ SW_INIT_OK

#### Test 3.3: OTA Update Messages
**Objective:** Verify OTA events post messages

**Steps:**
1. Trigger OTA update
2. Monitor messages

**Expected Results:**
- ✅ OTA_UPDATE_START (INFO)
- ✅ OTA_UPDATE_OK (INFO) on success
- ✅ OTA_UPDATE_FAIL (ERROR) on failure

#### Test 3.4: TCP Bridge and Web Server
**Objective:** Verify service startup messages

**Steps:**
1. Check boot messages
2. Verify services started

**Expected Results:**
- ✅ WEB_SERVER_START (INFO)
- ✅ TCP_BRIDGE_START (INFO)

---

### 4. LVGL UI Tests

#### Test 4.1: Message Badge Visibility
**Objective:** Verify badge appears on status bar

**Steps:**
1. Post gauntlet messages
2. Check LVGL status bar

**Expected Results:**
- ✅ Badge shows "🔔 4"
- ✅ Badge color is RED (ERROR severity)
- ✅ Badge is clickable

#### Test 4.2: Messages Screen Navigation
**Objective:** Verify messages screen can be accessed

**Steps:**
1. Tap badge or navigate to messages screen
2. Verify screen appears

**Expected Results:**
- ✅ Screen shows two tabs: Active | History
- ✅ Active tab selected by default
- ✅ Messages list visible

#### Test 4.3: Active Messages List
**Objective:** Verify active messages display correctly

**Expected Results:**
- ✅ Messages show severity icon
- ✅ Timestamps displayed
- ✅ Titles displayed
- ✅ "Acknowledge All" button visible

#### Test 4.4: Message Detail View
**Objective:** Verify detail popup works

**Steps:**
1. Tap a message in list
2. Check popup content

**Expected Results:**
- ✅ Modal popup appears
- ✅ All fields visible (severity, source, code, timestamp, count, title, details)
- ✅ "Acknowledge" button visible (for active)
- ✅ "Close" button works

#### Test 4.5: LVGL Acknowledgment Flow
**Objective:** Verify ACK works from LVGL

**Steps:**
1. Open message detail
2. Tap "Acknowledge"
3. Check lists update

**Expected Results:**
- ✅ Message moves from Active to History
- ✅ Badge count decreases
- ✅ Lists refresh automatically

#### Test 4.6: Badge Auto-Refresh
**Objective:** Verify badge updates automatically

**Steps:**
1. Post message via web UI or serial
2. Wait up to 1 second
3. Check LVGL badge

**Expected Results:**
- ✅ Badge count updates (no manual refresh needed)

---

### 5. Web UI Tests

#### Test 5.1: Messages Page Access
**Objective:** Verify /messages route works

**Steps:**
1. Navigate to `http://<device-ip>/messages`

**Expected Results:**
- ✅ Page loads successfully
- ✅ Two tabs visible: Active | History
- ✅ Auto-refresh every 5 seconds

#### Test 5.2: Web Badge Display
**Objective:** Verify badge in web UI (if implemented)

**Expected Results:**
- ✅ Badge shows active count
- ✅ Badge color matches severity
- ✅ Badge updates via polling

#### Test 5.3: Web Acknowledgment
**Objective:** Verify web ACK buttons work

**Steps:**
1. Click message to expand
2. Click "Acknowledge"

**Expected Results:**
- ✅ Message moves to History tab
- ✅ Badge count updates
- ✅ No page refresh needed

---

### 6. API Tests

#### Test 6.1: /api/messages/summary
**Request:**
```bash
curl http://<device-ip>/api/messages/summary
```

**Expected Response:**
```json
{
  "active_count": 4,
  "history_count": 0,
  "highest_severity": "ERROR",
  "sequence": 5
}
```

#### Test 6.2: /api/messages/active
**Request:**
```bash
curl http://<device-ip>/api/messages/active
```

**Expected Response:**
```json
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
```

#### Test 6.3: /api/health
**Request:**
```bash
curl http://<device-ip>/api/health
```

**Expected Response:**
```json
{
  "status": "ERROR",
  "overall_health": 2,
  "boot_complete": true,
  "boot_degraded": false,
  "active_messages": 4,
  "error_count": 1,
  "warn_count": 2,
  "info_count": 1,
  "subsystems": {
    "filesystem": true,
    "sd_present": false,
    "display": true,
    "network": true,
    "services": true
  },
  "uptime_ms": 123456,
  "heap_free": 250000,
  "wifi_connected": true,
  "wifi_rssi": -45,
  "ip": "192.168.1.100"
}
```

---

### 7. Security Tests

#### Test 7.1: Default Password Warning
**Objective:** Verify security warning appears

**Setup:**
- Ensure `AUTH_PASSWORD == "CHANGE_ME_NOW"`

**Expected Results:**
- ✅ ERROR message: SEC_DEFAULT_PASSWORD
- ✅ Badge shows ERROR (red)
- ✅ Message details warn about security

#### Test 7.2: Security Disabled Warning
**Objective:** Verify security disabled warning

**Setup:**
- Set `SECURITY_ENABLED = 0`

**Expected Results:**
- ✅ WARN message: SEC_DISABLED
- ✅ Message advises enabling for production

---

### 8. Stress Tests

#### Test 8.1: Many Messages
**Objective:** Verify system handles 100+ messages

**Steps:**
1. Post 100 unique messages rapidly
2. Check performance

**Expected Results:**
- ✅ All messages stored
- ✅ No crashes or hangs
- ✅ UI remains responsive
- ✅ Memory usage acceptable

#### Test 8.2: Coalescing Stress
**Objective:** Verify coalescing handles 1000+ duplicates

**Steps:**
1. Post same message 1000 times
2. Check result

**Expected Results:**
- ✅ Only 1 message stored
- ✅ Count = 1000
- ✅ Performance acceptable

#### Test 8.3: Persistence with Large History
**Objective:** Verify persistence with 500+ history messages

**Steps:**
1. Generate 500 messages
2. ACK all
3. Reboot

**Expected Results:**
- ✅ History loads after reboot
- ✅ No corruption
- ✅ Boot time acceptable

---

## Automated Test Script

Create a file `test_gauntlet.sh`:

```bash
#!/bin/bash

DEVICE_IP="192.168.1.100"  # Change to your device IP
BASE_URL="http://${DEVICE_IP}"

echo "=== MessageCenter Gauntlet Tests ==="
echo ""

# Test 1: Gauntlet
echo "Test 1: Running gauntlet..."
curl -s "${BASE_URL}/api/test/gauntlet" | jq .
sleep 1

# Test 2: Check summary
echo ""
echo "Test 2: Checking summary..."
curl -s "${BASE_URL}/api/messages/summary" | jq .
sleep 1

# Test 3: Get active messages
echo ""
echo "Test 3: Getting active messages..."
curl -s "${BASE_URL}/api/messages/active" | jq .
sleep 1

# Test 4: ACK first message
echo ""
echo "Test 4: Acknowledging first message..."
FIRST_ID=$(curl -s "${BASE_URL}/api/messages/active" | jq -r '.[0].id')
curl -s -X POST -H "Content-Type: application/json" \
  -d "{\"msg_id\":${FIRST_ID}}" \
  "${BASE_URL}/api/messages/ack" | jq .
sleep 1

# Test 5: Check history
echo ""
echo "Test 5: Checking history..."
curl -s "${BASE_URL}/api/messages/history" | jq .
sleep 1

# Test 6: Health snapshot
echo ""
echo "Test 6: Checking health..."
curl -s "${BASE_URL}/api/health" | jq .

echo ""
echo "=== Tests Complete ==="
```

Make executable: `chmod +x test_gauntlet.sh`

Run: `./test_gauntlet.sh`

---

## Test Results Template

Use this template to record test results:

```
MessageCenter Verification - Test Results
Date: _______________
Firmware Version: _______________
Device: _______________

[ ] Test 1.1: Message Posting - PASS / FAIL
[ ] Test 1.2: Message Coalescing - PASS / FAIL
[ ] Test 1.3: Message Acknowledgment - PASS / FAIL
[ ] Test 1.4: Acknowledge All - PASS / FAIL
[ ] Test 1.5: Clear History - PASS / FAIL
[ ] Test 1.6: Message Persistence - PASS / FAIL

[ ] Test 2.1: Normal Boot Sequence - PASS / FAIL
[ ] Test 2.2: Boot with Missing SD Card - PASS / FAIL
[ ] Test 2.3: Boot with WiFi Failure - PASS / FAIL
[ ] Test 2.4: Boot in Headless Mode - PASS / FAIL

[ ] Test 3.1: Config Save Messages - PASS / FAIL
[ ] Test 3.2: I2C and SerialWombat Init - PASS / FAIL
[ ] Test 3.3: OTA Update Messages - PASS / FAIL
[ ] Test 3.4: TCP Bridge and Web Server - PASS / FAIL

[ ] Test 4.1: Message Badge Visibility - PASS / FAIL / N/A
[ ] Test 4.2: Messages Screen Navigation - PASS / FAIL / N/A
[ ] Test 4.3: Active Messages List - PASS / FAIL / N/A
[ ] Test 4.4: Message Detail View - PASS / FAIL / N/A
[ ] Test 4.5: LVGL Acknowledgment Flow - PASS / FAIL / N/A
[ ] Test 4.6: Badge Auto-Refresh - PASS / FAIL / N/A

[ ] Test 5.1: Messages Page Access - PASS / FAIL
[ ] Test 5.2: Web Badge Display - PASS / FAIL
[ ] Test 5.3: Web Acknowledgment - PASS / FAIL

[ ] Test 6.1: /api/messages/summary - PASS / FAIL
[ ] Test 6.2: /api/messages/active - PASS / FAIL
[ ] Test 6.3: /api/health - PASS / FAIL

[ ] Test 7.1: Default Password Warning - PASS / FAIL
[ ] Test 7.2: Security Disabled Warning - PASS / FAIL

[ ] Test 8.1: Many Messages - PASS / FAIL
[ ] Test 8.2: Coalescing Stress - PASS / FAIL
[ ] Test 8.3: Persistence with Large History - PASS / FAIL

Overall Result: _______________
Notes:
```

---

## Troubleshooting

### Issue: No messages appear
**Check:**
- MessageCenter::begin() called?
- Correct API endpoint?
- Serial output for errors?

### Issue: Messages not persisting
**Check:**
- LittleFS mounted?
- `/messages` directory exists?
- Disk space available?

### Issue: LVGL badge not updating
**Check:**
- `updateMessageBadge()` being called?
- LVGL loop running?
- Display initialized?

### Issue: Web UI not showing messages
**Check:**
- JavaScript console for errors
- API endpoints returning data?
- Browser cache (hard refresh)

---

## Acceptance Criteria

The MessageCenter system is considered **fully functional** when:

✅ All core message operations work (post, ack, history, clear)
✅ All 11 boot stages post messages
✅ At least 5 subsystems post operational messages
✅ LVGL UI shows messages with working badge
✅ Web UI shows messages with all controls working
✅ Messages persist across reboots
✅ Health snapshot correctly derives system state
✅ No memory leaks or crashes under stress
✅ Documentation is complete and accurate
