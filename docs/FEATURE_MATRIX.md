# Feature Matrix

## Overview
This document maps every feature in the firmware to its module owner and UI screen controls after the refactoring.

## Feature Inventory

| Feature | Module Owner | UI Screen/Controls | Config Keys | Current Status |
|---------|--------------|-------------------|-------------|----------------|
| **I2C Device Management** |
| Fast I2C Scanner (0x08-0x77) | `services/i2c_manager` | I2C Tools → Scan Button | `i2c.sda`, `i2c.scl` | ✅ Exists |
| Deep I2C Analysis | `services/i2c_manager` | I2C Tools → Deep Scan | - | ✅ Exists |
| I2C Traffic Monitoring | `services/i2c_manager` | Status Bar → I2C TX/RX Indicators | - | ✅ Exists |
| **SerialWombat Integration** |
| Device Detection | `services/serialwombat` | Serial Wombat → Device List | `sw.target_address` | ✅ Exists |
| Firmware Variant Detection | `services/serialwombat` | Serial Wombat → Device Info | - | ✅ Exists |
| Pin Mode Configuration (41 modes) | `services/serialwombat` | Serial Wombat → Pin Config | - | ✅ Exists |
| Device Mode Control (H-Bridge, Servo, etc.) | `services/serialwombat` | Serial Wombat → Device Control | - | ✅ Exists |
| Target Address Switching | `services/serialwombat` | Serial Wombat → Change Address | - | ✅ Exists |
| Hardware Reset | `services/serialwombat` | Serial Wombat → Reset Device | - | ✅ Exists |
| **Firmware Management** |
| Multi-Slot Firmware Storage | `services/firmware_manager` | Firmware → Slot Management | - | ✅ Exists |
| Binary Upload | `services/firmware_manager` | Firmware → Upload Binary | - | ✅ Exists |
| IntelHEX Upload & Conversion | `services/firmware_manager` | Firmware → Upload HEX | - | ✅ Exists |
| SD Import with Conversion | `services/firmware_manager` | Firmware → Import from SD | - | ✅ Exists |
| Firmware Flashing to Device | `services/firmware_manager` | Firmware → Flash to Device | - | ✅ Exists |
| Firmware Metadata Tracking | `services/firmware_manager` | Firmware → Version Info | - | ✅ Exists |
| **Web Server** |
| HTTP Dashboard | `services/web_server` | (Web Browser) | `network.web_port` | ✅ Exists |
| REST API Endpoints | `services/web_server` | (Web Browser + Mobile Apps) | - | ✅ Exists |
| HTTP Basic Authentication | `services/web_server` | Settings → Security | `security.username`, `security.password` | ✅ Exists |
| Security Headers (CSP, CORS) | `services/web_server` | Settings → Security | `security.cors_origin` | ✅ Exists |
| Rate Limiting | `services/web_server` | - | `security.auth_lockout_ms` | ✅ Exists |
| **Storage & File Management** |
| LittleFS Config Storage | `hal/storage` | Storage → Internal Storage | - | ✅ Exists |
| SD Card Support (SPI) | `hal/storage` | Storage → SD Card | `sd.cs`, `sd.sck`, `sd.mosi`, `sd.miso` | ✅ Exists |
| File Browser | `services/file_manager` | Storage → File Browser | - | ✅ Exists |
| File Upload | `services/file_manager` | Storage → Upload | - | ✅ Exists |
| File Delete/Rename | `services/file_manager` | Storage → File Operations | - | ✅ Exists |
| Path Traversal Protection | `services/file_manager` | - | - | ✅ Exists |
| Storage Statistics | `services/file_manager` | Storage → Usage Stats | - | ✅ Exists |
| **Display & UI** |
| LovyanGFX Driver Support | `hal/display` | Settings → Display | `display.driver`, `display.width`, `display.height` | ✅ Exists |
| Multi-Panel Support (ILI9341, ST7789, ST7796, RGB) | `hal/display` | Settings → Panel Type | `display.panel` | ✅ Exists |
| Backlight Control | `hal/display` | Settings → Backlight | `display.bl_pin` | ✅ Exists |
| Display Rotation | `hal/display` | Settings → Rotation | `display.rotation` | ✅ Exists |
| LVGL v8/v9 Dual Support | `ui/lvgl_wrapper` | - | - | ✅ Exists |
| First-Boot Model Selection | `ui/setup` | (First Boot Screen) | `system.model` | ✅ Exists |
| Splash Screen Picker | `ui/setup` | (First Boot Screen) | `display.splash_path` | ✅ Exists |
| Status Bar (Time, WiFi, I2C, Battery) | `ui/components/statusbar` | (All Screens) | - | ✅ Exists |
| **Touch Input** |
| XPT2046 Resistive Touch (SPI) | `drivers/touch/xpt2046` | Settings → Touch | `touch.type`, `touch.cs`, `touch.irq` | ✅ Exists |
| GT911 Capacitive Touch (I2C) | `drivers/touch/gt911` | Settings → Touch | `touch.type`, `touch.i2c_address` | ✅ Exists |
| Touch Calibration | `ui/screens/touch_calibration` | Settings → Touch Calibration | `touch.cal_x_min/max`, `touch.cal_y_min/max` | 🔄 Add UI |
| **WiFi & Network** |
| WiFiManager Auto-Config | `services/network` | Network → WiFi Setup | - | ✅ Exists |
| AP Fallback Mode | `services/network` | Network → Access Point | `wifi.ap_ssid`, `wifi.ap_password` | ✅ Exists |
| WiFi Status Monitoring | `services/network` | Status Bar + Network Screen | - | ✅ Exists |
| RSSI Display | `services/network` | Status Bar → WiFi Icon | - | ✅ Exists |
| Network Configuration | `services/network` | Network → Settings | `wifi.ssid`, `wifi.password` | ✅ Exists |
| TCP Bridge (I2C over TCP) | `services/tcp_bridge` | Network → TCP Bridge | `network.tcp_port` | ✅ Exists |
| **OTA & Updates** |
| OTA Firmware Updates | `services/ota` | Settings → OTA Update | `ota.password` | ✅ Exists |
| OTA Password Protection | `services/ota` | Settings → Security | `ota.password` | ✅ Exists |
| **Configuration System** |
| JSON Config Load/Save | `config/config_manager` | Settings → Config | - | ✅ Exists |
| Named Config Profiles | `config/config_manager` | Settings → Profiles | - | ✅ Exists |
| Model Presets (9 CYD Variants) | `config/presets` | Settings → Model | `system.model` | ✅ Exists |
| Config Validation | `config/validator` | - | - | ✅ Exists |
| Default Fallbacks | `config/defaults` | - | - | ✅ Exists |
| **Battery & Power** |
| Battery ADC Monitoring | `hal/adc` | Status Bar → Battery Icon | `battery.adc_pin`, `battery.min_mv`, `battery.max_mv` | 🔄 Enhance |
| Battery Percentage Calculation | `services/power_manager` | Status Bar → Battery % | - | 🔄 Add |
| **RGB LED** |
| RGB LED Control | `hal/gpio/led` | Settings → LED Test | `led.r_pin`, `led.g_pin`, `led.b_pin`, `led.common_anode` | 🔄 Add Support |
| LED Status Indicators | `services/indicators` | - | - | 🔄 Add |
| **Logging & Diagnostics** |
| Serial Console Logging | `core/logging` | Logs → View Logs | `logging.level` | ✅ Exists |
| System Information | `services/system_info` | System → Info | - | ✅ Exists |
| Heap/PSRAM Monitoring | `services/system_info` | System → Memory | - | ✅ Exists |
| CPU Frequency | `services/system_info` | System → CPU | - | ✅ Exists |
| Uptime Tracking | `services/system_info` | System → Uptime | - | ✅ Exists |
| Reset Reason | `services/system_info` | System → Last Reset | - | ✅ Exists |

