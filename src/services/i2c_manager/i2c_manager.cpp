#include "i2c_manager.h"

// ===================================================================================
// Pin Mode Strings (PROGMEM lookup table)
// ===================================================================================
const char* const pinModeStrings[] PROGMEM = {"DIGITAL_IO",
                                              "CONTROLLED",
                                              "ANALOGINPUT",
                                              "SERVO",
                                              "THROUGHPUT_CONSUMER",
                                              "QUADRATURE_ENC",
                                              "HBRIDGE",
                                              "WATCHDOG",
                                              "PROTECTEDOUTPUT",
                                              "COUNTER",
                                              "DEBOUNCE",
                                              "TM1637",
                                              "WS2812",
                                              "SW_UART",
                                              "INPUT_PROCESSOR",
                                              "MATRIX_KEYPAD",
                                              "PWM",
                                              "UART0_TXRX",
                                              "PULSE_TIMER",
                                              "DMA_PULSE_OUTPUT",
                                              "ANALOG_THROUGHPUT",
                                              "FRAME_TIMER",
                                              "TOUCH",
                                              "UART1_TXRX",
                                              "RESISTANCE_INPUT",
                                              "PULSE_ON_CHANGE",
                                              "HF_SERVO",
                                              "ULTRASONIC_DISTANCE",
                                              "LIQUID_CRYSTAL",
                                              "HS_CLOCK",
                                              "HS_COUNTER",
                                              "VGA",
                                              "PS2_KEYBOARD",
                                              "I2C_CONTROLLER",
                                              "QUEUED_PULSE_OUTPUT",
                                              "MAX7219MATRIX",
                                              "FREQUENCY_OUTPUT",
                                              "IR_RX",
                                              "IR_TX",
                                              "RC_PPM",
                                              "BLINK"};

// ===================================================================================
// I2C Deep Scan - Variant Detection
// ===================================================================================
VariantInfo getDeepScanInfoSingle(uint8_t addr) {
  VariantInfo info;
  info.variant = "Unknown";
  for (int i = 0; i < 41; i++)
    info.caps[i] = false;

  SerialWombat sw_scan;
  sw_scan.begin(Wire, addr, false);
  if (!sw_scan.queryVersion()) return info;

  // Scan supported pin modes using the known-good "wrong order" fingerprint
  for (int pm = 0; pm < 41; ++pm) {
    yield();
    uint8_t tx[8] = {201, 1, (uint8_t)pm, 0x55, 0x55, 0x55, 0x55, 0x55};
    int16_t ret = sw_scan.sendPacket(tx);
    if ((ret * -1) == SW_ERROR_PIN_CONFIG_WRONG_ORDER) info.caps[pm] = true;
  }

  // Variant mapping matches the v06 Deep Scan decisions
  if (info.caps[15]) {
    info.variant = "Keypad Firmware";
  } else if (info.caps[27]) {
    info.variant = "Ultrasonic Firmware";
  } else if (info.caps[17]) {
    info.variant = "Communications Firmware";
  } else if (info.caps[11]) {
    info.variant = "TM1637 Display Firmware";
  } else if (info.caps[25] && info.caps[36] && !info.caps[6]) {
    info.variant = "Front Panel Firmware";
  } else if (info.caps[6] && info.caps[3]) {
    info.variant = "Motor Control / Default";
  } else if (info.caps[6] && !info.caps[3]) {
    info.variant = "Brushed Motor Firmware";
  } else {
    info.variant = "Custom_FW";
  }

  return info;
}

// ===================================================================================
// I2C Handler Functions
// ===================================================================================
void handleScanData(WebServer& server) {
  String found;
  int count = 0;
  for (uint8_t i = 8; i < 127; i++) {
    Wire.beginTransmission(i);
    if ((i2cMarkTx(), Wire.endTransmission()) == 0) {
      found += "Device Found: 0x" + String(i, HEX) + "<br>";
      count++;
    }
  }
  if (count == 0)
    found = "No devices found.";
  else
    found += "<br>Total: " + String(count);
  server.send(200, "text/plain", found);
}

