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
  tft.setTextColor(COLOR_VOLTAGE, COLOR_BG);
  tft.setTextSize(2);
  if (abs(currentData.voltage - lastDisplayedData.voltage) > 0.01f) {
    clearValue(5, 5, 100, 16);
    tft.setCursor(5, 5);
    tft.print(String(currentData.voltage, 2) + "V");
  }
  if (currentData.ttg_minutes != lastDisplayedData.ttg_minutes && currentData.ttg_minutes > 0) {
    clearValue(120, 5, 100, 16);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    int hours = currentData.ttg_minutes / 60;
    int mins = currentData.ttg_minutes % 60;
    tft.setCursor(120, 10);
    tft.print("TTG: " + String(hours) + "h" + String(mins) + "m");
  }
}

void Display::drawCurrentAndPower() {
  uint16_t currentColor = COLOR_TEXT;
  if (currentData.current > 0.1f) currentColor = COLOR_CURRENT_CHARGE;
  else if (currentData.current < -0.1f) currentColor = COLOR_CURRENT_DISCHARGE;
  
  if (abs(currentData.current - lastDisplayedData.current) > 0.01f) {
    clearValue(5, 25, 120, 16);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(5, 25);
    tft.print(String(currentData.current, 2) + "A");
  }
  if (abs(currentData.power - lastDisplayedData.power) > 0.1f) {
    clearValue(130, 25, 100, 16);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(130, 25);
    tft.print(String(abs(currentData.power), 0) + "W");
  }
  if ((currentData.current > 0.1f) != (lastDisplayedData.current > 0.1f) ||
      (currentData.current < -0.1f) != (lastDisplayedData.current < -0.1f)) {
    clearValue(5, 45, 120, 10);
    tft.setTextColor(currentColor, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(5, 45);
    if (currentData.current > 0.1f) tft.print("CHARGING");
    else if (currentData.current < -0.1f) tft.print("DISCHARGING");
    else tft.print("IDLE");
  }
}

void Display::drawSOCBar() {
  if (abs(currentData.soc - lastDisplayedData.soc) > 0.1f) {
    clearValue(5, 60, 60, 16);
    uint16_t socColor = COLOR_SOC_HIGH;
    if (currentData.soc < 20.0f) socColor = COLOR_SOC_LOW;
    else if (currentData.soc < 50.0f) socColor = COLOR_SOC_MED;
    tft.setTextColor(socColor, COLOR_BG);
    tft.setTextSize(2);
    tft.setCursor(5, 60);
    tft.print(String(currentData.soc, 1) + "%");
    int barWidth = 150, barHeight = 8, barX = 75, barY = 65;
    tft.drawRect(barX, barY, barWidth, barHeight, COLOR_TEXT);
    tft.fillRect(barX + 1, barY + 1, barWidth - 2, barHeight - 2, COLOR_BG);
    int fillWidth = (int)((currentData.soc / 100.0f) * (barWidth - 2));
    if (fillWidth > 0) tft.fillRect(barX + 1, barY + 1, fillWidth, barHeight - 2, socColor);
  }
}

void Display::drawAuxInfo() {
  if (abs(currentData.consumed_ah - lastDisplayedData.consumed_ah) > 0.1f) {
    clearValue(5, 80, 100, 10);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(5, 80);
    tft.print("Used: " + String(currentData.consumed_ah, 1) + "Ah");
  }
  if (currentData.alarms != lastDisplayedData.alarms && currentData.alarms != 0) {
    clearValue(120, 80, 70, 20);
    tft.setTextColor(COLOR_ALARM, COLOR_BG);
    tft.setTextSize(1);
    tft.setCursor(120, 80);
    tft.print("ALARM!");
    tft.setCursor(120, 95);
    tft.print("0x" + String(currentData.alarms, HEX));
  }
}

void Display::drawConnectionStatus() {
  if (currentData.rssi != lastDisplayedData.rssi) {
    clearValue(5, 110, 100, 10);
    tft.setTextColor(COLOR_TEXT, COLOR_BG);
    tft.setTextSize(1);
    uint16_t signalColor = COLOR_TEXT;
    if (currentData.rssi > -50) signalColor = COLOR_CURRENT_CHARGE;
    else if (currentData.rssi < -80) signalColor = COLOR_CURRENT_DISCHARGE;
    tft.setTextColor(signalColor, COLOR_BG);
    tft.setCursor(5, 110);
    tft.print("Signal: " + String(currentData.rssi) + "dBm");
  }
}

void Display::clearValue(int x, int y, int w, int h) {
  tft.fillRect(x, y, w, h, COLOR_BG);
}

void Display::setBrightness(uint8_t brightness) {
  // Placeholder for PWM control
} 