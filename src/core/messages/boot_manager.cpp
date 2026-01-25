/*
 * Boot Manager - Implementation
 * 
 * Manages boot sequence stages and integrates with MessageCenter.
 */

#include "boot_manager.h"

// ===================================================================================
// Singleton Implementation
// ===================================================================================

BootManager& BootManager::getInstance() {
  static BootManager instance;
  return instance;
}

BootManager::BootManager() {
  // Pre-populate all boot stages
  for (int i = static_cast<int>(BootStage::BOOT_START); 
       i <= static_cast<int>(BootStage::BOOT_COMPLETE); i++) {
    BootStageRecord record;
    record.stage = static_cast<BootStage>(i);
    record.status = BootStatus::PENDING;
    record.label = getStageName(record.stage);
    summary_.stages.push_back(record);
  }
}

// ===================================================================================
// Initialization
// ===================================================================================

void BootManager::begin() {
  summary_.current_stage = BootStage::NOT_STARTED;
  summary_.boot_complete = false;
  summary_.boot_degraded = false;
  summary_.error_count = 0;
  summary_.warn_count = 0;
  
  // Post initial boot start message
  msg_info("boot", BOOT_START, "System Boot", "Boot sequence initiated");
}

// ===================================================================================
// Boot Stage Management
// ===================================================================================

void BootManager::stageBegin(BootStage stage, const char* label) {
  summary_.current_stage = stage;
  
  BootStageRecord* record = findStage(stage);
  if (record) {
    record->status = BootStatus::IN_PROGRESS;
    record->label = String(label);
    record->start_ts = millis();
  }
  
  // Post begin message
  const char* code = getBeginCode(stage);
  if (code) {
    msg_info("boot", code, label, "Stage starting...");
  }
}

void BootManager::stageOk(BootStage stage, const char* details) {
  updateStageStatus(stage, BootStatus::OK, details);
  
  // Post OK message
  const char* code = getOkCode(stage);
  if (code) {
    BootStageRecord* record = findStage(stage);
    const char* label = record ? record->label.c_str() : "Stage";
    msg_info("boot", code, label, details ? details : "Completed successfully");
  }
}

void BootManager::stageWarn(BootStage stage, const char* details) {
  updateStageStatus(stage, BootStatus::WARN, details);
  summary_.warn_count++;
  summary_.boot_degraded = true;
  
  // Post WARN message
  const char* code = getWarnCode(stage);
  if (code) {
    BootStageRecord* record = findStage(stage);
    const char* label = record ? record->label.c_str() : "Stage";
    msg_warn("boot", code, label, details ? details : "Completed with warnings");
  }
}

void BootManager::stageFail(BootStage stage, const char* details) {
  updateStageStatus(stage, BootStatus::FAIL, details);
  summary_.error_count++;
  summary_.boot_degraded = true;
  
  // Post ERROR message
  const char* code = getFailCode(stage);
  if (code) {
    BootStageRecord* record = findStage(stage);
    const char* label = record ? record->label.c_str() : "Stage";
    msg_error("boot", code, label, details ? details : "Stage failed");
  }
}

void BootManager::bootComplete() {
  summary_.boot_complete = true;
  summary_.current_stage = BootStage::BOOT_COMPLETE;
  
  if (summary_.boot_degraded) {
    msg_warn("boot", BOOT_DEGRADED, "Boot Complete (Degraded)", 
             "System operational with %d errors, %d warnings", 
             summary_.error_count, summary_.warn_count);
  } else {
    msg_info("boot", BOOT_OK_READY, "Boot Complete", 
             "System ready for operation");
  }
}

// ===================================================================================
// Internal Helpers
// ===================================================================================

BootStageRecord* BootManager::findStage(BootStage stage) {
  for (auto& record : summary_.stages) {
    if (record.stage == stage) {
      return &record;
    }
  }
  return nullptr;
}

void BootManager::updateStageStatus(BootStage stage, BootStatus status, const char* details) {
  BootStageRecord* record = findStage(stage);
  if (record) {
    record->status = status;
    record->end_ts = millis();
    if (details) {
      record->details = String(details);
    }
  }
}

// ===================================================================================
// Stage Name Mapping
// ===================================================================================

