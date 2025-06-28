#include "victron_ble.h"
#include <mbedtls/aes.h>

VictronBLE::VictronBLE(const char* macAddress, const char* encryptionKeyStr) {
  targetAddress = macStringToAddress(macAddress);
  hexStringToBytes(encryptionKeyStr, encryptionKey, 16);
  pBLEScan = nullptr;
  display = nullptr;
}

bool VictronBLE::begin() {
  NimBLEDevice::init("Victron_Reader");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  pBLEScan = NimBLEDevice::getScan();
  if (!pBLEScan) {
    return false;
  }
  
  pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisingCallback(this));
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(50);
  pBLEScan->setActiveScan(false);
  pBLEScan->setDuplicateFilter(false);
  
  return true;
}

void VictronBLE::setDisplay(Display* disp) {
  display = disp;
}

void VictronBLE::startScanning() {
  if (pBLEScan) {
    pBLEScan->start(0, nullptr, false);
  }
}

void VictronBLE::handleAdvertisement(NimBLEAdvertisedDevice* advertisedDevice) {
  if (advertisedDevice->getAddress() != targetAddress) {
    return;
  }
  
  if (!advertisedDevice->haveManufacturerData()) {
    return;
  }
  
  std::string manufacturerData = advertisedDevice->getManufacturerData();
  if (manufacturerData.length() < 4) {
    return;
  }
  
  uint16_t manufacturerId = (uint8_t)manufacturerData[0] | 
                           ((uint8_t)manufacturerData[1] << 8);
  
  if (manufacturerId != 0x02E1) {
    return;
  }
  
  const uint8_t* encryptedPayload = (const uint8_t*)manufacturerData.data() + 2;
  size_t payloadLen = manufacturerData.length() - 2;
  
  uint8_t decryptedData[64];
  if (decryptVictronData(encryptedPayload, payloadLen, encryptionKey, decryptedData)) {
    parseSmartShuntData(decryptedData, payloadLen, advertisedDevice->getRSSI());
  }
}

void VictronBLE::hexStringToBytes(const char* hexString, uint8_t* byteArray, size_t byteArraySize) {
  size_t hexLen = strlen(hexString);
  for (size_t i = 0; i < byteArraySize && i * 2 < hexLen; i++) {
    char hexByte[3] = {hexString[i * 2], hexString[i * 2 + 1], '\0'};
    byteArray[i] = (uint8_t)strtol(hexByte, NULL, 16);
  }
}

