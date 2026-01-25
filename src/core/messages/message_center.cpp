/*
 * Message Center - Implementation
 * 
 * PLC-style message/acknowledgment system core logic.
 */

#include "message_center.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// Persistence file paths
const char* MessageCenter::HISTORY_FILE = "/messages/history.json";
const char* MessageCenter::ACTIVE_FILE = "/messages/active.json";

// ===================================================================================
// Singleton Implementation
// ===================================================================================

MessageCenter& MessageCenter::getInstance() {
  static MessageCenter instance;
  return instance;
}

MessageCenter::MessageCenter() 
  : sequence_(0), next_msg_id_(1), update_callback_(nullptr) {
  mutex_ = xSemaphoreCreateMutex();
}

// ===================================================================================
// Initialization
// ===================================================================================

void MessageCenter::begin() {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  
  // Ensure messages directory exists
  if (!LittleFS.exists("/messages")) {
    LittleFS.mkdir("/messages");
  }
  
  // Load persisted history
  loadHistory();
  
  // Optionally load active messages (for re-latching on reboot)
  // loadActive();
  
  xSemaphoreGive(mutex_);
}

// ===================================================================================
// Message Posting
// ===================================================================================

uint32_t MessageCenter::post(MessageSeverity severity, const char* source, const char* code,
                              const char* title, const char* details) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  
  uint32_t msg_id = 0;
  
  // Check for existing active message with same {severity, source, code}
  Message* existing = findActiveMessage(source, code, severity);
  
  if (existing) {
    // Coalesce: increment count, update timestamp and details
    existing->count++;
    existing->last_ts = millis();
    if (details && strlen(details) > 0) {
      existing->details = String(details);
    }
    msg_id = existing->id;
    incrementSequence();
  } else {
    // Create new message
    Message msg;
    msg.id = nextId();
    msg.timestamp = millis();
    msg.last_ts = msg.timestamp;
    msg.severity = severity;
    msg.source = String(source);
    msg.code = String(code);
    msg.title = String(title);
    msg.details = String(details ? details : "");
    msg.count = 1;
    msg.acknowledged = false;
    
    active_messages_.push_back(msg);
    msg_id = msg.id;
    incrementSequence();
    
    // Log to serial for debugging
    const char* sev_str = (severity == MessageSeverity::INFO) ? "INFO" :
                          (severity == MessageSeverity::WARN) ? "WARN" : "ERROR";
    Serial.printf("[%s] %s: %s - %s\n", sev_str, source, title, details ? details : "");
  }
  
  xSemaphoreGive(mutex_);
  
  notifyUpdate();
  return msg_id;
}

