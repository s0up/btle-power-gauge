#include <Arduino.h>
#include "NimBLEDevice.h"
#include "config.h"
#include <mbedtls/aes.h>

// Victron BLE Advertising Protocol Implementation
// Based on: https://communityarchive.victronenergy.com/questions/187303/victron-bluetooth-advertising-protocol.html

// Convert hex string to bytes
void hexStringToBytes(const char* hexString, uint8_t* byteArray, size_t byteArraySize) {
  size_t hexLen = strlen(hexString);
  for (size_t i = 0; i < byteArraySize && i * 2 < hexLen; i++) {
    char hexByte[3] = {hexString[i * 2], hexString[i * 2 + 1], '\0'};
    byteArray[i] = (uint8_t)strtol(hexByte, NULL, 16);
  }
}

// Convert MAC address string to NimBLEAddress
NimBLEAddress macStringToAddress(const char* macString) {
  // Convert "d6ec4c9e6307" to "d6:ec:4c:9e:63:07" format
  char formattedMac[18];
  snprintf(formattedMac, sizeof(formattedMac), "%c%c:%c%c:%c%c:%c%c:%c%c:%c%c",
           macString[0], macString[1], macString[2], macString[3],
           macString[4], macString[5], macString[6], macString[7],
           macString[8], macString[9], macString[10], macString[11]);
  return NimBLEAddress(formattedMac);
}

// Decrypt Victron advertising data
bool decryptVictronData(const uint8_t* encryptedData, size_t dataLen, 
                       const uint8_t* key, uint8_t* decryptedData) {
  if (dataLen < 8) return false; // Need at least full header
  
  // Parse BLE advertisement structure according to Jake Baldwin's analysis:
  // Byte [0]: 0x10 (Manufacturer Data Record type)
  // Byte [1-2]: Model ID 
  // Byte [3]: Read out type
  // Byte [4]: Record type (0x02 for Battery Monitor)
  // Byte [5-6]: Nonce/Data Counter (little-endian)
  // Byte [7]: First byte of encryption key
  // Byte [8+]: Encrypted data
  
  if (encryptedData[0] != 0x10) {
    Serial.printf("⚠️  Invalid manufacturer data record type: 0x%02X (expected 0x10)\n", encryptedData[0]);
    return false;
  }
  
  uint16_t model_id = encryptedData[1] | (encryptedData[2] << 8);
  uint8_t readout_type = encryptedData[3];
  uint8_t record_type = encryptedData[4];
  
  // Extract the nonce from bytes 5-6 (little-endian)
  uint16_t nonce = encryptedData[5] | (encryptedData[6] << 8);
  
  // Validate key byte (byte 7 should match first byte of our key)
  uint8_t key_byte = encryptedData[7];
  if (key_byte != key[0]) {
    Serial.printf("⚠️  Key validation failed. Expected: 0x%02X, Got: 0x%02X\n", key[0], key_byte);
    Serial.printf("💡 Device expects key starting with: 0x%02X\n", key_byte);
    Serial.println("💡 Please update INSTANT_READOUT_ENCRYPTION_KEY in config.h");
    Serial.println("💡 Get the correct key from VictronConnect app > Settings > Product Info > Instant Readout Details");
    return false;
  }
  
  Serial.printf("📋 Model ID: 0x%04X, Readout Type: 0x%02X, Record Type: 0x%02X\n", 
                model_id, readout_type, record_type);
  Serial.printf("🔢 Nonce: 0x%04X (%d)\n", nonce, nonce);
  
  if (record_type != 0x02) {
    Serial.printf("⚠️  Expected Battery Monitor record type (0x02), got 0x%02X\n", record_type);
    // Continue anyway, might still work
  }
  
  // The encrypted data starts at byte 8
  const uint8_t* encrypted_payload = encryptedData + 8;
  size_t encrypted_len = dataLen - 8;
  
  Serial.printf("🔐 Decrypting %d bytes of payload data\n", encrypted_len);
  
  // Setup AES-CTR decryption
  mbedtls_aes_context aes;
  mbedtls_aes_init(&aes);
  
  int ret = mbedtls_aes_setkey_enc(&aes, key, 128); // Note: CTR uses encryption key setup
  if (ret != 0) {
    Serial.printf("⚠️  AES key setup failed: %d\n", ret);
    mbedtls_aes_free(&aes);
    return false;
  }
  
  // Prepare CTR parameters - CRITICAL: nonce must be in little-endian format!
  size_t nc_off = 0;
  unsigned char nonce_counter[16];
  unsigned char stream_block[16];
  
  // Initialize nonce_counter block: nonce (2 bytes little-endian) + zeros
  // According to Jake Baldwin: "must also be used in the decryption as a little endian value"
  memset(nonce_counter, 0, 16);
  nonce_counter[0] = nonce & 0xFF;        // Nonce low byte (little-endian)
  nonce_counter[1] = (nonce >> 8) & 0xFF; // Nonce high byte (little-endian)
  // Bytes 2-15 remain zero (counter starts at 0)
  
  Serial.printf("🔢 Counter block: %02X %02X 00 00 00 00 00 00 00 00 00 00 00 00 00 00\n", 
                nonce_counter[0], nonce_counter[1]);
  
  // Decrypt using AES-CTR (only the encrypted payload, not the header)
  ret = mbedtls_aes_crypt_ctr(&aes, encrypted_len, &nc_off, nonce_counter, stream_block, 
                              encrypted_payload, decryptedData);
  
  mbedtls_aes_free(&aes);
  
  if (ret != 0) {
    Serial.printf("⚠️  AES-CTR decryption failed: %d\n", ret);
    return false;
  }
  
  Serial.printf("✅ Successfully decrypted %d bytes\n", encrypted_len);
  
  return true;
}