NimBLEAddress VictronBLE::macStringToAddress(const char* macString) {
  char formattedMac[18];
  snprintf(formattedMac, sizeof(formattedMac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           macString[0], macString[1], macString[2], macString[3],
           macString[4], macString[5], macString[6], macString[7],
           macString[8], macString[9], macString[10], macString[11]);
  return NimBLEAddress(formattedMac);
}

bool VictronBLE::decryptVictronData(const uint8_t* encryptedData, size_t dataLen, 
                                   const uint8_t* key, uint8_t* decryptedData) {
  if (dataLen < 8) return false;
  
  if (encryptedData[0] != 0x10) {
    Serial.printf("Invalid record type: 0x%02X\n", encryptedData[0]);
    return false;
  }
  
  uint16_t nonce = encryptedData[5] | (encryptedData[6] << 8);
  uint8_t key_byte = encryptedData[7];
  
  if (key_byte != key[0]) {
    Serial.printf("Key mismatch. Expected: 0x%02X, Got: 0x%02X\n", key[0], key_byte);
    return false;
  }
  
  const uint8_t* encrypted_payload = encryptedData + 8;
  size_t encrypted_len = dataLen - 8;
  
  // Setup AES-CTR decryption
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  int ret = mbedtls_aes_setkey_enc(&aes, key, 128);
  if (ret != 0) {
    mbedtls_aes_free(&aes);
    return false;
  }
  
  size_t nc_off = 0;
  unsigned char nonce_counter[16];
  unsigned char stream_block[16];
  
  memset(nonce_counter, 0, 16);
  nonce_counter[0] = nonce & 0xFF;
  nonce_counter[1] = (nonce >> 8) & 0xFF;
  
  ret = mbedtls_aes_crypt_ctr(&aes, encrypted_len, &nc_off, nonce_counter, stream_block, 
                              encrypted_payload, decryptedData);
  
  mbedtls_aes_free(&aes);
  
  return ret == 0;
}

uint32_t VictronBLE::extractBits(const uint8_t* data, int startBit, int numBits) {
  uint32_t result = 0;
  int currentBit = startBit;
  
  for (int i = 0; i < numBits; i++) {
    int byteIndex = currentBit / 8;
    int bitIndex = currentBit % 8;
    
    if (data[byteIndex] & (1 << bitIndex)) {
      result |= (1 << i);
    }
    currentBit++;
  }
  
  return result;
}

int32_t VictronBLE::signExtend(uint32_t value, int bits) {
  if (value & (1 << (bits - 1))) {
    return value | (0xFFFFFFFF << bits);
  }
  return value;
}

void VictronBLE::parseSmartShuntData(const uint8_t* data, size_t len, int8_t rssi) {
  if (len < 8) return;
  
  BatteryData batteryData;
  memset(&batteryData, 0, sizeof(batteryData)); // Initialize all fields
  batteryData.rssi = rssi;
  batteryData.data_valid = true;
  
  // Time To Go
  uint32_t ttg_raw = extractBits(data, 0, 16);
  if (ttg_raw != 0xFFFF) {
    batteryData.ttg_minutes = ttg_raw;
  }
  
  // Battery Voltage
  uint32_t voltage_raw = extractBits(data, 16, 16);
  if (voltage_raw != 0x7FFF) {
    int16_t voltage_signed = signExtend(voltage_raw, 16);
    batteryData.voltage = voltage_signed * 0.01f;
  }
  
  // Alarm Reason
  uint32_t alarm_raw = extractBits(data, 32, 16);
  batteryData.alarms = alarm_raw;
  
  // Aux Input
  uint32_t aux_raw = extractBits(data, 48, 16);
  uint32_t aux_input = extractBits(data, 64, 2);
  batteryData.aux_type = aux_input;
  
  if (aux_raw != 0x7FFF && aux_input != 1) {
    int16_t aux_signed = signExtend(aux_raw, 16);
    switch (aux_input) {
      case 0: // Voltage
        batteryData.aux_value = aux_signed * 0.01f;
        break;
      case 2: // Temperature
        batteryData.aux_value = aux_signed * 0.01f - 273.15f;
        break;
      case 3: // Mid-point
        batteryData.aux_value = aux_signed * 0.01f;
        break;
    }
  }
  
  // Battery Current
  uint32_t current_raw = extractBits(data, 66, 22);
  if (current_raw != 0x3FFFFF) {
    int32_t current_signed = signExtend(current_raw, 22);
    batteryData.current = current_signed * 0.001f;
    
    // Calculate power
    if (batteryData.voltage != 0.0f) {
      batteryData.power = batteryData.voltage * batteryData.current;
    }
  }
  
  if (len >= 16) {
    // Consumed Ah
    uint32_t consumed_raw = extractBits(data, 88, 20);
    if (consumed_raw != 0xFFFFF) {
      int32_t consumed_signed = signExtend(consumed_raw, 20);
      batteryData.consumed_ah = -consumed_signed * 0.1f;
    }
    
    // SOC
    uint32_t soc_raw = extractBits(data, 108, 10);
    if (soc_raw != 0x3FF) {
      batteryData.soc = soc_raw * 0.1f;
    }
  }
  
  if (display) {
    display->updateData(batteryData);
  }
  
  calculateBatteryTime(batteryData);
  
  Serial.printf("[%s] V:%.2f I:%.3f P:%.1f SOC:%.1f%% RSSI:%d\n",
                targetAddress.toString().c_str(),
                batteryData.voltage, batteryData.current, batteryData.power,
                batteryData.soc, batteryData.rssi);
}

void VictronBLE::calculateBatteryTime(BatteryData& batteryData) {
  batteryData.calculated_time_remaining_minutes = 0;
  batteryData.calculated_time_to_full_minutes = 0;
  batteryData.time_calculation_valid = false;
  
  if (!batteryData.data_valid || batteryData.soc <= 0.0f) {
    return;
  }
  
  float current_abs = abs(batteryData.current);
  
  if (current_abs < MIN_CURRENT_THRESHOLD) {
    return;
  }
  
  if (batteryData.current < -MIN_CURRENT_THRESHOLD) {
    float remaining_capacity_ah = (batteryData.soc / 100.0f) * BATTERY_CAPACITY_AH;
    float time_hours = remaining_capacity_ah / current_abs;
    batteryData.calculated_time_remaining_minutes = (uint16_t)(time_hours * 60.0f);
    batteryData.time_calculation_valid = true;
    
    Serial.printf("Discharging: %.1fAh remaining, %.1fA draw, %.1fh remaining\n",
                  remaining_capacity_ah, current_abs, time_hours);
                  
  } else if (batteryData.current > MIN_CURRENT_THRESHOLD) {
    float current_capacity_ah = (batteryData.soc / 100.0f) * BATTERY_CAPACITY_AH;
    float capacity_needed_ah = BATTERY_CAPACITY_AH - current_capacity_ah;
    float time_hours = capacity_needed_ah / current_abs;
    batteryData.calculated_time_to_full_minutes = (uint16_t)(time_hours * 60.0f);
    batteryData.time_calculation_valid = true;
    
    Serial.printf("Charging: %.1fAh needed, %.1fA charge, %.1fh to full\n",
                  capacity_needed_ah, current_abs, time_hours);
  }
  
  if (display) {
    display->updateData(batteryData);
  }
}

// Callback implementation
void VictronAdvertisingCallback::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
  victronBLE->handleAdvertisement(advertisedDevice);
}