#include <Arduino.h>
#include "victron_ble.h"
#include "display.h"
#include "config.h"

VictronBLE* victronBLE;
Display* display;

void setup() {
  Serial.begin(115200);
  Serial.println("Victron SmartShunt BLE Reader");

  display = new Display();
  if (!display->begin()) {
    Serial.println("Failed to initialize display");
    while (true);
  }

  victronBLE = new VictronBLE(INSTANT_READOUT_MAC_ADDRESS, INSTANT_READOUT_ENCRYPTION_KEY);
  if (!victronBLE->begin()) {
    Serial.println("Failed to initialize BLE");
    return;
  }

  victronBLE->setDisplay(display);
  
  Serial.printf("Monitoring: %s\n", INSTANT_READOUT_MAC_ADDRESS);
  victronBLE->startScanning();
}

void loop() {
  display->refresh();
  delay(100);
}