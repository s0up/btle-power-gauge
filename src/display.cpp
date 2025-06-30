#include "display.h"
#include "config.h"

Display::Display() : display(GxEPD2_290_T94_V2(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY)) {
  memset(&currentData, 0, sizeof(currentData));
  memset(&lastDisplayedData, 0, sizeof(lastDisplayedData));
}

bool Display::begin() {
  Serial.println("Initializing display...");
  
  // Power on the e-ink display
  pinMode(EPD_POWER, OUTPUT);
  digitalWrite(EPD_POWER, HIGH);
  delay(100);
  
  // Initialize SPI and display
  SPI.begin(EPD_SCK, -1, EPD_MOSI, EPD_CS);
  display.init(115200, true, 2, false);
  
  // Show initial screen
  showTestScreen();
  
  Serial.println("Display ready");
  return true;
}

void Display::updateData(const BatteryData& data) {
  BatteryData previousData = currentData;
  currentData = data;
  currentData.last_update = millis();
  
  if (hasSignificantChange(currentData, lastDisplayedData)) {
    screenNeedsUpdate = true;
    Serial.printf("Display: Significant change detected - scheduling update\n");
  }
  
  Serial.printf("Display: Received data - V:%.2f, Valid:%d, NeedsUpdate:%d\n", 
                currentData.voltage, currentData.data_valid, screenNeedsUpdate);
}

bool Display::hasSignificantChange(const BatteryData& newData, const BatteryData& oldData) {
  if (newData.data_valid != oldData.data_valid) {
    Serial.println("Change: Data validity");
    return true;
  }
  
  if (!newData.data_valid) {
    return false;
  }
  
  if (newData.alarms != oldData.alarms) {
    Serial.println("Change: Alarms");
    return true;
  }
  
  if (abs(newData.voltage - oldData.voltage) > VOLTAGE_CHANGE_THRESHOLD) {
    Serial.printf("Change: Voltage %.2f -> %.2f\n", oldData.voltage, newData.voltage);
    return true;
  }
  
  if (abs(newData.current - oldData.current) > CURRENT_CHANGE_THRESHOLD) {
    Serial.printf("Change: Current %.2f -> %.2f\n", oldData.current, newData.current);
    return true;
  }
  
  if (abs(newData.soc - oldData.soc) > SOC_CHANGE_THRESHOLD) {
    Serial.printf("Change: SOC %.1f -> %.1f\n", oldData.soc, newData.soc);
    return true;
  }
  
  if (abs(newData.power - oldData.power) > POWER_CHANGE_THRESHOLD) {
    Serial.printf("Change: Power %.1f -> %.1f\n", oldData.power, newData.power);
    return true;
  }
  
  if (newData.time_calculation_valid != oldData.time_calculation_valid) {
    Serial.println("Change: Time calculation validity");
    return true;
  }
  
  if (newData.time_calculation_valid) {
    if (abs((int)newData.calculated_time_remaining_minutes - (int)oldData.calculated_time_remaining_minutes) > TIME_CHANGE_THRESHOLD) {
      Serial.printf("Change: Time remaining %d -> %d min\n", 
                    oldData.calculated_time_remaining_minutes, newData.calculated_time_remaining_minutes);
      return true;
    }
    if (abs((int)newData.calculated_time_to_full_minutes - (int)oldData.calculated_time_to_full_minutes) > TIME_CHANGE_THRESHOLD) {
      Serial.printf("Change: Time to full %d -> %d min\n", 
                    oldData.calculated_time_to_full_minutes, newData.calculated_time_to_full_minutes);
      return true;
    }
  }
  
  auto getSignalBars = [](int8_t rssi) -> int {
    if (rssi >= -82) return 4;
    else if (rssi >= -85) return 3;
    else if (rssi >= -89) return 2;
    else return 1;
  };
  
  if (getSignalBars(newData.rssi) != getSignalBars(oldData.rssi)) {
    Serial.printf("Change: Signal strength %d -> %d dBm\n", oldData.rssi, newData.rssi);
    return true;
  }
  
  auto getCurrentState = [](float current) -> int {
    if (current > 0.1f) return 1;  // charging
    else if (current < -0.1f) return -1;  // discharging
    else return 0;  // idle
  };
  
  if (getCurrentState(newData.current) != getCurrentState(oldData.current)) {
    Serial.println("Change: Charging/discharging state");
    return true;
  }
  
  if (abs(newData.consumed_ah - oldData.consumed_ah) > CONSUMED_AH_THRESHOLD) {
    Serial.printf("Change: Consumed Ah %.1f -> %.1f\n", oldData.consumed_ah, newData.consumed_ah);
    return true;
  }
  
  return false;
}

