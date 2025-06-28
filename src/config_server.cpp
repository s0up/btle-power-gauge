#include "config_server.h"

ConfigServer::ConfigServer() : server(CONFIG_SERVER_PORT), isConfigMode(false) {
  memset(&currentConfig, 0, sizeof(currentConfig));
}

bool ConfigServer::begin() {
  if (!prefs.begin("device-config", false)) {
    Serial.println("Failed to init preferences");
    return false;
  }
  
  loadConfig();
  return true;
}

bool ConfigServer::loadConfig() {
  size_t macLen = prefs.getString("mac", "").length();
  size_t keyLen = prefs.getString("key", "").length();
  
  if (macLen == 12 && keyLen == 32) {
    String mac = prefs.getString("mac", "");
    String key = prefs.getString("key", "");
    
    strcpy(currentConfig.mac_address, mac.c_str());
    strcpy(currentConfig.encryption_key, key.c_str());
    currentConfig.valid = true;
    
    Serial.printf("Loaded config - MAC: %s\n", currentConfig.mac_address);
    return true;
  } else {
    // Use defaults from config.h if no valid stored config
    strcpy(currentConfig.mac_address, INSTANT_READOUT_MAC_ADDRESS);
    strcpy(currentConfig.encryption_key, INSTANT_READOUT_ENCRYPTION_KEY);
    currentConfig.valid = true;
    
    Serial.println("Using default configuration");
    return true;
  }
}

bool ConfigServer::saveConfig(const char* macAddress, const char* encryptionKey) {
  if (strlen(macAddress) != 12 || strlen(encryptionKey) != 32) {
    Serial.println("Invalid config format");
    return false;
  }
  
  prefs.putString("mac", macAddress);
  prefs.putString("key", encryptionKey);
  
  strcpy(currentConfig.mac_address, macAddress);
  strcpy(currentConfig.encryption_key, encryptionKey);
  currentConfig.valid = true;
  
  Serial.printf("Saved config - MAC: %s\n", macAddress);
  return true;
}

void ConfigServer::resetConfig() {
  prefs.clear();
  strcpy(currentConfig.mac_address, INSTANT_READOUT_MAC_ADDRESS);
  strcpy(currentConfig.encryption_key, INSTANT_READOUT_ENCRYPTION_KEY);
  currentConfig.valid = true;
  Serial.println("Configuration reset to defaults");
}

bool ConfigServer::startConfigMode() {
  if (isConfigMode) return true;
  
  Serial.println("Starting configuration mode...");
  
  WiFi.mode(WIFI_AP);
  if (!WiFi.softAP(CONFIG_AP_SSID, CONFIG_AP_PASSWORD)) {
    Serial.println("Failed to start AP");
    return false;
  }
  
  IPAddress IP = WiFi.softAPIP();
  Serial.printf("AP started: %s / %s\n", CONFIG_AP_SSID, CONFIG_AP_PASSWORD);
  Serial.printf("Config portal: http://%s\n", IP.toString().c_str());
  
  server.on("/", [this]() { handleRoot(); });
  server.on("/save", HTTP_POST, [this]() { handleSave(); });
  server.on("/status", [this]() { handleStatus(); });
  server.begin();
  
  configStartTime = millis();
  isConfigMode = true;
  
  return true;
}

void ConfigServer::stopConfigMode() {
  if (!isConfigMode) return;
  
  server.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  isConfigMode = false;
  
  Serial.println("Configuration mode stopped");
}

void ConfigServer::loop() {
  if (!isConfigMode) return;
  
  server.handleClient();
  
  // Auto-timeout after configured time
  if (millis() - configStartTime > CONFIG_TIMEOUT_MS) {
    Serial.println("Configuration timeout");
    stopConfigMode();
  }
}

void ConfigServer::handleRoot() {
  sendConfigPage();
}

void ConfigServer::handleStatus() {
  String json = "{";
  json += "\"mac\":\"" + String(currentConfig.mac_address) + "\",";
  json += "\"key\":\"" + String(currentConfig.encryption_key) + "\",";
  json += "\"valid\":" + String(currentConfig.valid ? "true" : "false");
  json += "}";
  
  server.send(200, "application/json", json);
}