// --- FINGERPRINT MATCHING DEEP SCAN (v06 preserved logic) ---
void handleDeepScan(WebServer& server) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent(F(
      "<!DOCTYPE HTML><html><head><meta name='viewport' content='width=device-width, "
      "initial-scale=1'><style>body{font-family:monospace;background:#222;color:#eee;padding:10px;}"
      ".chip{border:1px solid "
      "#0f0;padding:10px;margin-bottom:10px;background:#333;}h3{color:#00d2ff;margin:0;}b{color:#"
      "0f0;}.btn{display:block;padding:10px;background:#007acc;color:white;text-align:center;text-"
      "decoration:none;margin-top:20px;}</style></head><body><h2>Serial Wombat Deep Scan</h2>"));

  SerialWombat sw_scan;
  for (int i2cAddress = 0x0E; i2cAddress <= 0x77; ++i2cAddress) {
    yield();
    Wire.beginTransmission((uint8_t)i2cAddress);
    if ((i2cMarkTx(), Wire.endTransmission()) == 0) {
      String out = "<div class='chip'><h3>Device @ 0x" + String(i2cAddress, HEX) + "</h3>";

      sw_scan.begin(Wire, (uint8_t)i2cAddress, false);

      if (sw_scan.queryVersion()) {
        bool supported[41] = {0};
        if (sw_scan.isSW18() || sw_scan.isSW08()) {
          for (int pm = 0; pm < 41; ++pm) {
            yield();
            uint8_t tx[8] = {201, 1, (uint8_t)pm, 0x55, 0x55, 0x55, 0x55, 0x55};
            int16_t ret = sw_scan.sendPacket(tx);
            if ((ret * -1) == SW_ERROR_PIN_CONFIG_WRONG_ORDER) supported[pm] = true;
          }
        }

        String variant = "Custom_FW";
        if (supported[15]) {
          variant = "Keypad Firmware";
        } else if (supported[27]) {
          variant = "Ultrasonic Firmware";
        } else if (supported[17]) {
          variant = "Communications Firmware";
        } else if (supported[11]) {
          variant = "TM1637 Display Firmware";
        } else if (supported[25] && supported[36] && !supported[6]) {
          variant = "Front Panel Firmware";
        } else if (supported[6] && supported[3]) {
          variant = "Motor Control / Default";
        } else if (supported[6] && !supported[3]) {
          variant = "Brushed Motor Firmware";
        }

        out += "<b>Serial Wombat Found!</b><br>";
        if (sw_scan.inBoot)
          out += "STATUS: <b style='color:orange'>BOOT MODE</b><br>";
        else
          out += "STATUS: <b>APP MODE</b><br>";

        out += "Model: " + String((char*)sw_scan.model) + "<br>";
        out += "FW Version: " + String((char*)sw_scan.fwVersion) + "<br>";
        out += "<b>Variant: <span style='color:#0ff'>" + variant + "</span></b><br><br>";

        out += "Uptime: " + String(sw_scan.readFramesExecuted()) + " frames<br>";
        out += "Overflows: " + String(sw_scan.readOverflowFrames()) + "<br>";
        out += "Errors: " + String(sw_scan.errorCount) + "<br>";
        out += "Birthday: " + String(sw_scan.readBirthday()) + "<br>";

        char brand[32];
        sw_scan.readBrand(brand);
        out += "Brand: " + String(brand) + "<br>";
        out += "UUID: ";
        for (int i = 0; i < sw_scan.uniqueIdentifierLength; ++i) {
          if (sw_scan.uniqueIdentifier[i] < 16) out += "0";
          out += String(sw_scan.uniqueIdentifier[i], HEX) + " ";
        }
        out += "<br>Voltage: " + String(sw_scan.readSupplyVoltage_mV()) + " mV<br>";
        if (sw_scan.isSW18()) {
          uint16_t t = sw_scan.readTemperature_100thsDegC();
          out += "Temp: " + String(t / 100) + "." + String(t % 100) + " C<br>";
        }

        if (sw_scan.isSW18() || sw_scan.isSW08()) {
          out += "<br><b>Supported Pin Modes:</b><br><span style='font-size:0.8em;color:#aaa;'>";
          for (int pm = 0; pm < 41; ++pm) {
            if (supported[pm]) {
              out += String(FPSTR(pinModeStrings[pm])) + ", ";
            }
          }
          out += "</span>";
        }
      } else {
        out += "Unknown I2C Device";
      }

      out += "</div>";
      server.sendContent(out);
    }
  }

  server.sendContent(F("<a href='/' class='btn'>Return to Dashboard</a></body></html>"));
  server.sendContent("");
}
