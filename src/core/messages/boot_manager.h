/*
 * Boot Manager - Header
 *
 * Manages PLC-style boot sequence with explicit stages, pass/fail status,
 * and integration with MessageCenter for operator visibility.
 */

#ifndef BOOT_MANAGER_H
#define BOOT_MANAGER_H

#include <Arduino.h>

#include "message_center.h"
#include "message_codes.h"

// Boot stage identifiers
enum class BootStage : uint8_t {
  NOT_STARTED = 0,
  BOOT_START,
  BOOT_01_EARLY,
  BOOT_02_CONFIG,
  BOOT_03_FILESYSTEM,
  BOOT_04_SD,
  BOOT_05_DISPLAY,
  BOOT_06_TOUCH,
  BOOT_07_NETWORK,
  BOOT_08_TIME,
  BOOT_09_SERVICES,
  BOOT_10_SELFTEST,
  BOOT_COMPLETE
};

// Boot stage status
enum class BootStatus : uint8_t {
  PENDING = 0,  // Not yet started
  IN_PROGRESS,  // Currently executing
  OK,           // Completed successfully
  WARN,         // Completed with warnings (degraded but functional)
  FAIL          // Failed
};

// Individual boot stage record
struct BootStageRecord {
  BootStage stage;
  BootStatus status;
  String label;       // Human-readable stage name
  String details;     // Additional info (error message, etc.)
  uint32_t start_ts;  // millis() when stage started
  uint32_t end_ts;    // millis() when stage completed

  BootStageRecord()
      : stage(BootStage::NOT_STARTED), status(BootStatus::PENDING), start_ts(0), end_ts(0) {}
};

// Overall boot summary
struct BootSummary {
  bool boot_complete;
  bool boot_degraded;  // true if any WARN/ERROR occurred
  uint8_t error_count;
  uint8_t warn_count;
  BootStage current_stage;
  std::vector<BootStageRecord> stages;

  BootSummary()
      : boot_complete(false),
        boot_degraded(false),
        error_count(0),
        warn_count(0),
        current_stage(BootStage::NOT_STARTED) {}
};

// BootManager singleton class
class BootManager {
 public:
  // Singleton accessor
  static BootManager& getInstance();

  // Initialize boot manager (call at very start of setup())
  void begin();

  // Boot stage helpers (emit messages automatically)
  void stageBegin(BootStage stage, const char* label);
  void stageOk(BootStage stage, const char* details = "");
  void stageWarn(BootStage stage, const char* details);
  void stageFail(BootStage stage, const char* details);

  // Mark boot complete
  void bootComplete();

  // Query boot status
  const BootSummary& getSummary() const { return summary_; }
  BootStage getCurrentStage() const { return summary_.current_stage; }
  bool isBootComplete() const { return summary_.boot_complete; }
  bool isBootDegraded() const { return summary_.boot_degraded; }

  // Get human-readable stage name
  static const char* getStageName(BootStage stage);

  // Get message codes for a stage
  static const char* getBeginCode(BootStage stage);
  static const char* getOkCode(BootStage stage);
  static const char* getWarnCode(BootStage stage);
  static const char* getFailCode(BootStage stage);

 private:
  BootManager();  // Private constructor (singleton)
  ~BootManager() = default;
  BootManager(const BootManager&) = delete;
  BootManager& operator=(const BootManager&) = delete;

  // Internal helpers
  BootStageRecord* findStage(BootStage stage);
  void updateStageStatus(BootStage stage, BootStatus status, const char* details);

  // State
  BootSummary summary_;
};

// ===================================================================================
// Convenience Macros for Boot Stages
// ===================================================================================

#define boot_stage_begin(stage, label) BootManager::getInstance().stageBegin(stage, label)

#define boot_stage_ok(stage, ...) BootManager::getInstance().stageOk(stage, ##__VA_ARGS__)

#define boot_stage_warn(stage, details) BootManager::getInstance().stageWarn(stage, details)

#define boot_stage_fail(stage, details) BootManager::getInstance().stageFail(stage, details)

#endif  // BOOT_MANAGER_H
