/*
 * I2C Traffic Monitor - Header
 *
 * Provides counters and indicators for I2C TX/RX activity.
 * Used by UI status bar and web API to show I2C communication.
 *
 * Extracted from original .ino file (lines 125-129).
 */

#pragma once

#include <stdint.h>

// Global I2C traffic counters
extern volatile uint32_t g_i2c_tx_count;
extern volatile uint32_t g_i2c_rx_count;
extern volatile bool g_i2c_tx_blink;
extern volatile bool g_i2c_rx_blink;

// Mark I2C transmission (increments TX counter, sets blink flag)
static inline void i2cMarkTx() {
  g_i2c_tx_count++;
  g_i2c_tx_blink = true;
}

// Mark I2C reception (increments RX counter, sets blink flag)
static inline void i2cMarkRx() {
  g_i2c_rx_count++;
  g_i2c_rx_blink = true;
}

// Reset blink flags (call from UI after visual feedback shown)
static inline void i2cClearBlink() {
  g_i2c_tx_blink = false;
  g_i2c_rx_blink = false;
}
