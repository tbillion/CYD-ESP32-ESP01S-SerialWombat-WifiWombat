/*
 * TCP Bridge Service - Implementation
 *
 * TCP-to-I2C bridge for remote SerialWombat device access.
 *
 * Protocol:
 * - Client connects to TCP port 3000
 * - Sends 8-byte command packets
 * - Receives 8-byte response packets
 * - Commands are forwarded to I2C device
 * - Single client at a time (additional connections rejected)
 *
 * Extracted from original .ino file (lines 3812-3847).
 */

#include "tcp_bridge.h"

#include <Arduino.h>

#include <ArduinoOTA.h>
#include <Wire.h>

#include "../../core/i2c_monitor.h"

void initTcpBridge(WiFiServer& server) {
  server.begin();
}

void handleTcpBridge(WiFiServer& server, WiFiClient& client, uint8_t targetI2CAddress) {
  // Check for new client connection
  if (server.hasClient()) {
    if (!client || !client.connected()) {
      // Accept new client
      client = server.available();
      client.setNoDelay(true);
    } else {
      // Already have a client, reject new connection
      WiFiClient reject = server.available();
      reject.stop();
    }
  }

  // Handle active client communication
  if (client && client.connected()) {
    // Process available data in 8-byte packets
    while (client.available() >= 8) {
      uint8_t txBuffer[8];
      uint8_t rxBuffer[8];

      // Read 8-byte command from TCP client
      client.read(txBuffer, 8);

      // Forward to I2C device
      Wire.beginTransmission(targetI2CAddress);
      Wire.write(txBuffer, 8);
      Wire.endTransmission();
      i2cMarkTx();  // Update I2C traffic indicator

      // Read 8-byte response from I2C device
      uint8_t bytesRead = Wire.requestFrom(targetI2CAddress, (uint8_t)8);
      i2cMarkRx();  // Update I2C traffic indicator

      for (int i = 0; i < 8; i++) {
        if (i < bytesRead) {
          rxBuffer[i] = Wire.read();
        } else {
          rxBuffer[i] = 0xFF;  // Pad with 0xFF if less than 8 bytes received
        }
      }

      // Send response back to TCP client
      client.write(rxBuffer, 8);

      // Keep system responsive during bridge operations
      ArduinoOTA.handle();
      yield();
    }
  }
}
