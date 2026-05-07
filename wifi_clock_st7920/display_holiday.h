#pragma once
#include "config.h"

// ============================================================
//  ДИСПЛЕЙ — ПРАЗДНИКИ
// ============================================================
//  ДИСПЛЕЙ — TELEGRAM
// ============================================================
void drawTelegram() {
  u8g2.clearBuffer();

  // Шапка
  char hdr[38];
  snprintf(hdr, sizeof(hdr), "@%s:", cfg.tg_channel);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(0, 10, hdr);
  if (strlen(tgDate) > 0) {
    u8g2.setFont(u8g2_font_5x7_tf);
    u8g2.drawStr(128 - 55, 10, tgDate);
  }
  u8g2.drawHLine(0, 13, 128);

  if (!tgValid) {
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(10, 40, "Нет данных");
    u8g2.sendBuffer(); return;
  }

  // Компактный формат без прокрутки:
  // Водоем / Рыба / Координаты / Квадрат / Скорость
  String txt = String(tgText);
  String fields[5];
  fields[0] = "Водоем: -";
  fields[1] = "Рыба: -";
  fields[2] = "Координаты: -";
  fields[3] = "Квадрат: -";
  fields[4] = "Скорость: -";

  auto pickField = [&](const char* key) -> String {
    int p = txt.indexOf(key);
    if (p < 0) return "";
    int start = p + (int)strlen(key);
    while (start < (int)txt.length() && txt[start] == ' ') start++;
    int end = txt.indexOf(" - ", start);
    if (end < 0) end = txt.indexOf(" | ", start);
    if (end < 0) end = txt.length();
    String v = txt.substring(start, end);
    v.trim();
    return v;
  };

  String v;
  v = pickField("Водоем:");
  if (v.length() == 0) v = pickField("Водоём:");
  if (v.length() > 0) fields[0] = "Водоем: " + v;
  v = pickField("Рыба:");
  if (v.length() > 0) fields[1] = "Рыба: " + v;
  v = pickField("Координаты:");
  if (v.length() > 0) fields[2] = "Координаты: " + v;
  v = pickField("Квадрат:");
  if (v.length() > 0) fields[3] = "Квадрат: " + v;
  v = pickField("Скорость:");
  if (v.length() > 0) fields[4] = "Скорость: " + v;

  u8g2.setFont(u8g2_font_5x7_tf);
  const int Y_START = 22;
  const int LINE_H = 10;
  for (int i = 0; i < 5; i++) {
    int maxChars = (i == 0) ? 24 : 25;
    if ((int)fields[i].length() > maxChars) fields[i] = fields[i].substring(0, maxChars - 1) + "~";
    u8g2.drawUTF8(0, Y_START + i * LINE_H, fields[i].c_str());
  }

  u8g2.sendBuffer();
}

static void activateScreen(Screen next, unsigned long now) {
  currentScreen = next;
  screenTimer   = now;
  if (currentScreen == SCREEN_TELEGRAM) tgScrollPos = 0;
  if (currentScreen == SCREEN_HOLIDAY)  holScrollPos = 0;
  if (currentScreen == SCREEN_CLOCK)    scrollPos = 128; // начать бегущую строку сначала
}

void handleScreenCycle() {
  unsigned long now     = millis();
  unsigned long elapsed = now - screenTimer;
  unsigned long limits[6] = {
    (unsigned long)cfg.time_clock    * 1000UL,
    (unsigned long)cfg.time_weather  * 1000UL,
    (unsigned long)cfg.time_forecast * 1000UL,
    (unsigned long)cfg.time_telegram * 1000UL,
    (unsigned long)cfg.time_sensor   * 1000UL,
    (unsigned long)cfg.time_holiday  * 1000UL,
  };
  const Screen order[6] = {SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_FORECAST, SCREEN_TELEGRAM, SCREEN_SENSOR, SCREEN_HOLIDAY};
  for (int i = 0; i < 6; i++) {
    if (currentScreen == order[i] && elapsed > limits[i]) {
      activateScreen(nextActiveScreen(order[i]), now);
      break;
    }
  }
}

void handleScreenButton() {
  if (!cfg.scr_button) return;

  static bool lastReading = HIGH;
  static bool stableState = HIGH;
  static unsigned long lastDebounceMs = 0;
  const unsigned long debounceMs = 35UL;

  bool reading = digitalRead(SCREEN_BUTTON_PIN);
  unsigned long now = millis();

  if (reading != lastReading) {
    lastDebounceMs = now;
    lastReading = reading;
  }

  if ((now - lastDebounceMs) < debounceMs) return;
  if (reading == stableState) return;

  stableState = reading;
  if (stableState == LOW) {
    activateScreen(nextActiveScreen(currentScreen), now);
  }
}
