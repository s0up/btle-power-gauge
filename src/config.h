#ifndef CONFIG_H
#define CONFIG_H

// Hardware pins
#define CONFIG_BUTTON_PIN 21   // Button pin based on Vision Master E290 hardware

// SmartShunt device configuration
// #define DEVICE_NAME "SmartShunt HQ2448HY62J"
// #define DEVICE_PIN "171173" // Bonding PIN
#define INSTANT_READOUT_ENCRYPTION_KEY "64cd146fe6771ef40610ecf50f3bb06a"
#define INSTANT_READOUT_MAC_ADDRESS "d6ec4c9e6307"

// WiFi Configuration Server
#define CONFIG_AP_SSID "BTLE-Power-Gauge"
#define CONFIG_AP_PASSWORD "configure123"
#define CONFIG_SERVER_PORT 80
#define CONFIG_TIMEOUT_MS 300000  // 5 minutes

// Battery configuration
#define BATTERY_CAPACITY_AH 100.0f
#define MIN_CURRENT_THRESHOLD 0.1f

// Display update thresholds
#define VOLTAGE_CHANGE_THRESHOLD 0.1f    // Volts
#define CURRENT_CHANGE_THRESHOLD 0.2f    // Amps
#define SOC_CHANGE_THRESHOLD 1.0f        // Percent
#define POWER_CHANGE_THRESHOLD 10.0f     // Watts
#define TIME_CHANGE_THRESHOLD 5          // Minutes
#define CONSUMED_AH_THRESHOLD 0.5f       // Amp-hours

// Display refresh timing
#define PERIODIC_REFRESH_INTERVAL 300000   // 5 minutes
#define FULL_REFRESH_INTERVAL 600000       // 10 minutes

#endif // CONFIG_H