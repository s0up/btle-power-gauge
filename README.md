# BTLE Power Gauge

Monitors Victron SmartShunt devices via Bluetooth and displays battery data on an e-ink screen.

## Hardware Required

**Vision Master E290 Development Board**
- ESP32-S3 with built-in 2.9" e-ink display
- Available on Amazon - search "Vision Master E290" or "ESP32-S3 e-ink display board"
- Has configuration button on GPIO 21

## Setup

1. Flash this firmware to your Vision Master E290
2. Hold the button for 3 seconds to enter config mode
3. Connect to WiFi "BTLE-Power-Gauge" (password: configure123)
4. Go to http://192.168.4.1 and enter your Victron device details

## Getting Victron Credentials

In the VictronConnect app:
- **MAC Address**: Settings → Product Info → Bluetooth Address
- **Encryption Key**: Settings → Instant Readout → Show (copy the key)

## Display

Shows real-time battery data:
- Voltage, current, power, state of charge
- Time remaining (discharge) or time to full (charging)
- Signal strength and alarm status
- Updates automatically when values change significantly

## Building

```bash
git clone <repository>
cd btle-power-gauge
pio run --target upload
```

## Configuration

Edit `src/config.h` to customize:
- Battery capacity for time calculations
- Display update thresholds
- Refresh intervals
 