/*
 * Message Center - Header
 * 
 * PLC-style message/acknowledgment system serving as the single source of truth
 * for all operator-visible status across LVGL and Web UIs.
 * 
 * Design:
 * - All messages require acknowledgment (PLC style)
 * - Messages transition: ACTIVE (unacknowledged) → HISTORY (acknowledged)
 * - Duplicate messages coalesce (increment count + update timestamp)
 * - Thread-safe with mutex protection
 * - Persistent history in LittleFS
 */

#ifndef MESSAGE_CENTER_H
#define MESSAGE_CENTER_H

#include <Arduino.h>
#include <vector>
#include <functional>

// Message severity levels
enum class MessageSeverity : uint8_t {
  INFO = 0,  // Normal operational events requiring acknowledgment
  WARN = 1,  // Degraded operation, system continues
  ERROR = 2  // Fault condition requiring immediate attention
};

// Individual message structure
struct Message {
  uint32_t id;                  // Unique message ID (auto-increment)
  uint32_t timestamp;           // millis() when first posted
  uint32_t last_ts;             // millis() when last updated (for coalescing)
  MessageSeverity severity;     // INFO/WARN/ERROR
  String source;                // Subsystem name (e.g., "boot", "fs", "net")
  String code;                  // Stable message code (e.g., "BOOT_03_FS_OK")
  String title;                 // Short title (max 64 chars)
  String details;               // Longer details (max 256 chars)
  uint16_t count;               // Number of occurrences (coalescing counter)
  bool acknowledged;            // false = active, true = history
  
  Message() : id(0), timestamp(0), last_ts(0), severity(MessageSeverity::INFO),
              count(1), acknowledged(false) {}
};

// Summary information for UI badges and health checks
struct MessageSummary {
  uint16_t active_count;
  uint16_t history_count;
  MessageSeverity highest_active_severity;
  uint32_t sequence;  // Increments on any change (for efficient polling)
  
  MessageSummary() : active_count(0), history_count(0), 
                     highest_active_severity(MessageSeverity::INFO), sequence(0) {}
};

// MessageCenter singleton class
class MessageCenter {
public:
  // Singleton accessor
  static MessageCenter& getInstance();
  
  // Initialize the message center (call early in boot)
  void begin();
  
  // Post a new message (may coalesce with existing active message)
  // Returns: message ID
  uint32_t post(MessageSeverity severity, const char* source, const char* code,
                const char* title, const char* details = "");
  
  // Post with formatted details (printf-style)
  uint32_t postf(MessageSeverity severity, const char* source, const char* code,
                 const char* title, const char* fmt, ...);
  
  // Acknowledge a message (move from active to history)
  bool acknowledge(uint32_t msg_id);
  
  // Acknowledge all active messages
  void acknowledgeAll();
  
  // Clear history (operator action)
  void clearHistory();
  
  // Query methods
  const std::vector<Message>& getActiveMessages() const { return active_messages_; }
  const std::vector<Message>& getHistoryMessages() const { return history_messages_; }
  uint16_t getActiveCount() const { return active_messages_.size(); }
  uint16_t getHistoryCount() const { return history_messages_.size(); }
  MessageSeverity getHighestActiveSeverity() const;
  uint32_t getSequence() const { return sequence_; }
  MessageSummary getSummary() const;
  
  // Find message by ID
  Message* findMessageById(uint32_t msg_id);
  
  // Persistence
  void saveHistory();
  void loadHistory();
  
  // Optional: Save active messages (for re-latching on reboot)
  void saveActive();
  void loadActive();
  
  // Update callback (called when messages change, for UI refresh)
  void setUpdateCallback(std::function<void()> callback) { update_callback_ = callback; }

private:
  MessageCenter();  // Private constructor (singleton)
  ~MessageCenter() = default;
  MessageCenter(const MessageCenter&) = delete;
  MessageCenter& operator=(const MessageCenter&) = delete;
  
  // Internal helpers
  Message* findActiveMessage(const char* source, const char* code, MessageSeverity severity);
  void incrementSequence();
  void notifyUpdate();
  uint32_t nextId();
  
  // Storage
  std::vector<Message> active_messages_;
  std::vector<Message> history_messages_;
  
  // State
  uint32_t sequence_;       // Increments on any change
  uint32_t next_msg_id_;    // Next available message ID
  
  // Thread safety
  SemaphoreHandle_t mutex_;
  
  // Update callback
  std::function<void()> update_callback_;
  
  // Persistence paths
  static const char* HISTORY_FILE;
  static const char* ACTIVE_FILE;
  
  // Limits
  static const uint16_t MAX_ACTIVE = 100;
  static const uint16_t MAX_HISTORY = 1000;
};

// ===================================================================================
// Convenience Macros (to be used throughout the codebase)
// ===================================================================================

// Post INFO message
#define msg_info(source, code, title, ...) \
  MessageCenter::getInstance().postf(MessageSeverity::INFO, source, code, title, ##__VA_ARGS__)

// Post WARN message
#define msg_warn(source, code, title, ...) \
  MessageCenter::getInstance().postf(MessageSeverity::WARN, source, code, title, ##__VA_ARGS__)

// Post ERROR message
#define msg_error(source, code, title, ...) \
  MessageCenter::getInstance().postf(MessageSeverity::ERROR, source, code, title, ##__VA_ARGS__)

#endif // MESSAGE_CENTER_H
