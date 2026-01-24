#include "serialwombat_manager.h"
#include "../security/validators.h"
#include "../web_server/security.h"
#include "../i2c_manager/i2c_manager.h"

// ===================================================================================
// Global SerialWombat State
// ===================================================================================
SerialWombat sw;
uint8_t currentWombatAddress = 0x6C;  // Default I2C address

// ===================================================================================
// CONFIGURATOR APPLY LOGIC (JSON -> Wombat)
// ===================================================================================
void applyConfiguration(DynamicJsonDocument& doc) {
  // Safety: reset and re-begin before applying.
  sw.begin(Wire, currentWombatAddress, false);
  sw.hardwareReset();
  delay(600);
  sw.begin(Wire, currentWombatAddress, false);

  JsonArray devices = doc["device_mode"].as<JsonArray>();
  for (JsonObject dev : devices) {
    String type = dev["type"] | "";
    JsonObject pins = dev["pins"];
    JsonObject settings = dev["settings"];

    if (type == "MOTOR_SIMPLE_HBRIDGE") {
      SerialWombatHBridge b(sw);
      b.begin(pins["pwm"] | 0, pins["dir"] | 1);
    } else if (type == "SERVO") {
      SerialWombatServo s(sw);
      s.attach(pins["pin"] | 0, settings["min"] | 544, settings["max"] | 2400);
      s.write(settings["initial"] | 1500);
    } else if (type == "QUAD_ENC") {
      SerialWombatQuadEnc q(sw);
      q.begin(pins["A"] | 0, pins["B"] | 1, settings["debounce"] | 2);
    } else if (type == "ULTRASONIC") {
      SerialWombatUltrasonicDistanceSensor u(sw);
      u.begin(pins["echo"] | 1, SerialWombatUltrasonicDistanceSensor::driver::HC_SR04, pins["trig"] | 0, true, false);
    } else if (type == "TM1637") {
      SerialWombatTM1637 t(sw);
      t.begin(pins["clk"] | 0, pins["dio"] | 1, settings["digits"] | 4, (SWTM1637Mode)2, 0, settings["bright"] | 7);
      t.writeBrightness(settings["bright"] | 7);
    } else if (type == "PWM_DIMMER") {
      SerialWombatPWM p(sw);
      p.begin(pins["pin"] | 0);
      if (settings.containsKey("duty")) p.writeDutyCycle((uint16_t)settings["duty"]);
    }
  }

  JsonObject pinMap = doc["pin_mode"].as<JsonObject>();
  for (JsonPair kv : pinMap) {
    int pin = atoi(kv.key().c_str());
    JsonObject conf = kv.value().as<JsonObject>();
    String mode = conf["mode"] | "DIGITAL_IN";

    if (mode == "DIGITAL_IN") {
      sw.pinMode(pin, INPUT);
    } else if (mode == "INPUT_PULLUP") {
      sw.pinMode(pin, INPUT_PULLUP);
    } else if (mode == "DIGITAL_OUT") {
      sw.pinMode(pin, OUTPUT);
      sw.digitalWrite(pin, conf["initial"] | 0);
    } else if (mode == "SERVO") {
      SerialWombatServo s(sw);
      s.attach(pin);
      if (conf.containsKey("pos")) s.write((uint16_t)conf["pos"]);
    } else if (mode == "PWM") {
      SerialWombatPWM p(sw);
      p.begin(pin);
      if (conf.containsKey("duty")) p.writeDutyCycle((uint16_t)conf["duty"]);
    } else if (mode == "ANALOG_IN") {
      SerialWombatAnalogInput a(sw);
      a.begin(pin);
    }
  }
}

// ===================================================================================
// SERIALWOMBAT HANDLERS
// ===================================================================================
void handleConnect(WebServer& server) {
  // Authentication required for I2C operations
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);
  
  if (server.hasArg("addr")) {
    String addrStr = server.arg("addr");
    uint8_t addr = (uint8_t)strtol(addrStr.c_str(), NULL, 16);
    
    // Validate I2C address
    if (!isValidI2CAddress(addr)) {
      server.send(400, "text/plain", "Invalid I2C address. Must be 0x08-0x77");
      return;
    }
    
    currentWombatAddress = addr;
    sw.begin(Wire, currentWombatAddress);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSetPin(WebServer& server) {
  // Authentication required for pin configuration
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);
  
  if (server.hasArg("pin") && server.hasArg("mode")) {
    int pin = server.arg("pin").toInt();
    int mode = server.arg("mode").toInt();
    
    // Validate pin number
    if (!isValidPinNumber(pin)) {
      server.send(400, "text/plain", "Invalid pin number");
      return;
    }
    
    // Validate mode range (assuming valid modes 0-40 based on pinModeStrings)
    if (!isValidRange(mode, 0, 40)) {
      server.send(400, "text/plain", "Invalid mode value");
      return;
    }
    
    uint8_t tx[8] = {200, (uint8_t)pin, (uint8_t)mode, 0, 0, 0, 0, 0};
    sw.sendPacket(tx);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleChangeAddr(WebServer& server) {
  // Authentication required for address changes
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);
  
  if (server.hasArg("newaddr")) {
    String val = server.arg("newaddr");
    uint8_t newAddr = (uint8_t)strtol(val.c_str(), NULL, 16);

    if (!isValidI2CAddress(newAddr)) {
      server.send(400, "text/plain", "Invalid I2C address. Must be 0x08-0x77");
      return;
    }
    
    // 1) Library method (known good on SW8B)
    sw.setThroughputPin((uint32_t)newAddr);
    delay(200);

    // 2) Fallback raw packet
    Wire.beginTransmission(currentWombatAddress);
    Wire.write(0xAF);
    Wire.write(0x5F);
    Wire.write(0x42);
    Wire.write(0xAF);
    Wire.write(newAddr);
    Wire.write(0x55);
    Wire.write(0x55);
    Wire.write(0x55);
    Wire.endTransmission();
    i2cMarkTx();

    delay(200);

    // 3) Reset to latch
    sw.begin(Wire, currentWombatAddress);
    sw.hardwareReset();
    delay(1500);

    // 4) Switch to new address
    currentWombatAddress = newAddr;
    sw.begin(Wire, currentWombatAddress);
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleResetTarget(WebServer& server) {
  // Authentication required for hardware reset
  if (!checkAuth(server)) return;
  addSecurityHeaders(server);
  
  sw.hardwareReset();
  server.sendHeader("Location", "/");
  server.send(303);
}
