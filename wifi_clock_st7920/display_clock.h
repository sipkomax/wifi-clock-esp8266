#pragma once
#include "config.h"

// ============================================================
//  ДИСПЛЕЙ — ЧАСЫ
// ============================================================
void drawClock() {
  unsigned long now = millis();

  // Обновляем дисплей только когда нужно:
  // - каждую секунду (часы с секундами) или минуту (без секунд)
  // - каждые SCROLL_SPEED мс (бегущая строка)
  static unsigned long lastScrollDraw = 0;
  static char lastTimeBuf[9] = {0};
  static bool firstDrawDone = false;
  const unsigned long CLOCK_SCROLL_INTERVAL = max((unsigned long)SCROLL_SPEED, 40UL);

  time_t t = timeClient.getEpochTime() + cfg.timezone * 3600L;
  struct tm* ti = gmtime(&t);
  char timeBuf[9], dateBuf[20];
  if (cfg.showSeconds)
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
  else
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti->tm_hour, ti->tm_min);

  bool timeChanged = (strcmp(timeBuf, lastTimeBuf) != 0);
  bool scrollTick  = (now - lastScrollDraw >= CLOCK_SCROLL_INTERVAL);

  // Если ничего не изменилось — не перерисовываем
  if (!timeChanged && !scrollTick) return;

  if (timeChanged) {
    strncpy(lastTimeBuf, timeBuf, sizeof(lastTimeBuf) - 1);
    lastTimeBuf[sizeof(lastTimeBuf) - 1] = '\0';
  }
  if (scrollTick) lastScrollDraw = now;

  const char* mo[] = {"Янв","Фев","Мар","Апр","Май","Июн","Июл","Авг","Сен","Окт","Ноя","Дек"};
  const char* wd[] = {"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};
  snprintf(dateBuf, sizeof(dateBuf), "%s %d %s %d", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon], 1900+ti->tm_year);

  bool fullRedraw = !firstDrawDone || timeChanged;
  if (fullRedraw) {
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(2, 10, dateBuf);
    u8g2.drawHLine(0, 13, 128);
    if (cfg.showSeconds) {
      u8g2.setFont(u8g2_font_logisoso24_tf);
      int tw = u8g2.getStrWidth(timeBuf);
      u8g2.drawUTF8((128 - tw) / 2, 44, timeBuf);
    } else {
      u8g2.setFont(u8g2_font_logisoso32_tf);
      int tw = u8g2.getStrWidth(timeBuf);
      u8g2.drawUTF8((128-tw)/2, 50, timeBuf);
    }
    u8g2.drawHLine(0, 51, 128);
  } else {
    // На шаге скролла трогаем только нижнюю полосу, чтобы меньше мерцал экран
    u8g2.setDrawColor(0);
    u8g2.drawBox(0, 52, 128, 12);
    u8g2.setDrawColor(1);
  }

  // Нижняя строка
  if (millis() - bootMs < 3000) {
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(2, 62, wifiConnected ? WiFi.localIP().toString().c_str() : "AP: 192.168.4.1");
  } else {
    // Бегущая строка: текущий праздник
    const char* hol = getTodayHoliday();
    String holidayLine = (hol && hol[0] != '\0') ? String(hol) : String("Сегодня без праздников");
    if (holidayLine != bottomLine) { bottomLine = holidayLine; scrollPos = 128; }
    if (bottomLine.length() > 0) {
      u8g2.setFont(u8g2_font_6x13_t_cyrillic);
      int textW = u8g2.getUTF8Width(bottomLine.c_str());
      if (textW <= 124) {
        u8g2.drawUTF8((128 - textW) / 2, 62, bottomLine.c_str());
      } else {
        if (scrollTick) {
          scrollPos -= 2; // быстрее и читабельнее, чем по 1 пикселю
          if (scrollPos < -(int)textW) scrollPos = 128;
        }
        u8g2.drawUTF8(scrollPos, 62, bottomLine.c_str());
      }
    }
  }
  u8g2.sendBuffer();
  firstDrawDone = true;
}
