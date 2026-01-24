/*
 * Health Snapshot
 * 
 * Derives overall system health from active messages in MessageCenter.
 * Provides a unified health status for dashboards and monitoring.
 * 
 * Health derivation rules:
 * - ERROR present => overall health = ERROR
 * - WARN present (no ERROR) => overall health = WARN  
 * - Only INFO or no messages => overall health = OK
 */

#ifndef HEALTH_SNAPSHOT_H
#define HEALTH_SNAPSHOT_H

#include <Arduino.h>
#include "message_center.h"

enum class SystemHealth {
  OK = 0,
  WARN = 1,
  ERROR = 2,
  UNKNOWN = 3
};

struct HealthSnapshot {
  SystemHealth overall;
  int active_count;
  int error_count;
  int warn_count;
  int info_count;
  uint32_t last_update_ms;
  
  // Boot stage status
  bool boot_complete;
  bool boot_degraded;
  
  // Subsystem status (derived from messages)
  bool filesystem_ok;
  bool sd_present;
  bool display_ok;
  bool network_ok;
  bool services_ok;
};

class HealthSnapshotManager {
public:
  static HealthSnapshotManager& getInstance();
  
  // Update the snapshot by querying MessageCenter
  void update();
  
  // Get current snapshot
  const HealthSnapshot& getSnapshot() const { return snapshot; }
  
  // Get overall health as string
  const char* getHealthString() const;
  
  // Get overall health color (for UI)
  uint32_t getHealthColor() const;
  
private:
  HealthSnapshotManager() = default;
  HealthSnapshot snapshot;
  
  void deriveHealthFromMessages();
  void deriveSubsystemStatus();
};

// Convenience function
inline HealthSnapshotManager& getHealth() {
  return HealthSnapshotManager::getInstance();
}

#endif // HEALTH_SNAPSHOT_H
