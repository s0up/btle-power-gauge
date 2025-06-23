#ifndef DISPLAY_H
#define DISPLAY_H

#include <TFT_eSPI.h>

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
};

class Display {
private:
  TFT_eSPI tft;
  BatteryData currentData;
  BatteryData lastDisplayedData;
  unsigned long lastScreenUpdate = 0;
  bool screenNeedsUpdate = false;
  
  void drawHeader();
  void drawBatteryInfo();
  void drawCurrentAndPower();
  void drawSOCBar();
  void drawAuxInfo();
  void drawConnectionStatus();
  void clearValue(int x, int y, int w, int h);
  
  // Color definitions
  static const uint16_t COLOR_BG = TFT_BLACK;
  static const uint16_t COLOR_TEXT = TFT_WHITE;
  static const uint16_t COLOR_HEADER = TFT_CYAN;
  static const uint16_t COLOR_VOLTAGE = TFT_YELLOW;
  static const uint16_t COLOR_CURRENT_CHARGE = TFT_GREEN;
  static const uint16_t COLOR_CURRENT_DISCHARGE = TFT_RED;
  static const uint16_t COLOR_SOC_HIGH = TFT_GREEN;
  static const uint16_t COLOR_SOC_MED = TFT_YELLOW;
  static const uint16_t COLOR_SOC_LOW = TFT_RED;
  static const uint16_t COLOR_ALARM = TFT_MAGENTA;

public:
  Display();
  bool begin();
  void updateData(const BatteryData& data);
  void refresh();
  void showNoData();
  void showTestScreen();
  void setBrightness(uint8_t brightness);
};

#endif // DISPLAY_H 