const char* BootManager::getStageName(BootStage stage) {
  switch (stage) {
    case BootStage::BOOT_START:       return "Boot Start";
    case BootStage::BOOT_01_EARLY:    return "Early Initialization";
    case BootStage::BOOT_02_CONFIG:   return "Configuration Load";
    case BootStage::BOOT_03_FILESYSTEM: return "Filesystem Mount";
    case BootStage::BOOT_04_SD:       return "SD Card";
    case BootStage::BOOT_05_DISPLAY:  return "Display Init";
    case BootStage::BOOT_06_TOUCH:    return "Touch Init";
    case BootStage::BOOT_07_NETWORK:  return "Network Init";
    case BootStage::BOOT_08_TIME:     return "Time Sync";
    case BootStage::BOOT_09_SERVICES: return "Services Start";
    case BootStage::BOOT_10_SELFTEST: return "Self Tests";
    case BootStage::BOOT_COMPLETE:    return "Boot Complete";
    default:                          return "Unknown";
  }
}

// ===================================================================================
// Message Code Mapping
// ===================================================================================

const char* BootManager::getBeginCode(BootStage stage) {
  switch (stage) {
    case BootStage::BOOT_01_EARLY:    return BOOT_01_EARLY_BEGIN;
    case BootStage::BOOT_02_CONFIG:   return BOOT_02_CONFIG_BEGIN;
    case BootStage::BOOT_03_FILESYSTEM: return BOOT_03_FS_BEGIN;
    case BootStage::BOOT_04_SD:       return BOOT_04_SD_BEGIN;
    case BootStage::BOOT_05_DISPLAY:  return BOOT_05_DISPLAY_BEGIN;
    case BootStage::BOOT_06_TOUCH:    return BOOT_06_TOUCH_BEGIN;
    case BootStage::BOOT_07_NETWORK:  return BOOT_07_NET_BEGIN;
    case BootStage::BOOT_08_TIME:     return BOOT_08_TIME_BEGIN;
    case BootStage::BOOT_09_SERVICES: return BOOT_09_SERVICES_BEGIN;
    case BootStage::BOOT_10_SELFTEST: return BOOT_10_SELFTEST_BEGIN;
    default:                          return nullptr;
  }
}

const char* BootManager::getOkCode(BootStage stage) {
  switch (stage) {
    case BootStage::BOOT_01_EARLY:    return BOOT_01_EARLY_OK;
    case BootStage::BOOT_02_CONFIG:   return BOOT_02_CONFIG_OK;
    case BootStage::BOOT_03_FILESYSTEM: return BOOT_03_FS_OK;
    case BootStage::BOOT_04_SD:       return BOOT_04_SD_OK;
    case BootStage::BOOT_05_DISPLAY:  return BOOT_05_DISPLAY_OK;
    case BootStage::BOOT_06_TOUCH:    return BOOT_06_TOUCH_OK;
    case BootStage::BOOT_07_NETWORK:  return BOOT_07_NET_OK;
    case BootStage::BOOT_08_TIME:     return BOOT_08_TIME_OK;
    case BootStage::BOOT_09_SERVICES: return BOOT_09_SERVICES_OK;
    case BootStage::BOOT_10_SELFTEST: return BOOT_10_SELFTEST_OK;
    default:                          return nullptr;
  }
}

const char* BootManager::getWarnCode(BootStage stage) {
  switch (stage) {
    case BootStage::BOOT_02_CONFIG:   return BOOT_02_CONFIG_WARN;
    case BootStage::BOOT_04_SD:       return BOOT_04_SD_NOT_PRESENT;
    case BootStage::BOOT_05_DISPLAY:  return BOOT_05_DISPLAY_DISABLED;
    case BootStage::BOOT_06_TOUCH:    return BOOT_06_TOUCH_FAIL;
    case BootStage::BOOT_07_NETWORK:  return BOOT_07_NET_AP_FALLBACK;
    case BootStage::BOOT_08_TIME:     return BOOT_08_TIME_FAIL;
    case BootStage::BOOT_09_SERVICES: return BOOT_09_OTA_FAIL;
    case BootStage::BOOT_10_SELFTEST: return BOOT_10_SELFTEST_FAIL;
    default:                          return nullptr;
  }
}

const char* BootManager::getFailCode(BootStage stage) {
  switch (stage) {
    case BootStage::BOOT_02_CONFIG:   return BOOT_02_CONFIG_FAIL;
    case BootStage::BOOT_03_FILESYSTEM: return BOOT_03_FS_FAIL;
    case BootStage::BOOT_04_SD:       return BOOT_04_SD_FAIL;
    case BootStage::BOOT_05_DISPLAY:  return BOOT_05_DISPLAY_FAIL;
    case BootStage::BOOT_07_NETWORK:  return BOOT_07_NET_FAIL;
    case BootStage::BOOT_09_SERVICES: return BOOT_09_WEB_FAIL;
    default:                          return nullptr;
  }
}