uint32_t MessageCenter::postf(MessageSeverity severity, const char* source, const char* code,
                               const char* title, const char* fmt, ...) {
  char details[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(details, sizeof(details), fmt, args);
  va_end(args);
  
  return post(severity, source, code, title, details);
}

// ===================================================================================
// Acknowledgment
// ===================================================================================

bool MessageCenter::acknowledge(uint32_t msg_id) {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  
  bool found = false;
  
  // Find message in active list
  for (auto it = active_messages_.begin(); it != active_messages_.end(); ++it) {
    if (it->id == msg_id) {
      // Move to history
      it->acknowledged = true;
      history_messages_.push_back(*it);
      active_messages_.erase(it);
      
      incrementSequence();
      found = true;
      break;
    }
  }
  
  xSemaphoreGive(mutex_);
  
  if (found) {
    saveHistory();  // Persist history
    notifyUpdate();
  }
  
  return found;
}

void MessageCenter::acknowledgeAll() {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  
  // Move all active messages to history
  for (auto& msg : active_messages_) {
    msg.acknowledged = true;
    history_messages_.push_back(msg);
  }
  
  active_messages_.clear();
  incrementSequence();
  
  xSemaphoreGive(mutex_);
  
  saveHistory();
  notifyUpdate();
}

void MessageCenter::clearHistory() {
  xSemaphoreTake(mutex_, portMAX_DELAY);
  
  history_messages_.clear();
  incrementSequence();
  
  xSemaphoreGive(mutex_);
  
  // Delete history file
  if (LittleFS.exists(HISTORY_FILE)) {
    LittleFS.remove(HISTORY_FILE);
  }
  
  notifyUpdate();
}

// ===================================================================================
// Query Methods
// ===================================================================================

MessageSeverity MessageCenter::getHighestActiveSeverity() const {
  MessageSeverity highest = MessageSeverity::INFO;
  
  for (const auto& msg : active_messages_) {
    if (msg.severity > highest) {
      highest = msg.severity;
    }
  }
  
  return highest;
}

MessageSummary MessageCenter::getSummary() const {
  MessageSummary summary;
  summary.active_count = active_messages_.size();
  summary.history_count = history_messages_.size();
  summary.highest_active_severity = getHighestActiveSeverity();
  summary.sequence = sequence_;
  return summary;
}

Message* MessageCenter::findMessageById(uint32_t msg_id) {
  for (auto& msg : active_messages_) {
    if (msg.id == msg_id) {
      return &msg;
    }
  }
  for (auto& msg : history_messages_) {
    if (msg.id == msg_id) {
      return &msg;
    }
  }
  return nullptr;
}

// ===================================================================================
// Internal Helpers
// ===================================================================================

Message* MessageCenter::findActiveMessage(const char* source, const char* code, MessageSeverity severity) {
  for (auto& msg : active_messages_) {
    if (msg.severity == severity && 
        msg.source.equals(source) && 
        msg.code.equals(code)) {
      return &msg;
    }
  }
  return nullptr;
}

void MessageCenter::incrementSequence() {
  sequence_++;
}

void MessageCenter::notifyUpdate() {
  if (update_callback_) {
    update_callback_();
  }
}

uint32_t MessageCenter::nextId() {
  return next_msg_id_++;
}

// ===================================================================================
// Persistence
// ===================================================================================

void MessageCenter::saveHistory() {
  // Limit history size
  while (history_messages_.size() > MAX_HISTORY) {
    history_messages_.erase(history_messages_.begin());
  }
  
  // Create JSON document
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (const auto& msg : history_messages_) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = msg.id;
    obj["ts"] = msg.timestamp;
    obj["last_ts"] = msg.last_ts;
    obj["sev"] = static_cast<uint8_t>(msg.severity);
    obj["src"] = msg.source;
    obj["code"] = msg.code;
    obj["title"] = msg.title;
    obj["details"] = msg.details;
    obj["count"] = msg.count;
  }
  
  // Write to file
  File file = LittleFS.open(HISTORY_FILE, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

void MessageCenter::loadHistory() {
  if (!LittleFS.exists(HISTORY_FILE)) {
    return;  // No history file, start fresh
  }
  
  File file = LittleFS.open(HISTORY_FILE, "r");
  if (!file) {
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("MessageCenter: Failed to parse history file");
    return;
  }
  
  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    Message msg;
    msg.id = obj["id"] | 0;
    msg.timestamp = obj["ts"] | 0;
    msg.last_ts = obj["last_ts"] | msg.timestamp;
    msg.severity = static_cast<MessageSeverity>(obj["sev"] | 0);
    msg.source = obj["src"] | "";
    msg.code = obj["code"] | "";
    msg.title = obj["title"] | "";
    msg.details = obj["details"] | "";
    msg.count = obj["count"] | 1;
    msg.acknowledged = true;
    
    history_messages_.push_back(msg);
    
    // Update next_msg_id to avoid collisions
    if (msg.id >= next_msg_id_) {
      next_msg_id_ = msg.id + 1;
    }
  }
}

void MessageCenter::saveActive() {
  // Similar to saveHistory but for active messages
  JsonDocument doc;
  JsonArray arr = doc.to<JsonArray>();
  
  for (const auto& msg : active_messages_) {
    JsonObject obj = arr.add<JsonObject>();
    obj["id"] = msg.id;
    obj["ts"] = msg.timestamp;
    obj["last_ts"] = msg.last_ts;
    obj["sev"] = static_cast<uint8_t>(msg.severity);
    obj["src"] = msg.source;
    obj["code"] = msg.code;
    obj["title"] = msg.title;
    obj["details"] = msg.details;
    obj["count"] = msg.count;
  }
  
  File file = LittleFS.open(ACTIVE_FILE, "w");
  if (file) {
    serializeJson(doc, file);
    file.close();
  }
}

void MessageCenter::loadActive() {
  if (!LittleFS.exists(ACTIVE_FILE)) {
    return;
  }
  
  File file = LittleFS.open(ACTIVE_FILE, "r");
  if (!file) {
    return;
  }
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, file);
  file.close();
  
  if (error) {
    Serial.println("MessageCenter: Failed to parse active file");
    return;
  }
  
  JsonArray arr = doc.as<JsonArray>();
  for (JsonObject obj : arr) {
    Message msg;
    msg.id = obj["id"] | 0;
    msg.timestamp = obj["ts"] | 0;
    msg.last_ts = obj["last_ts"] | msg.timestamp;
    msg.severity = static_cast<MessageSeverity>(obj["sev"] | 0);
    msg.source = obj["src"] | "";
    msg.code = obj["code"] | "";
    msg.title = obj["title"] | "";
    msg.details = obj["details"] | "";
    msg.count = obj["count"] | 1;
    msg.acknowledged = false;
    
    active_messages_.push_back(msg);
    
    if (msg.id >= next_msg_id_) {
      next_msg_id_ = msg.id + 1;
    }
  }
}
