#include <Arduino.h>
#include "victron_ble.h"
#include "display.h"
#include "config.h"
#include "config_server.h"

VictronBLE* victronBLE = nullptr;
Display* display = nullptr;
ConfigServer* configServer = nullptr;

bool buttonPressed = false;
unsigned long buttonPressTime = 0;
bool lastButtonState = HIGH;

void checkButton() {
  bool currentButtonState = digitalRead(CONFIG_BUTTON_PIN);
  
  if (lastButtonState == HIGH && currentButtonState == LOW) {
    buttonPressTime = millis();
    buttonPressed = true;
    Serial.println("Button pressed - starting config mode in 3 seconds...");
    
    if (display) {
      display->showConfigScreen("Config Mode", "Hold button for 3s", "Release to cancel");
    }
  }
  
  if (buttonPressed && currentButtonState == HIGH) {
    // Button released
    buttonPressed = false;
    if (display) {
      display->showTestScreen();
    }
    Serial.println("Button released - config mode cancelled");
  }
  
  if (buttonPressed && currentButtonState == LOW && 
      millis() - buttonPressTime > 3000) {
    // Button held for 3 seconds
    buttonPressed = false;
    Serial.println("Starting configuration mode...");
    
    if (configServer && !configServer->isInConfigMode()) {
      configServer->startConfigMode();
      
             if (display) {
         display->showConfigScreen("Config Mode Active", 
                                  "WiFi: BTLE-Power-Gauge", 
                                  "Pass: configure123", 
                                  "IP: 192.168.4.1", 
                                  "Timeout: 5 minutes");
       }
    }
  }
  
  lastButtonState = currentButtonState;
}

void initializeBLE() {
  if (victronBLE) {
    delete victronBLE;
    victronBLE = nullptr;
  }
  
  if (!configServer || !configServer->hasValidConfig()) {
    Serial.println("No valid configuration available");
    return;
  }
  
  DeviceConfig config = configServer->getConfig();
  Serial.printf("Initializing BLE with MAC: %s\n", config.mac_address);
  
  victronBLE = new VictronBLE(config.mac_address, config.encryption_key);
  if (!victronBLE->begin()) {
    Serial.println("BLE failed to initialize");
    delete victronBLE;
    victronBLE = nullptr;
  } else {
    victronBLE->setDisplay(display);
    Serial.printf("Monitoring device: %s\n", config.mac_address);
    victronBLE->startScanning();
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("=== Victron Battery Monitor ===");
  Serial.printf("ESP32-S3 @ %d MHz, %d MB Flash\n", ESP.getCpuFreqMHz(), ESP.getFlashChipSize()/1024/1024);
  
  // Setup button
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  
  // Initialize configuration server
  configServer = new ConfigServer();
  if (!configServer->begin()) {
    Serial.println("Config server failed to initialize");
  }
  
  // Initialize display
  display = new Display();
  if (!display->begin()) {
    Serial.println("Display failed to initialize");
    delete display;
    display = nullptr;
  } else {
    display->showTestScreen();
  }
  
  // Initialize Victron BLE with stored configuration
  initializeBLE();
  
  Serial.println("Ready - Hold button for 3s to enter config mode");
}

void loop() {
  static unsigned long lastRefresh = 0;
  static unsigned long lastConfigCheck = 0;
  
  unsigned long now = millis();
  
  // Check button for config mode
  checkButton();
  
  // Handle config server if active
  if (configServer) {
    bool wasInConfigMode = configServer->isInConfigMode();
    configServer->loop();
    
    // If config mode just ended, reinitialize BLE with new config
    if (wasInConfigMode && !configServer->isInConfigMode()) {
      Serial.println("Config mode ended - reinitializing...");
      if (display) {
        display->showTestScreen();
      }
      delay(1000);
      initializeBLE();
    }
  }
  
  // Check for display updates (only if not in config mode)
  if (display && !configServer->isInConfigMode() && now - lastRefresh >= 2000) {
    lastRefresh = now;
    display->refresh();
  }
  
  // Show "no config" message if needed
  if (!configServer->isInConfigMode() && (!victronBLE || !configServer->hasValidConfig()) && 
      now - lastConfigCheck >= 10000) {
    lastConfigCheck = now;
    if (display) {
      display->showConfigScreen("No Configuration", "Hold button 3s", "to configure", "device");
    }
  }
  
  delay(100);
}