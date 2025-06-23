#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <Arduino.h>
#include "NimBLEDevice.h"
#include "display.h"

class VictronBLE {
private:
  NimBLEAddress targetAddress;
  uint8_t encryptionKey[16];
  NimBLEScan* pBLEScan;
  Display* display;
  
  // Helper functions
  void hexStringToBytes(const char* hexString, uint8_t* byteArray, size_t byteArraySize);
  NimBLEAddress macStringToAddress(const char* macString);
  bool decryptVictronData(const uint8_t* encryptedData, size_t dataLen, 
                         const uint8_t* key, uint8_t* decryptedData);
  uint32_t extractBits(const uint8_t* data, int startBit, int numBits);
  int32_t signExtend(uint32_t value, int bits);
  void parseSmartShuntData(const uint8_t* data, size_t len, int8_t rssi);

public:
  VictronBLE(const char* macAddress, const char* encryptionKey);
  bool begin();
  void setDisplay(Display* disp);
  void startScanning();
  void handleAdvertisement(NimBLEAdvertisedDevice* advertisedDevice);
};

// Callback class that forwards to VictronBLE instance
class VictronAdvertisingCallback: public NimBLEAdvertisedDeviceCallbacks {
private:
  VictronBLE* victronBLE;
  
public:
  VictronAdvertisingCallback(VictronBLE* ble) : victronBLE(ble) {}
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
};

#endif // VICTRON_BLE_H 