## New UI Screens (PHASE 3)

### 1. Home Dashboard
- **Purpose**: Central navigation hub
- **Components**: 
  - Feature tiles/cards for each module
  - Quick status indicators
  - Navigation to all screens

### 2. System Status
- **Purpose**: System diagnostics and information
- **Components**:
  - Heap memory (free/used/max)
  - PSRAM status (if available)
  - Uptime display
  - Reset reason
  - CPU frequency
  - Flash usage
  - IP address
  - MAC address

### 3. Network
- **Purpose**: WiFi and network management
- **Components**:
  - WiFi connection status
  - SSID, RSSI signal strength
  - IP address, gateway, subnet
  - Connect/disconnect buttons
  - WiFi reconfiguration
  - TCP Bridge status
  - MQTT status (placeholder for future)

### 4. Storage
- **Purpose**: File and storage management
- **Components**:
  - LittleFS usage (total/used/free)
  - SD card mount/unmount controls
  - SD card usage stats
  - File browser (LittleFS + SD)
  - File operations (view, delete, rename, upload)
  - Safe eject functionality

### 5. I2C Tools
- **Purpose**: I2C device discovery and management
- **Components**:
  - Quick scan button + device list
  - Deep scan with device details
  - I2C device detail pages
  - Device address/variant info
  - Real-time I2C traffic counters
  - Pin configuration (SDA/SCL)

### 6. Serial Wombat Manager
- **Purpose**: Serial Wombat device configuration
- **Components**:
  - Connected device info
  - Firmware version/variant
  - Pin mode configuration matrix
  - Device mode controls (H-Bridge, Servo, etc.)
  - Target address change
  - Hardware reset

### 7. Firmware Manager
- **Purpose**: Firmware upload and flashing
- **Components**:
  - Firmware slot list with versions
  - Upload binary/HEX buttons
  - Import from SD
  - Flash to device button
  - Progress indicators
  - Firmware metadata viewer

### 8. Touch Calibration
- **Purpose**: Resistive touch calibration
- **Components**:
  - Real-time raw touch coordinates
  - Calibration targets (corners)
  - Save calibration button
  - Reset to defaults
  - Test area

