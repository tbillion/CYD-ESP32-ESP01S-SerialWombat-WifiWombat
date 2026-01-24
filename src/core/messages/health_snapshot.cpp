/*
 * Health Snapshot - Implementation
 */

#include "health_snapshot.h"
#include "boot_manager.h"

HealthSnapshotManager& HealthSnapshotManager::getInstance() {
  static HealthSnapshotManager instance;
  return instance;
}

void HealthSnapshotManager::update() {
  deriveHealthFromMessages();
  deriveSubsystemStatus();
  snapshot.last_update_ms = millis();
}

void HealthSnapshotManager::deriveHealthFromMessages() {
  auto& msgCenter = MessageCenter::getInstance();
  auto summary = msgCenter.getSummary();
  
  snapshot.active_count = summary.active_count;
  
  // Count messages by severity
  snapshot.error_count = 0;
  snapshot.warn_count = 0;
  snapshot.info_count = 0;
  
  auto activeMessages = msgCenter.getActiveMessages();
  for (const auto& msg : activeMessages) {
    switch (msg.severity) {
      case MessageSeverity::ERROR:
        snapshot.error_count++;
        break;
      case MessageSeverity::WARN:
        snapshot.warn_count++;
        break;
      case MessageSeverity::INFO:
        snapshot.info_count++;
        break;
    }
  }
  
  // Derive overall health using PLC logic
  if (snapshot.error_count > 0) {
    snapshot.overall = SystemHealth::ERROR;
  } else if (snapshot.warn_count > 0) {
    snapshot.overall = SystemHealth::WARN;
  } else if (snapshot.active_count > 0) {
    snapshot.overall = SystemHealth::OK;  // Only INFO messages
  } else {
    snapshot.overall = SystemHealth::OK;  // No messages
  }
}

void HealthSnapshotManager::deriveSubsystemStatus() {
  auto& bootMgr = BootManager::getInstance();
  auto bootSummary = bootMgr.getSummary();
  
  snapshot.boot_complete = bootSummary.complete;
  snapshot.boot_degraded = bootSummary.degraded;
  
  // Derive subsystem status from boot stages
  auto stage_fs = bootMgr.getStageStatus(BootStage::BOOT_03_FILESYSTEM);
  snapshot.filesystem_ok = (stage_fs == BootStageStatus::OK);
  
  auto stage_sd = bootMgr.getStageStatus(BootStage::BOOT_04_SD);
  snapshot.sd_present = (stage_sd == BootStageStatus::OK);
  
  auto stage_disp = bootMgr.getStageStatus(BootStage::BOOT_05_DISPLAY);
  snapshot.display_ok = (stage_disp == BootStageStatus::OK);
  
  auto stage_net = bootMgr.getStageStatus(BootStage::BOOT_07_NETWORK);
  snapshot.network_ok = (stage_net == BootStageStatus::OK);
  
  auto stage_svc = bootMgr.getStageStatus(BootStage::BOOT_09_SERVICES);
  snapshot.services_ok = (stage_svc == BootStageStatus::OK);
}

const char* HealthSnapshotManager::getHealthString() const {
  switch (snapshot.overall) {
    case SystemHealth::OK:
      return "OK";
    case SystemHealth::WARN:
      return "WARNING";
    case SystemHealth::ERROR:
      return "ERROR";
    case SystemHealth::UNKNOWN:
    default:
      return "UNKNOWN";
  }
}

uint32_t HealthSnapshotManager::getHealthColor() const {
  switch (snapshot.overall) {
    case SystemHealth::OK:
      return 0x28a745;  // Green
    case SystemHealth::WARN:
      return 0xffc107;  // Orange
    case SystemHealth::ERROR:
      return 0xdc3545;  // Red
    case SystemHealth::UNKNOWN:
    default:
      return 0x6c757d;  // Gray
  }
}