void ConfigServer::handleSave() {
  String mac = server.arg("mac");
  String key = server.arg("key");
  
  // Remove any spaces or colons from MAC
  mac.replace(":", "");
  mac.replace(" ", "");
  mac.toLowerCase();
  
  // Remove any spaces from key
  key.replace(" ", "");
  key.toLowerCase();
  
  // Validate MAC (12 hex chars)
  bool macValid = (mac.length() == 12);
  for (int i = 0; i < mac.length() && macValid; i++) {
    char c = mac.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      macValid = false;
    }
  }
  
  // Validate encryption key (32 hex chars)
  bool keyValid = (key.length() == 32);
  for (int i = 0; i < key.length() && keyValid; i++) {
    char c = key.charAt(i);
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      keyValid = false;
    }
  }
  
  if (macValid && keyValid) {
    if (saveConfig(mac.c_str(), key.c_str())) {
      String response = "<!DOCTYPE html><html><head><title>BTLE Power Gauge Config</title>";
      response += "<meta charset='UTF-8'>";
      response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
      response += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
      response += ".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:500px;margin:0 auto}";
      response += ".success{color:#4CAF50;text-align:center;font-size:18px;margin-bottom:20px}";
      response += ".info{background:#e3f2fd;padding:15px;border-radius:5px;margin:15px 0}";
      response += "</style></head><body>";
      response += "<div class='container'>";
      response += "<h1>Configuration Saved!</h1>";
      response += "<div class='success'>&check; Device configuration updated successfully</div>";
      response += "<div class='info'>";
      response += "<strong>MAC Address:</strong> " + mac + "<br>";
      response += "<strong>Encryption Key:</strong> " + key.substring(0, 8) + "...";
      response += "</div>";
             response += "<p>The device will restart and connect to your device.</p>";
      response += "<p>Configuration portal will close automatically in 10 seconds.</p>";
      response += "<script>setTimeout(function(){window.close();}, 10000);</script>";
      response += "</div></body></html>";
      
      server.send(200, "text/html", response);
      
      // Schedule config mode stop
      configStartTime = millis() - CONFIG_TIMEOUT_MS + 10000; // 10 seconds
    } else {
      server.send(500, "text/plain", "Failed to save configuration");
    }
  } else {
         String response = "<!DOCTYPE html><html><head><title>Configuration Error</title>";
     response += "<meta charset='UTF-8'>";
     response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    response += "<style>body{font-family:Arial;margin:40px;background:#f0f0f0}";
    response += ".container{background:white;padding:30px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1);max-width:500px;margin:0 auto}";
    response += ".error{color:#f44336;text-align:center;font-size:18px;margin-bottom:20px}";
    response += "</style></head><body>";
    response += "<div class='container'>";
    response += "<h1>Configuration Error</h1>";
    response += "<div class='error'>&times; Invalid format</div>";
    response += "<p><strong>Requirements:</strong></p><ul>";
    response += "<li>MAC Address: Exactly 12 hexadecimal characters</li>";
    response += "<li>Encryption Key: Exactly 32 hexadecimal characters</li>";
    response += "</ul>";
    response += "<a href='/'>← Go back and try again</a>";
    response += "</div></body></html>";
    
    server.send(400, "text/html", response);
  }
}

void ConfigServer::sendConfigPage() {
  String html = "<!DOCTYPE html><html><head><title>BTLE Power Gauge Configuration</title>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; margin: 0; padding: 20px; background: #f0f0f0; }";
  html += ".container { background: white; padding: 30px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); max-width: 600px; margin: 0 auto; }";
  html += "h1 { color: #333; text-align: center; margin-bottom: 30px; }";
  html += ".form-group { margin-bottom: 20px; }";
  html += "label { display: block; margin-bottom: 5px; font-weight: bold; color: #555; }";
  html += "input[type='text'] { width: 100%; padding: 12px; border: 2px solid #ddd; border-radius: 5px; font-size: 16px; box-sizing: border-box; }";
  html += "input[type='text']:focus { border-color: #2196F3; outline: none; }";
  html += ".btn { background: #2196F3; color: white; padding: 12px 30px; border: none; border-radius: 5px; font-size: 16px; cursor: pointer; width: 100%; margin-top: 10px; }";
  html += ".btn:hover { background: #1976D2; }";
  html += ".info { background: #e3f2fd; padding: 15px; border-radius: 5px; margin: 20px 0; }";
  html += ".current { background: #f1f8e9; padding: 15px; border-radius: 5px; margin: 20px 0; }";
  html += ".help { font-size: 14px; color: #666; margin-top: 5px; }";
  html += "</style></head><body>";
  
  html += "<div class='container'>";
  html += "<h1>BTLE Power Gauge</h1>";
  html += "<h2>Device Configuration</h2>";
  
  html += "<div class='current'>";
  html += "<h3>Current Configuration:</h3>";
  html += "<strong>MAC:</strong> " + String(currentConfig.mac_address) + "<br>";
  html += "<strong>Key:</strong> " + String(currentConfig.encryption_key).substring(0, 8) + "..." + String(currentConfig.encryption_key).substring(24);
  html += "</div>";
  
  html += "<form method='POST' action='/save'>";
  
  html += "<div class='form-group'>";
  html += "<label for='mac'>Device MAC Address:</label>";
  html += "<input type='text' id='mac' name='mac' value='" + String(currentConfig.mac_address) + "' maxlength='17' placeholder='d6ec4c9e6307'>";
  html += "<div class='help'>Enter MAC address (12 hex characters, no separators)</div>";
  html += "</div>";
  
  html += "<div class='form-group'>";
  html += "<label for='key'>Encryption Key:</label>";
  html += "<input type='text' id='key' name='key' value='" + String(currentConfig.encryption_key) + "' maxlength='32' placeholder='64cd146fe6771ef40610ecf50f3bb06a'>";
  html += "<div class='help'>32-character hexadecimal encryption key</div>";
  html += "</div>";
  
  html += "<button type='submit' class='btn'>Save Configuration</button>";
  html += "</form>";
  
  html += "<div class='info'>";
  html += "<h3>How to find these values:</h3>";
  html += "<p><strong>Using Device App:</strong></p>";
  html += "<ol>";
  html += "<li>Open your device management app and connect to the device</li>";
  html += "<li>Go to Settings &rarr; Product Info</li>";
  html += "<li><strong>MAC Address:</strong> Note the Bluetooth address (remove colons)</li>";
  html += "<li>Go to Settings &rarr; Instant Readout or BLE Settings</li>";
  html += "<li><strong>Encryption Key:</strong> Copy the key shown</li>";
  html += "</ol>";
  html += "</div>";
  
  html += "</div></body></html>";
  
  server.send(200, "text/html", html);
} 