### 9. Logs Viewer
- **Purpose**: On-device log viewing
- **Components**:
  - Log message scrollable list
  - Filter by level (Debug, Info, Warning, Error)
  - Export to SD button
  - Clear logs button
  - Auto-scroll toggle

### 10. Settings/Config
- **Purpose**: System configuration
- **Components**:
  - Security settings (username, password)
  - Display settings (brightness, rotation)
  - Model selection
  - Config profile save/load/delete
  - Factory reset
  - OTA update trigger

## Status Indicators

### Top Status Bar (Present on all screens)
| Indicator | Source | Update Frequency |
|-----------|--------|------------------|
| Back/Home Button | UI Navigation | On-demand |
| WiFi Status Icon + RSSI | `services/network` | 1-2 seconds |
| I2C TX/RX Activity | `services/i2c_manager` | Real-time blink |
| Battery Icon + % | `hal/adc` → `services/power_manager` | 5 seconds |
| Clock (Time) | NTP or uptime | 1 second |

## Configuration Keys Reference

### Display
```
display.driver = st7796u|ili9341|st7789|rgb
display.width = 320|480|800
display.height = 240|480
display.rotation = 0|1|2|3
display.spi_sck = <pin>
display.spi_mosi = <pin>
display.spi_miso = <pin>
display.cs = <pin>
display.dc = <pin>
display.bl = <pin>
display.reset = <pin>|en_shared
display.backlight_freq = 12000
display.splash_path = /sd/splash.png
```

### Touch
```
touch.type = xpt2046|gt911|i2c_generic
touch.cs = <pin>
touch.irq = <pin>
touch.spi_sck = <pin>  # if separate from display
touch.spi_mosi = <pin>
touch.spi_miso = <pin>
touch.i2c_address = 0x5D  # for capacitive
touch.cal_x_min = 200
touch.cal_x_max = 3900
touch.cal_y_min = 200
touch.cal_y_max = 3900
```

### SD Card
```
sd.enabled = true|false
sd.cs = <pin>
sd.sck = <pin>
sd.mosi = <pin>
sd.miso = <pin>
```

### I2C
```
i2c.sda = <pin>
i2c.scl = <pin>
i2c.frequency = 100000|400000
```

### LED RGB
```
led.r_pin = <pin>
led.g_pin = <pin>
led.b_pin = <pin>
led.common_anode = true|false
```

### Battery
```
battery.adc_pin = <pin>
battery.min_mv = 3300
battery.max_mv = 4200
```

### WiFi
```
wifi.ssid = <string>
wifi.password = <string>
wifi.ap_ssid = Wombat-Setup
wifi.ap_password = <string>
```

### Network
```
network.hostname = wombat-bridge
network.web_port = 80
network.tcp_port = 3000
```

### Security
```
security.enabled = true|false
security.username = admin
security.password = <string>
security.cors_origin = *|https://domain.com
security.auth_lockout_ms = 5000
```

### OTA
```
ota.enabled = true|false
ota.password = <string>
ota.port = 3232
```

### System
```
system.model = lcdwiki_35_esp32_32e|2432S028R|8048S070|...
system.configured = true|false
system.headless = false
logging.level = debug|info|warning|error
```

## Default Profile: LCDWIKI 3.5" ESP32-32E

**Profile Name**: `lcdwiki_35_esp32_32e`

```ini
[system]
model=lcdwiki_35_esp32_32e

[display]
driver=st7796u
width=320
height=480
rotation=0
spi_sck=14
spi_mosi=13
spi_miso=12
cs=15
dc=2
bl=27
reset=en_shared
backlight_freq=12000

[touch]
type=xpt2046
cs=33
irq=36
spi_sck=14
spi_mosi=13
spi_miso=12

[sd]
enabled=true
cs=5
sck=18
mosi=23
miso=19

[i2c]
sda=32
scl=25
frequency=400000

[led]
r_pin=22
g_pin=16
b_pin=17
common_anode=true

[battery]
adc_pin=34
min_mv=3300
max_mv=4200

[network]
hostname=wombat-lcdwiki
web_port=80
tcp_port=3000

[security]
enabled=true
username=admin
# password MUST be changed in code
cors_origin=*

[ota]
enabled=true
port=3232
```

## Implementation Notes

1. **Existing Features**: Most features already exist in the monolithic .ino file and need to be refactored into modules
2. **New UI**: The LVGL UI screens (#1-10) are new implementations that expose existing functionality
3. **Enhanced Features**: Battery monitoring, RGB LED, and touch calibration need UI enhancements
4. **Module Boundaries**: Clear separation between HAL (hardware), drivers (specific chips), services (business logic), and UI (presentation)
5. **Event Bus**: UI actions trigger service calls via event bus to decouple layers
6. **No Placeholders**: All listed features will be fully implemented, not stubbed

## Feature Coverage Guarantee

Every feature listed in this matrix will:
- Have a dedicated module/service owner
- Be exposed via the LVGL UI (where applicable)
- Have configuration keys (where applicable)
- Be fully functional after refactoring (no regressions)
- Include proper error handling and user feedback
