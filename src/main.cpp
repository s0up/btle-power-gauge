#include <Arduino.h>
#include "victron_ble.h"
#include "display.h"
#include "config.h"
#include "config_server.h"
#include "esp_sleep.h"
#include "driver/gpio.h"

VictronBLE* victronBLE = nullptr;
Display* display = nullptr;
ConfigServer* configServer = nullptr;

// Button handling with hardware interrupts - no more blocking issues!
struct ButtonEvent {
  unsigned long timestamp;
  bool pressed;  // true = pressed, false = released
};

#define MAX_BUTTON_EVENTS 10
volatile ButtonEvent buttonEvents[MAX_BUTTON_EVENTS];
volatile int eventHead = 0;
volatile int eventTail = 0;
volatile unsigned long lastInterruptTime = 0;
const unsigned long DEBOUNCE_MS = 50;

// Interrupt Service Routine - keep it simple and fast
void IRAM_ATTR buttonISR() {
  unsigned long now = millis();
  
  // Simple debounce in ISR
  if (now - lastInterruptTime < DEBOUNCE_MS) {
    return;
  }
  lastInterruptTime = now;
  
  // Record the event
  int nextHead = (eventHead + 1) % MAX_BUTTON_EVENTS;
  if (nextHead != eventTail) {  // Don't overflow
    buttonEvents[eventHead].timestamp = now;
    buttonEvents[eventHead].pressed = (digitalRead(CONFIG_BUTTON_PIN) == LOW);
    eventHead = nextHead;
  }
}

// Button state machine for processing events
enum ButtonState {
  IDLE,
  FIRST_PRESS,
  WAITING_FOR_SECOND,
  SECOND_PRESS
};

ButtonState buttonState = IDLE;
unsigned long buttonPressStart = 0;
unsigned long firstReleaseTime = 0;
int pressCount = 0;

void enterDeepSleep() {
  Serial.println("=== ENTERING DEEP SLEEP MODE ===");
  Serial.println("Showing sleep screen...");
  
  // Show sleep screen
  if (display) {
    display->showSleepScreen();
    delay(2000);  // Show the sleep screen for 2 seconds
  }
  
  Serial.println("Stopping BLE and config server...");
  
  // Stop BLE to save power
  if (victronBLE) {
    delete victronBLE;
    victronBLE = nullptr;
  }
  
  // Stop config server
  if (configServer) {
    delete configServer;
    configServer = nullptr;
  }
  
  Serial.println("Configuring wake-up on button press...");
  
  // Configure wake-up on button press (LOW level, since button is pulled up)
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_21, 0);
  
  // Turn off display power
  digitalWrite(EPD_POWER, LOW);
  
  Serial.println("Going to sleep in 1 second...");
  Serial.flush();
  delay(1000);
  
  // Enter deep sleep
  esp_deep_sleep_start();
}

void processButtonEvents() {
  // Process all pending button events
  while (eventTail != eventHead) {
    // Manually copy from volatile struct to avoid compiler issues
    ButtonEvent event;
    event.timestamp = buttonEvents[eventTail].timestamp;
    event.pressed = buttonEvents[eventTail].pressed;
    eventTail = (eventTail + 1) % MAX_BUTTON_EVENTS;
    
    Serial.printf("Button %s at %lu ms\n", event.pressed ? "pressed" : "released", event.timestamp);
    
    if (event.pressed) {
      // Button pressed
      if (buttonState == IDLE) {
        buttonState = FIRST_PRESS;
        buttonPressStart = event.timestamp;
        pressCount = 1;
        Serial.println("First press detected");
        
        // Don't show display feedback yet - wait to see if it's a double-press
        
      } else if (buttonState == WAITING_FOR_SECOND) {
        buttonState = SECOND_PRESS;
        pressCount = 2;
        Serial.println("Second press detected - DOUBLE PRESS!");
        
        // Immediate double press action
        enterDeepSleep();
        return;
      }
      
    } else {
      // Button released
      if (buttonState == FIRST_PRESS) {
        unsigned long pressDuration = event.timestamp - buttonPressStart;
        Serial.printf("First press duration: %lu ms\n", pressDuration);
        
        if (pressDuration >= 6000) {
          // Long press - start config mode
          buttonState = IDLE;
          Serial.println("Long press detected (6+ seconds) - starting config mode");
          
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
        } else {
          // Short press - wait for potential second press (no display update yet)
          buttonState = WAITING_FOR_SECOND;
          firstReleaseTime = event.timestamp;
          Serial.printf("Press duration %lu ms - waiting for second press...\n", pressDuration);
          
          // Don't show "Tap Again" screen - it's confusing during fast double-press
        }
      }
    }
  }
  
  // Handle timeouts
  unsigned long now = millis();
  if (buttonState == WAITING_FOR_SECOND && (now - firstReleaseTime) > 2000) {
    buttonState = IDLE;
    Serial.println("Double press timeout - treating as single press");
    
    // Only now show feedback since it was just a single press
    if (display) {
      display->showConfigScreen("Single Press", "Hold 6s for config", "Double-press for sleep", "");
    }
  }
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
  
  // Check wake-up reason
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  bool wokeFromSleep = false;
  
  if (wakeup_reason == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("=== Woke up from deep sleep (button press) ===");
    wokeFromSleep = true;
  } else {
    Serial.println("=== Victron Battery Monitor ===");
  }
  
  Serial.printf("ESP32-S3 @ %d MHz, %d MB Flash\n", ESP.getCpuFreqMHz(), ESP.getFlashChipSize()/1024/1024);
  
  // Setup button with hardware interrupt
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(CONFIG_BUTTON_PIN), buttonISR, CHANGE);
  Serial.println("Button interrupt configured on pin 21");
  
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
    if (wokeFromSleep) {
      display->showConfigScreen("Wake Up", "Device awakened", "from sleep mode", "Initializing...");
      delay(2000);
    }
    display->showTestScreen();
    if (wokeFromSleep) {
      display->forceNextUpdate(); // Force display to update with new data after wake
      Serial.println("Wake from sleep detected - display state reset");
    }
  }
  
  // Initialize Victron BLE with stored configuration
  initializeBLE();
  
  Serial.println("Ready - Hold button for 6s to enter config mode");
  Serial.println("       Double-press button to enter sleep mode");
  Serial.println("       (Hardware interrupt-based - no blocking!)");
}

void loop() {
  static unsigned long lastRefresh = 0;
  static unsigned long lastConfigCheck = 0;
  
  unsigned long now = millis();
  
  // Process button events
  processButtonEvents();
  
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
      display->showConfigScreen("No Configuration", "Hold button 6s", "to configure", "device");
    }
  }
  
  delay(100);
}