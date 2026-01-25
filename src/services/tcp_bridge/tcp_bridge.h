/*
 * TCP Bridge Service - Header
 * 
 * Provides TCP-to-I2C bridge functionality for remote SerialWombat access.
 * Listens on TCP port 3000 and forwards 8-byte packets to/from I2C devices.
 * 
 * Extracted from original .ino file (lines 3812-3847).
 */

#pragma once

#include <WiFiServer.h>
#include <WiFiClient.h>
#include <stdint.h>

// TCP Bridge Configuration
#define TCP_PORT 3000

// Initialize TCP bridge server
void initTcpBridge(WiFiServer& server);

// Handle TCP bridge communication (call in loop)
void handleTcpBridge(WiFiServer& server, WiFiClient& client, uint8_t targetI2CAddress);
