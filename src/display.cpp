#include "display.h"

Display::Display() : tft(TFT_eSPI()) {
  memset(&currentData, 0, sizeof(currentData));
  memset(&lastDisplayedData, 0, sizeof(lastDisplayedData));
}

bool Display::begin() {
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(COLOR_BG);
  tft.setTextDatum(TL_DATUM);
  showNoData();
  return true;
}

void Display::updateData(const BatteryData& data) {
  currentData = data;
  currentData.last_update = millis();
  screenNeedsUpdate = true;
}

void Display::refresh() {
  if (!screenNeedsUpdate && millis() - lastScreenUpdate < 1000) return;
  if (millis() - currentData.last_update > 30000) {
    showNoData();
    return;
  }
  if (!currentData.data_valid) {
    showNoData();
    return;
  }
  
  // Clear the screen completely when transitioning from no data to real data
  bool wasShowingNoData = (millis() - lastDisplayedData.last_update > 30000) || !lastDisplayedData.data_valid;
  if (wasShowingNoData) {
    tft.fillScreen(COLOR_BG);
  }
  
  drawBatteryInfo();
  drawCurrentAndPower();
  drawSOCBar();
  drawAuxInfo();
  drawConnectionStatus();
  lastDisplayedData = currentData;
  screenNeedsUpdate = false;
  lastScreenUpdate = millis();
}

void Display::showNoData() {
  tft.fillRect(0, 0, 240, 135, COLOR_BG);
  tft.setTextColor(TFT_RED, COLOR_BG);
  tft.setTextSize(2);
  tft.setCursor(70, 50);
  tft.print("No Data");
  tft.setTextColor(COLOR_TEXT, COLOR_BG);
  tft.setTextSize(1);
  tft.setCursor(50, 80);
  tft.print("Waiting for SmartShunt...");
}

void Display::showTestScreen() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Battery Monitor");
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.print("Text rendering works!");
}

void Display::drawHeader() {
  // Header removed - no longer used
}

void Display::drawBatteryInfo() {
  // Voltage: Top right, large text
  tft.setTextColor(COLOR_VOLTAGE, COLOR_BG);
  tft.setTextSize(3);
  if (abs(currentData.voltage - lastDisplayedData.voltage) > 0.01f) {
    clearValue(120, 5, 115, 25);
    tft.setCursor(120, 5);
    tft.print(String(currentData.voltage, 2) + "V");
  }
  
  // Time to Go: Right side, medium text
  if (currentData.ttg_minutes != lastDisplayedData.ttg_minutes && currentData.ttg_minutes > 0) {
    clearValue(120, 35, 115, 20);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(2);
    int hours = currentData.ttg_minutes / 60;
    int mins = currentData.ttg_minutes % 60;
    tft.setCursor(120, 35);
    if (hours > 0) {
      tft.print(String(hours) + "h" + String(mins) + "m");
    } else {
      tft.print(String(mins) + "min");
    }
  }
}

void Display::drawCurrentAndPower() {
  uint16_t currentColor = COLOR_TEXT;
  if (currentData.current > 0.1f) currentColor = COLOR_CURRENT_CHARGE;
  else if (currentData.current < -0.1f) currentColor = COLOR_CURRENT_DISCHARGE;
  
  // Current: Right middle, large text - space for up to 999A
  if (abs(currentData.current - lastDisplayedData.current) > 0.01f) {
    clearValue(120, 60, 115, 25);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(3);
    tft.setCursor(120, 60);
    // Format current to handle high values properly
    if (abs(currentData.current) >= 100.0f) {
      tft.print(String((int)currentData.current) + "A");  // No decimal for 100A+
    } else if (abs(currentData.current) >= 10.0f) {
      tft.print(String(currentData.current, 1) + "A");    // One decimal for 10-99A
    } else {
      tft.print(String(currentData.current, 1) + "A");    // One decimal for <10A
    }
  }
  
  // Power: Right lower, large text - space for up to 9999W
  if (abs(currentData.power - lastDisplayedData.power) > 0.1f) {
    clearValue(120, 90, 115, 25);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(3);
    tft.setCursor(120, 90);
    // Format power to handle high values
    if (abs(currentData.power) >= 1000.0f) {
      tft.print(String((int)(currentData.power/1000)) + "kW");  // Show as kW for 1000W+
    } else {
      tft.print(String((int)abs(currentData.power)) + "W");    // Watts for <1000W
    }
  }
  
  // Status: Bottom right, smaller text
  if ((currentData.current > 0.1f) != (lastDisplayedData.current > 0.1f) ||
      (currentData.current < -0.1f) != (lastDisplayedData.current < -0.1f)) {
    clearValue(120, 115, 115, 20);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(120, 115);
    if (currentData.current > 0.1f) tft.print("CHARGE");
    else if (currentData.current < -0.1f) tft.print("DRAIN");
    else tft.print("IDLE");
  }
}

void Display::drawSOCBar() {
  if (abs(currentData.soc - lastDisplayedData.soc) > 0.1f) {
    // Clear the entire left area for battery display
    clearValue(5, 5, 110, 125);
    uint16_t socColor = COLOR_SOC_HIGH;
    if (currentData.soc < 20.0f) socColor = COLOR_SOC_LOW;
    else if (currentData.soc < 50.0f) socColor = COLOR_SOC_MED;
    
    // MASSIVE battery percentage at the top - size adjusts for 100%
    tft.setTextColor(socColor, COLOR_BG);
    if (currentData.soc >= 100.0f) {
      // Size 5 for "100%" to fit in width (30x40px per char * 4 = 120px)
      tft.setTextSize(5);
      tft.setCursor(5, 10);
      tft.print("100%");
    } else {
      // Size 6 for "99%" and below (36x48px per char * 3 = 108px)
      tft.setTextSize(6);
      tft.setCursor(5, 5);
      tft.print(String((int)currentData.soc) + "%");
    }
    
    // "Total" label below the percentage - medium size
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(5, 65);
    tft.print("Total");
    
    // Total value on separate line - large size
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(3);
    tft.setCursor(5, 85);
    // Format for space efficiency - handles high values
    if (abs(currentData.consumed_ah) >= 1000.0f) {
      // Very high values: show in kAh
      tft.print(String(currentData.consumed_ah/1000.0f, 1) + "kAh");
    } else if (abs(currentData.consumed_ah) >= 100.0f) {
      // 100+ Ah: no decimal
      tft.print(String((int)currentData.consumed_ah) + "Ah");
    } else {
      // <100 Ah: one decimal
      tft.print(String(currentData.consumed_ah, 1) + "Ah");
    }
  }
}

void Display::drawAuxInfo() {
  // Total is now handled in drawSOCBar() to avoid spacing issues
  
  // Alarms: Full width at very bottom if present
  if (currentData.alarms != lastDisplayedData.alarms && currentData.alarms != 0) {
    clearValue(0, 125, 240, 10);
    tft.setTextColor(COLOR_ALARM, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(5, 125);
    tft.print("ALARM: 0x" + String(currentData.alarms, HEX));
  }
}

void Display::drawConnectionStatus() {
  // Connection status removed to maximize space for data
}

void Display::clearValue(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, COLOR_BG);
}

void Display::setBrightness(uint8_t brightness) {
  // Placeholder for PWM control
} 