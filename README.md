# BTLE Power Gauge

A wireless battery monitoring system that connects to Bluetooth Low Energy (BLE) enabled battery monitors and displays real-time information on an e-ink screen. Originally designed for Victron SmartShunt devices but can work with any compatible BLE battery monitor.

![BTLE Power Gauge](docs/device-image.jpg)
*E-ink display showing battery voltage, current, power, SOC, and time estimates*

## Features

### 📊 Real-Time Battery Monitoring
- **Voltage & Current**: Live battery voltage and current readings
- **State of Charge (SOC)**: Battery percentage with visual battery indicator
- **Power**: Real-time power consumption/generation
- **Time Estimates**: Smart time-to-go (discharging) and time-to-charge calculations
- **Signal Strength**: Bluetooth signal quality indicator
- **Consumed Energy**: Track amp-hours used

### 🔧 Easy Configuration
- **WiFi Setup Portal**: Press and hold button to enter configuration mode
- **Web Interface**: User-friendly web portal for device setup
- **Persistent Storage**: Configuration saved in flash memory
- **No Hardcoding**: Configure any compatible device without code changes

### ⚡ Power Efficient
- **E-ink Display**: Ultra-low power consumption, perfect for battery applications
- **Smart Updates**: Intelligent change detection prevents unnecessary refreshes
- **Partial Refresh**: Fast partial updates for real-time data
- **Configurable Thresholds**: Tune update sensitivity for optimal power usage

### 🎯 User Experience
- **Visual Feedback**: Clear status indicators and error messages
- **Button Control**: Simple one-button operation
- **Auto-timeout**: Configuration portal automatically closes
- **Responsive Design**: Mobile-friendly web interface

## Hardware Requirements

### Supported Devices
- **Vision Master E290**: E-ink display development board (recommended)
- **ESP32-S3**: Main microcontroller with BLE support
- **2.9" E-ink Display**: 296x128 pixel monochrome display
- **Button**: Configuration trigger (GPIO 21)

### Compatible Battery Monitors
- Victron SmartShunt (all models)
- Any BLE device using similar encryption protocol
- Devices with "Instant Readout" BLE advertising

## Quick Start

### 1. Hardware Setup
1. Flash the firmware to your ESP32-S3 device
2. Ensure button is connected to GPIO 21
3. Power on the device

### 2. Initial Configuration
1. **Enter Config Mode**: Hold the button for 3 seconds
2. **Connect to WiFi**: Join network "BTLE-Power-Gauge" (password: configure123)
3. **Open Browser**: Go to http://192.168.4.1
4. **Configure Device**: Enter your battery monitor's MAC address and encryption key
5. **Save Settings**: Device will restart and connect automatically

### 3. Finding Device Credentials
Use your battery monitor's mobile app:
1. Connect to your device
2. Go to **Settings → Product Info** (note Bluetooth MAC address)
3. Go to **Settings → Instant Readout** (copy encryption key)
4. Remove colons from MAC address when entering in config portal

## Display Information

### Main Screen Layout
```
12.5V  85%  [████████████    ]
+2.5A  30W               15s

TTG: 4h23m              ||||

Used: 15.2Ah            OK
```

### Display Elements
- **Top Row**: Battery voltage, SOC percentage, visual battery indicator
- **Second Row**: Current (+ for charging, - for discharging), power, data age
- **Third Row**: Time estimate (TTG/TTC), signal strength bars
- **Bottom Row**: Consumed amp-hours, alarm status

### Time Calculations
- **TTG (Time To Go)**: Estimated time until battery empty (when discharging)
- **TTC (Time To Charge)**: Estimated time to full charge (when charging)
- **Smart Logic**: Uses configurable battery capacity and real-time current

## Configuration Options

### Battery Settings (`config.h`)
```cpp
#define BATTERY_CAPACITY_AH 100.0f        // Your battery capacity
#define MIN_CURRENT_THRESHOLD 0.1f        // Minimum current for calculations
```

### Display Update Thresholds
```cpp
#define VOLTAGE_CHANGE_THRESHOLD 0.1f     // Volts
#define CURRENT_CHANGE_THRESHOLD 0.2f     // Amps  
#define SOC_CHANGE_THRESHOLD 1.0f         // Percent
#define POWER_CHANGE_THRESHOLD 10.0f      // Watts
#define TIME_CHANGE_THRESHOLD 5           // Minutes
```

### Refresh Timing
```cpp
#define PERIODIC_REFRESH_INTERVAL 300000  // 5 minutes
#define FULL_REFRESH_INTERVAL 600000      // 10 minutes
```

## Building and Flashing

### Prerequisites
- [PlatformIO](https://platformio.org/) (recommended) or Arduino IDE
- ESP32 board support package

### Using PlatformIO
```bash
# Clone repository
git clone https://github.com/yourusername/btle-power-gauge.git
cd btle-power-gauge

# Build and upload
pio run --target upload

# Monitor serial output
pio device monitor
```

### Using Arduino IDE
1. Install ESP32 board support
2. Install required libraries:
   - NimBLE-Arduino
   - GxEPD2
3. Select "ESP32-S3 Dev Module" as board
4. Compile and upload

## Troubleshooting

### Device Not Connecting
- Verify MAC address format (12 hex characters, no colons)
- Check encryption key (32 hex characters)
- Ensure battery monitor has BLE enabled
- Check distance between devices

### Display Issues
- **Blank Screen**: Check power connections and EPD_POWER pin
- **No Updates**: Verify current thresholds aren't too high
- **Ghosting**: Full refresh occurs every 10 minutes automatically

### Configuration Problems
- **Can't Access Portal**: Ensure button is held for full 3 seconds
- **Web Page Won't Load**: Check WiFi connection to "BTLE-Power-Gauge"
- **Settings Not Saving**: Configuration automatically saves to flash memory

## Technical Details

### Architecture
- **ESP32-S3**: Main processor with dual-core, WiFi, and BLE
- **NimBLE**: Lightweight Bluetooth stack for device communication
- **GxEPD2**: E-ink display driver with partial update support
- **Preferences**: Non-volatile storage for configuration
- **WebServer**: Built-in HTTP server for configuration portal

### Security
- **WPA2**: Configuration portal uses WPA2 encryption
- **Local Only**: No external network access required
- **Timeout**: Configuration portal auto-closes for security

### Power Consumption
- **Active Monitoring**: ~50mA average (depending on BLE activity)
- **Display Updates**: Brief current spikes during refresh
- **Deep Sleep**: Not implemented (always-on monitoring)

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues for:
- Additional battery monitor support
- Display layout improvements
- Power optimization features
- Documentation updates

## License

This project is open source. See LICENSE file for details.

## Acknowledgments

- Built for Vision Master E290 hardware
- Uses NimBLE and GxEPD2 libraries
- Inspired by the need for reliable battery monitoring in off-grid applications

---

**Need Help?** Open an issue on GitHub or check the troubleshooting section above. 