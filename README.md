# BTLE Power Gauge

ESP32-S3 based device that polls Victron SmartShunt devices via BLE GATT and displays battery data on a 2.9" e-ink screen.

## Hardware

- ESP32-S3 microcontroller
- 2.9" e-ink display (296x128 pixels) 
- Button on GPIO 21 for configuration
- Compatible with Vision Master E290 development board

## Functionality

- Connects to Victron SmartShunt via BLE
- Decrypts instant readout data using AES-CTR
- Displays voltage, current, power, SOC, and time estimates
- Calculates time-to-go and time-to-charge based on configurable battery capacity
- Web-based configuration portal accessible via WiFi AP

## Configuration

### Initial Setup
1. Hold button for 3 seconds to enter configuration mode
2. Connect to WiFi network "BTLE-Power-Gauge" (password: configure123)
3. Navigate to http://192.168.4.1
4. Enter Victron device MAC address and encryption key
5. Configuration persists in ESP32 NVS storage

### Finding Victron Credentials
- MAC Address: Found in VictronConnect app under Settings → Product Info
- Encryption Key: Found in VictronConnect app under Settings → Instant Readout

## Display Layout

```
12.5V  85%  [████████]
+2.5A  30W         15s

TTG: 4h23m        ||||

Used: 15.2Ah        OK
```

- Row 1: Voltage, SOC percentage, battery level indicator
- Row 2: Current (+/- for charge/discharge), power, data age
- Row 3: Time estimate (TTG=time to go, TTC=time to charge), signal strength
- Row 4: Consumed amp-hours, alarm status

## Configuration Options

Edit `src/config.h`:

```cpp
#define BATTERY_CAPACITY_AH 100.0f        // Battery capacity for time calculations
#define VOLTAGE_CHANGE_THRESHOLD 0.1f     // Minimum voltage change to update display
#define CURRENT_CHANGE_THRESHOLD 0.2f     // Minimum current change to update display
#define SOC_CHANGE_THRESHOLD 1.0f         // Minimum SOC change to update display
```

## Building

### PlatformIO
```bash
git clone <repository>
cd btle-power-gauge
pio run --target upload
```

### Dependencies
- NimBLE-Arduino (BLE stack)
- GxEPD2 (e-ink display driver)
- ESP32 Preferences (NVS storage)

## Protocol Details

Connects to Victron devices using:
- BLE GATT advertisements
- Manufacturer ID 0x02E1 
- AES-CTR decryption with device-specific key
- Parses SmartShunt data format for voltage, current, SOC, etc.

## Power Management

- Partial e-ink updates minimize power consumption
- Configurable update thresholds prevent unnecessary refreshes
- Full display refresh every 10 minutes to prevent ghosting

 