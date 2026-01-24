# MessageCenter System Specification

## Overview

The MessageCenter is a PLC-style message/acknowledgment system that serves as the **single source of truth** for all operator-visible status across both LVGL and Web UIs. Every significant event during boot, runtime, and all feature modules must originate a centralized message with stable codes.

## Design Principles

### 1. First-Class Subsystem
- MessageCenter is a core dependency, initialized very early in boot
- Globally available via singleton pattern
- No placeholders, no TODOs - production-ready from day one

### 2. PLC-Style Acknowledgment
- All messages require operator acknowledgment by default
- Messages transition: **ACTIVE** (unacknowledged) → **HISTORY** (acknowledged)
- Messages persist until explicitly acknowledged
- Duplicate messages coalesce (increment count + update timestamp)

### 3. Stable Message Codes
- Every message has a unique, stable code (e.g., `BOOT_03_FS_OK`, `NET_WIFI_CONNECTED`)
- Codes are defined in `/src/core/messages/message_codes.h`
- Codes NEVER change - they are part of the API contract

### 4. Three Severity Levels

| Severity | Meaning | Examples | Operator Action |
|----------|---------|----------|-----------------|
| **INFO** | Normal operational events requiring acknowledgment | Boot complete, SD mounted, Config saved | Acknowledge when reviewed |
| **WARN** | Degraded operation, system continues | SD not present, MQTT disconnected, Touch failed | Review and acknowledge; may need attention |
| **ERROR** | Fault condition requiring immediate attention | Display init failed, FS mount failed, Overcurrent | Investigate root cause, fix, acknowledge |

### 5. Operator Model
- **Active Messages**: Unacknowledged, requiring operator attention
- **History**: Acknowledged messages, kept for audit trail
- **Badge Count**: Number of active messages
- **Severity Indicator**: Highest active severity (ERROR > WARN > INFO)

## Architecture

### Core Components

```
MessageCenter
├── Message
│   ├── id (uint32_t, auto-increment)
│   ├── timestamp (uint32_t, millis())
│   ├── severity (INFO/WARN/ERROR)
│   ├── source (e.g., "boot", "fs", "net")
│   ├── code (e.g., BOOT_03_FS_OK)
│   ├── title (short, 32 chars)
│   ├── details (longer, 128 chars)
│   ├── count (coalesce counter)
│   └── acknowledged (bool)
│
├── Active List (unacknowledged)
├── History List (acknowledged)
└── Sequence Number (for efficient polling)
```

### API Surface

#### Posting Messages
```cpp
// High-level helpers
msg_info(source, code, title, fmt, ...);
msg_warn(source, code, title, fmt, ...);
msg_error(source, code, title, fmt, ...);

// Direct API
MessageCenter::post(severity, source, code, title, details);
```

#### Acknowledgment
```cpp
MessageCenter::acknowledge(msg_id);
MessageCenter::acknowledgeAll();
MessageCenter::clearHistory();
```

#### Queries
```cpp
MessageCenter::getActiveMessages();
MessageCenter::getHistoryMessages();
MessageCenter::getActiveCount();
MessageCenter::getHighestActiveSeverity();
MessageCenter::getSequence(); // for polling
```

### Boot Integration

```cpp
// Boot step helpers
boot_step_begin(step_id, label);
boot_step_ok(step_id, details);
boot_step_warn(step_id, details);
boot_step_fail(step_id, details);
```

These automatically:
1. Post messages (INFO/WARN/ERROR)
2. Update BootSummary
3. Trigger UI updates

### Message Coalescing

When a duplicate message is posted (same {severity, source, code}):
- **Increment** count
- **Update** last_ts
- **Optionally update** details with latest information
- **Do NOT** create a new message
- **Increment** sequence number (triggers UI refresh)

## Persistence

### History Persistence
- History messages saved to LittleFS: `/messages/history.json`
- Format: JSON array, append-only with periodic compaction
- Includes: id, timestamp, severity, source, code, title, details, count
- Loaded on boot to restore audit trail