void Display::refresh() {
  unsigned long currentTime = millis();
  bool dataStale = !currentData.data_valid || currentTime - currentData.last_update > 60000;
  bool forcePeriodicUpdate = currentTime - lastScreenUpdate > PERIODIC_REFRESH_INTERVAL;
  bool criticalUpdate = currentData.data_valid && currentData.alarms != 0;
  
  bool shouldUpdate = screenNeedsUpdate || dataStale || forcePeriodicUpdate || criticalUpdate;
  
  if (!shouldUpdate) {
    return;
  }
  
  Serial.printf("Display refresh: needsUpdate=%d, stale=%d, periodic=%d, critical=%d\n", 
                screenNeedsUpdate, dataStale, forcePeriodicUpdate, criticalUpdate);
  
  display.setRotation(1);
  
  bool useFullUpdate = forcePeriodicUpdate || (currentTime - lastScreenUpdate > FULL_REFRESH_INTERVAL);
  
  if (useFullUpdate) {
    display.setFullWindow();
    Serial.println("Using full display update");
  } else {
    display.setPartialWindow(0, 0, 296, 128);
  }
  
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  
  if (dataStale) {
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(10, 40);
    display.print("NO DATA");
    
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 70);
    display.print("Searching...");
    
    display.setCursor(10, 95);
    display.print("Check connection");
  } else {
    display.setFont(&FreeMonoBold18pt7b);
    display.setCursor(10, 35);
    display.print(String(currentData.voltage, 1) + "V");
    
    display.setCursor(160, 35);
    display.print(String((int)currentData.soc) + "%");
    
    display.drawRect(240, 15, 40, 20, GxEPD_BLACK);
    display.drawRect(280, 20, 4, 10, GxEPD_BLACK);
    int fillWidth = (currentData.soc / 100.0) * 38;
    if (fillWidth > 0) {
      display.fillRect(241, 16, fillWidth, 18, GxEPD_BLACK);
    }
    
    display.setFont(&FreeMonoBold12pt7b);
    display.setCursor(10, 65);
    if (currentData.current >= 0) {
      display.print("+" + String(currentData.current, 1) + "A");
    } else {
      display.print(String(currentData.current, 1) + "A");
    }
    
    display.setCursor(110, 65);
    display.print(String((int)abs(currentData.power)) + "W");
    
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(220, 65);
    unsigned long timeSinceUpdate = (millis() - currentData.last_update) / 1000;
    if (timeSinceUpdate < 60) {
      display.print(String(timeSinceUpdate) + "s");
    } else if (timeSinceUpdate < 3600) {
      display.print(String(timeSinceUpdate / 60) + "m");
    } else {
      display.print(">1h");
    }
    
    display.setFont(&FreeMonoBold9pt7b);
    display.setCursor(10, 90);
    
    if (currentData.time_calculation_valid) {
      if (currentData.current < -0.1) {
        int hours = currentData.calculated_time_remaining_minutes / 60;
        int mins = currentData.calculated_time_remaining_minutes % 60;
        if (hours > 0) {
          display.print("TTG: " + String(hours) + "h" + String(mins) + "m");
        } else {
          display.print("TTG: " + String(mins) + "min");
        }
      } else if (currentData.current > 0.1) {
        int hours = currentData.calculated_time_to_full_minutes / 60;
        int mins = currentData.calculated_time_to_full_minutes % 60;
        if (hours > 0) {
          display.print("TTC: " + String(hours) + "h" + String(mins) + "m");
        } else {
          display.print("TTC: " + String(mins) + "min");
        }
      } else {
        display.print("IDLE");
      }
    } else {
      if (currentData.ttg_minutes > 0 && currentData.current < -0.1) {
        int hours = currentData.ttg_minutes / 60;
        int mins = currentData.ttg_minutes % 60;
        if (hours > 0) {
          display.print("TTG: " + String(hours) + "h" + String(mins) + "m");
        } else {
          display.print("TTG: " + String(mins) + "min");
        }
      } else if (currentData.current > 0.1) {
        display.print("CHARGING");
      } else if (currentData.current < -0.1) {
        display.print("DISCHARGING");
      } else {
        display.print("IDLE");
      }
    }
    
    display.setCursor(200, 90);
    int signalBars;
    if (currentData.rssi >= -82) {
      signalBars = 4;
    } else if (currentData.rssi >= -85) {
      signalBars = 3;
    } else if (currentData.rssi >= -89) {
      signalBars = 2;
    } else {
      signalBars = 1;
    }
    
    for (int i = 0; i < 4; i++) {
      if (i < signalBars) {
        display.print("|");
      } else {
        display.print(".");
      }
    }
    
    display.setCursor(10, 115);
    display.print("Used: " + String(abs(currentData.consumed_ah), 1) + "Ah");
    
    display.setCursor(180, 115);
    if (currentData.alarms != 0) {
      display.print("ALARM!");
    } else {
      display.print("OK");
    }
  }
  
  display.display(!useFullUpdate);
  
  lastDisplayedData = currentData;
  screenNeedsUpdate = false;
  lastScreenUpdate = currentTime;
}

