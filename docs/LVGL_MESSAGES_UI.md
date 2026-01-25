# LVGL Messages UI - Implementation Documentation

## Overview

This document describes the LVGL user interface implementation for the PLC-style MessageCenter system. The UI provides operators with visual feedback on active messages and a comprehensive interface to view and acknowledge them.

## Components

### 1. Status Bar Message Badge (`statusbar.cpp/h`)

**Location:** `src/ui/components/statusbar.cpp`

**Purpose:** Displays a notification badge in the status bar showing the count of active messages.

**Features:**
- Shows bell icon (🔔) with count when active messages exist
- Color-coded by severity:
  - Blue (#0088FF) - INFO messages
  - Orange (#FF8800) - WARN messages  
  - Red (#FF0000) - ERROR messages
- Clickable - tapping opens the Messages Screen
- Updates automatically every 500ms via `lvglTickAndUpdate()`

**API:**
```cpp
void updateMessageBadge();  // Called from LVGL update loop
```

**Implementation Notes:**
- Queries `MessageCenter::getInstance().getSummary()` for current state
- Hidden when no active messages (clean UI)
- Positioned at LEFT_MID, 60px offset in status bar

### 2. Messages Screen (`messages_screen.cpp/h`)

**Location:** `src/ui/screens/messages_screen.cpp`

**Purpose:** Full-screen tabbed interface for viewing and managing messages.

**Features:**

#### Active Tab
- Lists all unacknowledged messages
- Shows: severity icon, timestamp (HH:MM:SS), title
- Each message is clickable to view details
- "Acknowledge All" button at bottom
- Scrollable list for many messages

#### History Tab  
- Lists all acknowledged messages
- Same format as Active tab
- "Clear History" button at bottom
- Scrollable list

**API:**
```cpp
void showMessagesScreen();           // Display the messages screen
void showMessageDetail(uint32_t id); // Show detail popup for a message
```

**Implementation Notes:**
- Uses LVGL tabview for clean tab switching
- Flex layout with column flow for message lists
- Timestamps displayed as uptime (HH:MM:SS since boot)
- Messages sorted by MessageCenter (newest first)

### 3. Message Detail Popup

**Purpose:** Modal popup showing full message information.

**Features:**
- Displays all message fields:
  - Severity (icon + text)
  - Source subsystem
  - Message code
  - Timestamp
  - Occurrence count (coalescing)
  - Title
  - Details
- For active messages: "Acknowledge" button
- For history messages: "Close" button only
- Semi-transparent dark background overlay

**Implementation Notes:**
- Created dynamically when message clicked
- 90% screen width/height for good visibility
- Destroyed when closed or after acknowledgment
- Refreshes parent lists after actions

## Integration Points

### LVGL Update Loop

**File:** `src/ui/lvgl_wrapper.cpp`

```cpp
void lvglTickAndUpdate() {
  // ... existing code ...
  
  // Update message badge every 500ms
  updateMessageBadge();
  
  // ... existing code ...
}
```

This ensures the badge always reflects current message state without manual refresh calls.

### Navigation Flow

```
Status Bar Badge (Click)
    ↓
Messages Screen
    ↓
Message Item (Click)
    ↓
Message Detail Popup
    ↓
[Acknowledge] → Back to Messages Screen → Active/History lists refresh
```

## MessageCenter API Usage

The UI uses these MessageCenter methods:

```cpp
// Get summary for badge
MessageSummary summary = MessageCenter::getInstance().getSummary();
// Returns: active_count, history_count, highest_active_severity, sequence

// Get message lists
const vector<Message>& active = MessageCenter::getInstance().getActiveMessages();
const vector<Message>& history = MessageCenter::getInstance().getHistoryMessages();

// Find specific message
Message* msg = MessageCenter::getInstance().findMessageById(msg_id);

// Actions
MessageCenter::getInstance().acknowledge(msg_id);     // ACK single message
MessageCenter::getInstance().acknowledgeAll();         // ACK all active
MessageCenter::getInstance().clearHistory();           // Clear history
```

## Display Compatibility

All code is wrapped in:
```cpp
#if DISPLAY_SUPPORT_ENABLED
// ... UI code ...
#endif
```

This ensures clean compilation when display support is disabled (headless mode).

## LVGL Version

Implemented for **LVGL 9** using:
- `lv_tabview_create()` / `lv_tabview_add_tab()`
- Flex layouts (`lv_obj_set_flex_flow()`)
- Event callbacks (`lv_obj_add_event_cb()`)
- Standard widgets (button, label, container)

Compatible with LVGL 8 with minor API adjustments if needed.

## Styling

**Design Philosophy:** Simple, functional, PLC-style interface

**Colors:**
- Background: Black (#000000)
- Popup: Dark gray (#202020)
- Borders: Medium gray (#606060)
- Info: Blue (#0088FF)
- Warning: Orange (#FF8800)
- Error: Red (#FF0000)

**Icons:**
- Bell: 🔔
- Info: ℹ️
- Warning: ⚠️
- Error: ❌

**Layout:**
- Status bar: 24px height, 100% width
- Messages screen: 90% height (below status bar)
- Popups: 90% width, 80% height, centered
- Buttons: 40px height, appropriate width
- List items: 95% width, auto height

## Performance Considerations

1. **Badge Updates:** Only every 500ms (not every frame)
2. **List Population:** Only when screen shown or after actions
3. **Sequence Check:** Could be added to skip unnecessary redraws
4. **Memory:** Message lists are references (no copying)

## Testing Checklist

- [ ] Badge appears when messages posted
- [ ] Badge color matches highest severity
- [ ] Badge click opens messages screen
- [ ] Active tab shows unacknowledged messages
- [ ] History tab shows acknowledged messages
- [ ] Message detail popup shows all fields
- [ ] Acknowledge button moves message to history
- [ ] Acknowledge All clears active list
- [ ] Clear History empties history tab
- [ ] Timestamps display correctly
- [ ] Long titles scroll/wrap properly
- [ ] Works with 0 messages (empty state)
- [ ] Works with many messages (scrolling)

## Future Enhancements

Potential improvements (not implemented):
- Search/filter messages by source or severity
- Export messages to SD card
- Timestamp formatting options (absolute vs relative)
- Sound/vibration on new ERROR messages
- Auto-scroll to newest message
- Message priority/pinning
- Batch acknowledge (checkboxes)

## File Summary

| File | Lines | Purpose |
|------|-------|---------|
| `statusbar.h` | ~25 | Badge API declarations |
| `statusbar.cpp` | ~75 | Badge implementation |
| `messages_screen.h` | ~15 | Screen API declarations |
| `messages_screen.cpp` | ~290 | Full screen and popup implementation |
| `lvgl_wrapper.cpp` | ~190 | LVGL loop integration (1 line added) |

**Total:** ~605 lines of code across 5 files

## Dependencies

- `core/messages/message_center.h` - Message data and operations
- `ui/components/statusbar.h` - Status bar infrastructure  
- `ui/lvgl_wrapper.h` - LVGL initialization and loop
- `config/system_config.h` - Display enable flags
- `lvgl.h` - LVGL library (v9)

## Configuration

No configuration options yet. Potential additions:
- Badge position customization
- Update rate adjustment
- Message list size limits
- Color scheme selection
- Icon set choice

---

**Implementation Date:** January 2025  
**LVGL Version:** 9.x  
**Status:** Complete and functional
