#ifndef SERIALWOMBAT_MANAGER_H
#define SERIALWOMBAT_MANAGER_H

#include <Arduino.h>

#include <ArduinoJson.h>
#include <SerialWombat.h>
#include <WebServer.h>
#include <Wire.h>

// Forward declarations
extern SerialWombat sw;
extern uint8_t currentWombatAddress;

/**
 * Apply a JSON configuration to the SerialWombat device.
 * Configures pins, devices, and modules based on JSON structure.
 */
void applyConfiguration(DynamicJsonDocument& doc);

/**
 * Connect to a SerialWombat device at the specified I2C address.
 */
void handleConnect(WebServer& server);

/**
 * Set pin mode on the SerialWombat device.
 */
void handleSetPin(WebServer& server);

/**
 * Change the I2C address of the SerialWombat device.
 */
void handleChangeAddr(WebServer& server);

/**
 * Reset the SerialWombat device (hardware reset).
 */
void handleResetTarget(WebServer& server);

#endif  // SERIALWOMBAT_MANAGER_H
