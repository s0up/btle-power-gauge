#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
#include <Fonts/FreeMonoBold12pt7b.h>
#include <Fonts/FreeMonoBold18pt7b.h>

// E-ink display pins for Vision Master E290 (from working ESPHome example)
#define EPD_CS     3   // GPIO3 (corrected from working example)
#define EPD_DC     4   // GPIO4 (pin 19)  
#define EPD_RST    5   // GPIO5 (pin 15)
#define EPD_BUSY   6   // GPIO6 (pin 13)
#define EPD_MOSI   1   // GPIO1 (corrected from working example)
#define EPD_SCK    2   // GPIO2 (pin 21) - E-Ink CLK
#define EPD_POWER  18  // Power control pin - CRITICAL!

struct BatteryData {
  float voltage = 0.0f;
  float current = 0.0f;
  float power = 0.0f;
  float soc = 0.0f;
  float consumed_ah = 0.0f;
  uint16_t ttg_minutes = 0;
  float aux_value = 0.0f;
  uint8_t aux_type = 0; // 0=voltage, 2=temp, 3=midpoint
  uint16_t alarms = 0;
  int8_t rssi = 0;
  bool data_valid = false;
  unsigned long last_update = 0;
  
  // Calculated time fields
  uint16_t calculated_time_remaining_minutes = 0;  // Time remaining until empty (when discharging)
  uint16_t calculated_time_to_full_minutes = 0;    // Time to full charge (when charging)
  bool time_calculation_valid = false;             // Whether the time calculation is reliable
};

class Display {
private:
  GxEPD2_BW<GxEPD2_290_T94_V2, GxEPD2_290_T94_V2::HEIGHT> display;
  BatteryData currentData;
  BatteryData lastDisplayedData;
  unsigned long lastScreenUpdate = 0;
  bool screenNeedsUpdate = false;
  
  // Change detection method
  bool hasSignificantChange(const BatteryData& newData, const BatteryData& oldData);

public:
  Display();
  bool begin();
  void updateData(const BatteryData& data);
  void refresh();
  void showNoData();
  void showTestScreen();
  void showConfigScreen(const String& title, const String& line1 = "", const String& line2 = "", const String& line3 = "", const String& line4 = "");
  void showSleepScreen();
  void setBrightness(uint8_t brightness);
  void clearScreen();
  void drawText(int16_t x, int16_t y, const String& text, const GFXfont* font = nullptr);
  void forceNextUpdate(); // Force the next refresh to update display
  
  // Status information
  bool isUpdatePending() const { return screenNeedsUpdate; }
  unsigned long getLastUpdateTime() const { return lastScreenUpdate; }
  unsigned long getTimeSinceLastUpdate() const { return millis() - lastScreenUpdate; }
};

#endif // DISPLAY_H 