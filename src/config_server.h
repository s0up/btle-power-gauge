#ifndef CONFIG_SERVER_H
#define CONFIG_SERVER_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "config.h"

struct DeviceConfig {
  char mac_address[13];     // 12 hex chars + null terminator
  char encryption_key[33];  // 32 hex chars + null terminator
  bool valid;
};

class ConfigServer {
private:
  WebServer server;
  Preferences prefs;
  DeviceConfig currentConfig;
  unsigned long configStartTime;
  bool isConfigMode;
  
  void handleRoot();
  void handleSave();
  void handleStatus();
  void sendConfigPage();
  
public:
  ConfigServer();
  bool begin();
  void loop();
  bool startConfigMode();
  void stopConfigMode();
  bool isInConfigMode() const { return isConfigMode; }
  
  // Configuration management
  bool loadConfig();
  bool saveConfig(const char* macAddress, const char* encryptionKey);
  DeviceConfig getConfig() const { return currentConfig; }
  bool hasValidConfig() const { return currentConfig.valid; }
  
  // Reset to defaults
  void resetConfig();
};

#endif // CONFIG_SERVER_H 