### Active Persistence (Optional)
- Active messages may be persisted to survive reboots
- Implementation: Same format as history, separate file
- Use case: Fault that caused reboot should re-appear on restart

### Robustness
- Checksum validation on load
- Corrupted file → log warning, start fresh
- Append journal + periodic compact to prevent unbounded growth
- Max history size: 1000 messages (configurable)

## UI Integration

### LVGL UI

#### Status Bar Badge
- Position: Top-right corner
- Display: Badge with count + severity color
  - No active: Badge hidden or gray
  - INFO: Blue badge
  - WARN: Yellow badge
  - ERROR: Red badge
- Tap: Opens Messages screen

#### Messages Screen
- **Active Tab**: List of unacknowledged messages
  - Sort: Most recent first
  - Display: Severity icon, title, timestamp, count (if > 1)
  - Tap: Open detail view
- **History Tab**: List of acknowledged messages
  - Sort: Most recent first
  - Display: Dimmed, same format
- **Detail View**:
  - Full title + details
  - Source, code, timestamp
  - Count (e.g., "Occurred 5 times, last at 12:34:56")
  - **ACK Button** (active only)
- **Controls**:
  - ACK ALL (active tab)
  - CLEAR HISTORY (history tab)

#### Boot Progress Screen
- Shown during boot sequence
- Displays: Boot stage name, status (pending/ok/warn/fail)
- Auto-transitions to Messages screen if WARN/ERROR present after boot

### Web UI

#### Navigation Badge
- Message icon with badge count
- Color: Highest active severity

#### /messages Route
- Table view with tabs: Active | History
- Columns: Time, Severity, Source, Title, Count
- Actions: ACK, ACK ALL, CLEAR HISTORY
- Auto-refresh via polling (seq-based) or WebSocket

#### API Endpoints
```
GET  /api/messages/summary      → { active_count, highest_severity, sequence }
GET  /api/messages/active       → [ { id, ts, severity, source, code, title, details, count }, ... ]
GET  /api/messages/history      → [ ... ]
POST /api/messages/ack          → { msg_id: <id> }
POST /api/messages/ack_all      → {}
POST /api/messages/clear_history → {}
```

## Health Snapshot

Provides a unified view of system health derived from active messages:

```cpp
HealthSnapshot {
  boot_stage: "BOOT_OK_READY",
  overall_health: "ERROR" | "WARN" | "INFO" | "OK",
  active_error_count: 2,
  active_warn_count: 1,
  active_info_count: 0,
  subsystem_status: {
    "fs": "OK",
    "sd": "WARN",
    "net": "OK",
    ...
  }
}
```

Health derivation:
- **ERROR** if any active ERROR message exists
- **WARN** if any active WARN message exists (and no ERROR)
- **INFO** if any active INFO message exists (and no WARN/ERROR)
- **OK** if no active messages

Both LVGL dashboard and Web dashboard use this snapshot.

## Threading & Safety

- MessageCenter uses internal mutex for thread safety
- Post/ack/clear operations are atomic
- Sequence number increments are atomic
- Safe to call from ISR context via deferred queue (future enhancement)

## Performance Characteristics

- **Post**: O(n) lookup for coalescing (n = active count, typically < 50)
- **Acknowledge**: O(n) lookup + list manipulation
- **Query**: O(1) for counts, O(n) for list iteration
- **Persistence**: Async write (non-blocking) or blocking with timeout

## Memory Footprint

- Each Message: ~200 bytes
- Max active: 100 messages = 20KB
- Max history: 1000 messages = 200KB (in LittleFS, not RAM)
- Total RAM for active messages: ~20KB (acceptable for ESP32-S3 with 8MB PSRAM)

## Future Enhancements

1. **Remote logging**: Forward messages to syslog/MQTT
2. **Message filters**: Per-subsystem enable/disable
3. **Localization**: Multi-language message text
4. **Audio alerts**: Beep on ERROR severity
5. **Email/SMS notifications**: Critical errors trigger external alerts

## References

- Message Origins Matrix: `ORIGINS_MATRIX.md`
- Boot Sequence: `BOOT_SEQUENCE.md`
- Verification Plan: `GAUNTLET.md`