void Display::showNoData() {
  display.setRotation(1);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  display.setTextColor(GxEPD_BLACK);
  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 40);
  display.print("NO DATA");
  
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(10, 70);
  display.print("Searching...");
  
  display.display(false);
}

void Display::showTestScreen() {
  display.setRotation(1);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 30);
  display.print("Battery Monitor");
  
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(10, 55);
  display.print("Waiting for data...");
  
  display.setCursor(10, 80);
  display.print("BTLE Power Gauge");
  
  display.setCursor(10, 105);
  display.print("Ready");
  
  display.display(false);
}

void Display::showConfigScreen(const String& title, const String& line1, const String& line2, const String& line3, const String& line4) {
  display.setRotation(1);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  
  display.setTextColor(GxEPD_BLACK);
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(10, 30);
  display.print(title);
  
  display.setFont(&FreeMonoBold9pt7b);
  
  if (line1.length() > 0) {
    display.setCursor(10, 55);
    display.print(line1);
  }
  
  if (line2.length() > 0) {
    display.setCursor(10, 75);
    display.print(line2);
  }
  
  if (line3.length() > 0) {
    display.setCursor(10, 95);
    display.print(line3);
  }
  
  if (line4.length() > 0) {
    display.setCursor(10, 115);
    display.print(line4);
  }
  
  display.display(false);
}

void Display::showSleepScreen() {
  display.setRotation(1);
  display.setFullWindow();
  display.fillScreen(GxEPD_WHITE);
  
  display.setTextColor(GxEPD_BLACK);
  
  // Draw a simple "sleep" icon using text characters
  display.setFont(&FreeMonoBold18pt7b);
  display.setCursor(120, 40);
  display.print("ZZ");
  
  display.setFont(&FreeMonoBold12pt7b);
  display.setCursor(90, 70);
  display.print("SLEEPING");
  
  display.setFont(&FreeMonoBold9pt7b);
  display.setCursor(70, 95);
  display.print("Press button to wake");
  
  display.setCursor(85, 115);
  display.print("Power saving mode");
  
  display.display(false);
}

void Display::setBrightness(uint8_t brightness) {
  Serial.printf("E-ink displays don't support brightness control\n");
}

void Display::clearScreen() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());
}

void Display::drawText(int16_t x, int16_t y, const String& text, const GFXfont* font) {
  display.setTextColor(GxEPD_BLACK);
  if (font) {
    display.setFont(font);
  } else {
    display.setFont();
  }
  display.setCursor(x, y);
  display.print(text);
}

void Display::forceNextUpdate() {
  // Reset the display state to force an update on next refresh
  memset(&lastDisplayedData, 0, sizeof(lastDisplayedData));
  lastDisplayedData.data_valid = false;
  screenNeedsUpdate = true;
  Serial.println("Display: Forcing next update (reset after wake from sleep)");
} 