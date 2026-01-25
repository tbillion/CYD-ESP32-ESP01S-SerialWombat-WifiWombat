/*
 * I2C Traffic Monitor - Implementation
 * 
 * Global variables for I2C traffic monitoring.
 * 
 * Extracted from original .ino file (lines 125-129).
 */

#include "i2c_monitor.h"

// I2C traffic counters - always available (used by both Web UI and LVGL)
volatile uint32_t g_i2c_tx_count = 0;
volatile uint32_t g_i2c_rx_count = 0;
volatile bool g_i2c_tx_blink = false;
volatile bool g_i2c_rx_blink = false;