// Helper function to extract bits from a byte array
uint32_t extractBits(const uint8_t* data, int startBit, int numBits) {
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

// Sign extend a value from specified bit width to 32-bit signed
int32_t signExtend(uint32_t value, int bits) {
  if (value & (1 << (bits - 1))) {
    // Negative number - sign extend
    return value | (0xFFFFFFFF << bits);
  }
  return value;
}

// Parse SmartShunt data from decrypted payload
void parseSmartShuntData(const uint8_t* data, size_t len) {
  if (len < 8) {
    Serial.println("⚠️  Payload too short for SmartShunt data (need at least 8 bytes)");
    return;
  }
  
  Serial.println("\n=== SmartShunt Data ===");
  
  // Print raw decrypted data for debugging
  Serial.print("📊 Raw decrypted data: ");
  for (size_t i = 0; i < len; i++) {
    Serial.printf("%02X ", data[i]);
  }
  Serial.println();
  
  // Parse according to official Victron Battery Monitor (0x02) format
  // Reference: https://communityarchive.victronenergy.com/storage/attachments/extra-manufacturer-data-2022-12-14.pdf
  // The decrypted data now contains only the battery data (no header)
  // Bit positions are now relative to the start of the decrypted data
  
  // Declare variables for all data points (so they can be used in summary)
  uint32_t ttg_raw, voltage_raw, alarm_raw, aux_raw, aux_input, current_raw, consumed_raw, soc_raw;
  
  // TTG (Time To Go): bits 0-15 (16 bits), unsigned, scale 1 min
  ttg_raw = extractBits(data, 0, 16);
  if (ttg_raw != 0xFFFF) {
    Serial.printf("⏰ Time to Go: %d minutes (%.1f hours)\n", ttg_raw, ttg_raw / 60.0f);
  } else {
    Serial.println("⏰ Time to Go: N/A");
  }
  
  // Battery Voltage: bits 16-31 (16 bits), signed, scale 0.01V
  voltage_raw = extractBits(data, 16, 16);
  if (voltage_raw != 0x7FFF) {
    int16_t voltage_signed = signExtend(voltage_raw, 16);
    float voltage = voltage_signed * 0.01f;
    Serial.printf("🔋 Battery Voltage: %.2f V\n", voltage);
  } else {
    Serial.println("🔋 Battery Voltage: N/A");
  }
  
  // Alarm Reason: bits 32-47 (16 bits), unsigned
  alarm_raw = extractBits(data, 32, 16);
  if (alarm_raw != 0x0000) {
    Serial.printf("⚠️  Alarm Reason: 0x%04X", alarm_raw);
    
    // Decode common alarm bits (based on Victron documentation)
    if (alarm_raw & 0x0001) Serial.print(" [Low Voltage]");
    if (alarm_raw & 0x0002) Serial.print(" [High Voltage]");
    if (alarm_raw & 0x0004) Serial.print(" [Low SOC]");
    if (alarm_raw & 0x0008) Serial.print(" [Low Temperature]");
    if (alarm_raw & 0x0010) Serial.print(" [High Temperature]");
    if (alarm_raw & 0x0020) Serial.print(" [Mid Voltage]");
    if (alarm_raw & 0x0040) Serial.print(" [Overload]");
    if (alarm_raw & 0x0080) Serial.print(" [DC-ripple]");
    if (alarm_raw & 0x0100) Serial.print(" [Low V AC out]");
    if (alarm_raw & 0x0200) Serial.print(" [High V AC out]");
    Serial.println();
  }
  
  // Aux Voltage/Temperature/Mid-point: bits 48-63 (16 bits), signed
  aux_raw = extractBits(data, 48, 16);
  
  // Aux Input Type: bits 64-65 (2 bits)
  aux_input = extractBits(data, 64, 2);
  const char* aux_types[] = {"Voltage", "Reserved", "Temperature", "Mid-point"};
  
  Serial.printf("🔧 Aux Input: %s", aux_types[aux_input]);
  
  if (aux_raw != 0x7FFF && aux_input != 1) { // Skip "Reserved" type
    int16_t aux_signed = signExtend(aux_raw, 16);
    if (aux_input == 0) { // Aux Voltage
      float aux_voltage = aux_signed * 0.01f;
      Serial.printf(" (%.2f V)", aux_voltage);
    } else if (aux_input == 2) { // Temperature
      float aux_temp = aux_signed * 0.01f - 273.15f; // Convert from Kelvin to Celsius
      Serial.printf(" (%.1f°C)", aux_temp);
    } else if (aux_input == 3) { // Mid-point voltage
      float mid_voltage = aux_signed * 0.01f;
      Serial.printf(" (%.2f V)", mid_voltage);
      // Calculate mid-point deviation
      if (voltage_raw != 0x7FFF) {
        int16_t battery_voltage_signed = signExtend(voltage_raw, 16);
        float battery_voltage = battery_voltage_signed * 0.01f;
        float expected_mid = battery_voltage / 2.0f;
        float deviation = mid_voltage - expected_mid;
        Serial.printf(" [Deviation: %+.2f V]", deviation);
      }
    }
  }
  Serial.println();
  
  // Battery Current: bits 66-87 (22 bits), signed, scale 0.001A
  current_raw = extractBits(data, 66, 22);
  float current = 0.0f;
  float voltage = 0.0f;
  
  if (current_raw != 0x3FFFFF) {
    int32_t current_signed = signExtend(current_raw, 22);
    current = current_signed * 0.001f;
    
    // Get voltage for power calculation
    if (voltage_raw != 0x7FFF) {
      int16_t voltage_signed = signExtend(voltage_raw, 16);
      voltage = voltage_signed * 0.01f;
    }
    
    Serial.printf("⚡ Battery Current: %.3f A", current);
    if (current > 0) {
      Serial.print(" (Charging)");
    } else if (current < 0) {
      Serial.print(" (Discharging)");
    }
    
    // Calculate and display power
    if (voltage != 0.0f) {
      float power = voltage * current;
      Serial.printf(" | Power: %.1f W", power);
    }
    Serial.println();
  } else {
    Serial.println("⚡ Battery Current: N/A");
  }
  
  // Check if we have enough data for SOC and Consumed Ah
  if (len >= 16) {
    // Consumed Ah: bits 88-107 (20 bits), signed, scale 0.1Ah
    uint32_t consumed_raw = extractBits(data, 88, 20);
    if (consumed_raw != 0xFFFFF) {
      int32_t consumed_signed = signExtend(consumed_raw, 20);
      // According to the spec: Consumed Ah = -Record value
      float consumed_ah = -consumed_signed * 0.1f;
      Serial.printf("📊 Consumed Ah: %.1f Ah", consumed_ah);
      
      // Show if battery is net charging or discharging since last sync
      if (consumed_ah > 0) {
        Serial.print(" (Net consumed since sync)");
      } else if (consumed_ah < 0) {
        Serial.print(" (Net charged since sync)");
      }
      Serial.println();
    } else {
      Serial.println("📊 Consumed Ah: N/A");
    }
    
    // SOC: bits 108-117 (10 bits), unsigned, scale 0.1%
    uint32_t soc_raw = extractBits(data, 108, 10);
    if (soc_raw != 0x3FF) {
      float soc = soc_raw * 0.1f;
      Serial.printf("📊 State of Charge: %.1f%%", soc);
      
      // Add SOC status indicators
      if (soc >= 90.0f) {
        Serial.print(" [Excellent]");
      } else if (soc >= 70.0f) {
        Serial.print(" [Good]");
      } else if (soc >= 50.0f) {
        Serial.print(" [Fair]");
      } else if (soc >= 20.0f) {
        Serial.print(" [Low]");
      } else {
        Serial.print(" [Critical]");
      }
      
      // Estimate remaining capacity if we know the battery capacity
      // Note: This would need to be configured based on your actual battery
      // For now, just show the calculation method
      Serial.printf(" | Remaining: %.0f%% of capacity", soc);
      Serial.println();
    } else {
      Serial.println("📊 State of Charge: N/A");
    }
  }
  
  
  // Add summary section with key insights
  Serial.println("📈 === BATTERY SUMMARY ===");
  
  if (voltage_raw != 0x7FFF && current_raw != 0x3FFFFF && soc_raw != 0x3FF) {
    int16_t voltage_signed = signExtend(voltage_raw, 16);
    int32_t current_signed = signExtend(current_raw, 22);
    float voltage = voltage_signed * 0.01f;
    float current = current_signed * 0.001f;
    float soc = soc_raw * 0.1f;
    float power = voltage * current;
    
    Serial.printf("🔋 System: %.1fV @ %.3fA = %.1fW", voltage, current, power);
    
    if (current > 0.1f) {
      Serial.printf(" (CHARGING at %.1fW)", power);
    } else if (current < -0.1f) {
      Serial.printf(" (DISCHARGING at %.1fW)", -power);
    } else {
      Serial.print(" (IDLE/FLOAT)");
    }
    Serial.println();
    
    // Battery health indicator based on voltage and SOC correlation
    float expected_voltage_per_cell = 0.0f;
    int estimated_cells = 0;
    
    if (voltage >= 48.0f && voltage <= 58.4f) {
      estimated_cells = 16; // 16S LiFePO4 (51.2V nominal)
      expected_voltage_per_cell = voltage / 16.0f;
    } else if (voltage >= 24.0f && voltage <= 29.2f) {
      estimated_cells = 8; // 8S LiFePO4 (25.6V nominal)
      expected_voltage_per_cell = voltage / 8.0f;
    } else if (voltage >= 12.0f && voltage <= 14.6f) {
      estimated_cells = 4; // 4S LiFePO4 (12.8V nominal)
      expected_voltage_per_cell = voltage / 4.0f;
    }
    
    if (estimated_cells > 0) {
      Serial.printf("🔬 Estimated: %dS battery, %.3fV per cell", estimated_cells, expected_voltage_per_cell);
      
      if (expected_voltage_per_cell >= 3.4f) {
        Serial.print(" [High]");
      } else if (expected_voltage_per_cell >= 3.2f) {
        Serial.print(" [Normal]");
      } else if (expected_voltage_per_cell >= 3.0f) {
        Serial.print(" [Low]");
      } else {
        Serial.print(" [Very Low]");
      }
      Serial.println();
    }
  }
  
  Serial.println("=====================");
}

// Scan callback for Victron advertising data
class VictronAdvertisingCallback: public NimBLEAdvertisedDeviceCallbacks {
private:
  NimBLEAddress targetAddress;
  uint8_t encryptionKey[16];
  
public:
  VictronAdvertisingCallback() {
    targetAddress = macStringToAddress(INSTANT_READOUT_MAC_ADDRESS);
    hexStringToBytes(INSTANT_READOUT_ENCRYPTION_KEY, encryptionKey, 16);
    
    Serial.printf("🎯 Monitoring device: %s\n", targetAddress.toString().c_str());
    Serial.print("🔑 Using encryption key: ");
    for (int i = 0; i < 16; i++) {
      Serial.printf("%02X", encryptionKey[i]);
    }
    Serial.println();
  }
  
  void onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    // Check if this is our target device
    if (advertisedDevice->getAddress() != targetAddress) {
    return;
  }
  
    Serial.printf("\n📡 Advertisement from %s (RSSI: %d dBm)\n", 
                  advertisedDevice->getAddress().toString().c_str(),
                  advertisedDevice->getRSSI());
    
    // Look for manufacturer data (Victron uses manufacturer ID)
    if (advertisedDevice->haveManufacturerData()) {
      std::string manufacturerData = advertisedDevice->getManufacturerData();
      
      Serial.printf("🏭 Manufacturer data length: %d bytes\n", manufacturerData.length());
      
      if (manufacturerData.length() >= 2) {
        // Get manufacturer ID (first 2 bytes, little-endian)
        uint16_t manufacturerId = (uint8_t)manufacturerData[0] | 
                                 ((uint8_t)manufacturerData[1] << 8);
        
        Serial.printf("🏭 Manufacturer ID: 0x%04X\n", manufacturerId);
        
        // Victron manufacturer ID is typically 0x02E1 (737 decimal)
        if (manufacturerId == 0x02E1 && manufacturerData.length() > 2) {
          Serial.println("✅ Victron manufacturer data found!");
          
          // Extract encrypted payload (skip manufacturer ID)
          const uint8_t* encryptedPayload = (const uint8_t*)manufacturerData.data() + 2;
          size_t payloadLen = manufacturerData.length() - 2;
          
          Serial.printf("🔐 Encrypted payload length: %d bytes\n", payloadLen);
          Serial.print("🔐 Encrypted data: ");
          for (size_t i = 0; i < payloadLen; i++) {
            Serial.printf("%02X ", encryptedPayload[i]);
          }
          Serial.println();
          
          // Decrypt the payload
          uint8_t decryptedData[64]; // Buffer for decrypted data
          if (decryptVictronData(encryptedPayload, payloadLen, encryptionKey, decryptedData)) {
            Serial.println("✅ Decryption successful!");
            parseSmartShuntData(decryptedData, payloadLen);
          } else {
            Serial.println("❌ Decryption failed!");
          }
        } else {
          Serial.printf("⚠️  Not Victron data (ID: 0x%04X)\n", manufacturerId);
        }
      }
    } else {
      Serial.println("⚠️  No manufacturer data in advertisement");
    }
    
    // Also check service data if available
    if (advertisedDevice->haveServiceData()) {
      Serial.println("📋 Device has service data (checking...)");
      // Service data parsing would go here if needed
    }
  }
};

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== Victron SmartShunt BLE Advertising Reader v1.0 ===");
  Serial.printf("Target device MAC: %s\n", INSTANT_READOUT_MAC_ADDRESS);
  Serial.printf("Encryption key: %s\n", INSTANT_READOUT_ENCRYPTION_KEY);
  Serial.println("📡 Using BLE advertising protocol (recommended by Victron)");
  
  // Initialize BLE
  Serial.println("🔧 Initializing BLE...");
  NimBLEDevice::init("Victron_Reader");
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);
  
  // Setup scanner for passive listening
  NimBLEScan* pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new VictronAdvertisingCallback());
  pBLEScan->setInterval(100);  // 100ms interval
  pBLEScan->setWindow(50);     // 50ms window
  pBLEScan->setActiveScan(false); // Passive scanning for advertising data
  pBLEScan->setDuplicateFilter(false); // Allow duplicate advertisements
  
  Serial.println("✅ BLE initialized successfully");
  Serial.println("🔍 Starting continuous scan for Victron advertising data...");
  Serial.println("💡 This method is passive and doesn't require pairing/authentication");
  
  // Start continuous scanning
  pBLEScan->start(0, nullptr, false); // Scan indefinitely
}

void loop() {
  static unsigned long lastStatusPrint = 0;
  unsigned long now = millis();
  
  // Print status every 30 seconds
  if (now - lastStatusPrint > 30000) {
    Serial.printf("📊 Status: Scanning for advertisements | Uptime: %lu ms\n", now);
    Serial.println("💡 Waiting for Victron SmartShunt advertising data...");
    lastStatusPrint = now;
  }
  
  // Check for serial input commands
  if (Serial.available()) {
    char cmd = Serial.read();
    if (cmd == 's' || cmd == 'S') {
      Serial.println("📊 Scan status:");
      NimBLEScan* pScan = NimBLEDevice::getScan();
      Serial.printf("   Scanning: %s\n", pScan->isScanning() ? "YES" : "NO");
      Serial.printf("   Target MAC: %s\n", INSTANT_READOUT_MAC_ADDRESS);
      Serial.printf("   Encryption key: %s\n", INSTANT_READOUT_ENCRYPTION_KEY);
    }
    // Clear any remaining input
    while (Serial.available()) Serial.read();
  }
  
  delay(100);
}