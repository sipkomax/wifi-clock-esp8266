#pragma once
#include "config.h"

// ============================================================
//  ДИСПЛЕЙ — ПОГОДА
// ============================================================
void drawWeather() {
  u8g2.clearBuffer();

  // ── Шапка: иконка погоды + город ──
  u8g2.drawXBMP(0, 0, 16, 16, getWeatherIcon(wx.icon));
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(20, 12, cfg.city);
  u8g2.drawHLine(0, 14, 128);

  if (!wx.valid) {
    u8g2.drawUTF8(10, 40, "Нет данных");
    u8g2.sendBuffer(); return;
  }

  // ── Левая колонка: большая температура ──
  u8g2.setFont(u8g2_font_logisoso32_tf);
  char tBuf[8];
  snprintf(tBuf, sizeof(tBuf), "%.0f", wx.temp);
  u8g2.drawUTF8(0, 48, tBuf);

  // Знак градуса — вычисляем ширину ПОКА активен logisoso32
  int tW = u8g2.getStrWidth(tBuf);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(tW + 2, 20, "`");

  // ── Правая колонка: иконки + значения ──
  // Иконка x=62, текст x=80 — влезает до 8 символов (128-80=48px / 6px)
  char buf[18];

  // Термометр + ощущаемая температура
  u8g2.drawXBMP(62, 16, 16, 16, ico_thermometer);
  snprintf(buf, sizeof(buf), "%.0f`C", wx.feels_like);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(80, 28, buf);

  // Ветер + скорость
  u8g2.drawXBMP(62, 33, 16, 16, ico_wind);
  snprintf(buf, sizeof(buf), "%.1fm/s", wx.wind);
  u8g2.drawUTF8(80, 45, buf);

  // ── Нижняя строка: описание + влажность ──
  u8g2.drawHLine(0, 51, 128);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  // Описание кириллицей — drawUTF8, обрезаем до 10 символов
  char desc[40] = {0};
  int dc = 0, bc = 0;
  const char* dp = wx.desc;
  while (*dp && dc < 10) {
    int cl = (*dp & 0x80) ? ((*dp & 0xE0)==0xC0 ? 2 : 3) : 1;
    if (bc + cl >= 39) break;
    memcpy(desc + bc, dp, cl); bc += cl; dc++; dp += cl;
  }
  u8g2.drawUTF8(0, 63, desc);
  // Влажность справа
  snprintf(buf, sizeof(buf), "H:%.0f%%", wx.humid);
  u8g2.drawStr(96, 63, buf);

  u8g2.sendBuffer();
}

// ============================================================
//  ДИСПЛЕЙ — ПРОГНОЗ 3 ДНЯ
// ============================================================
void drawForecast() {
  u8g2.clearBuffer();

  // ── Шапка ──
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(14, 10, "--- 3 дня ---");
  u8g2.drawHLine(0, 12, 128);

  if (!forecastValid) {
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(10, 40, "Нет данных");
    u8g2.sendBuffer(); return;
  }

  for (int i = 0; i < 3; i++) {
    int x = i * 43;   // колонка 42px + 1px разделитель
    char buf[12];

    // День недели — по центру колонки (кириллица)
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    int dw = u8g2.getUTF8Width(forecast[i].day);
    u8g2.drawUTF8(x + (42 - dw) / 2, 23, forecast[i].day);

    // Иконка погоды — по центру колонки
    u8g2.drawXBMP(x + 13, 25, 16, 16, getWeatherIcon(forecast[i].icon));

    // Температура: +max/min
    snprintf(buf, sizeof(buf), "%+.0f/%+.0f", forecast[i].tempMax, forecast[i].tempMin);
    u8g2.setFont(u8g2_font_5x8_tf);
    int tw = u8g2.getStrWidth(buf);
    u8g2.drawStr(x + (42 - tw) / 2, 53, buf);

    // Ветер: иконка + значение
    u8g2.drawXBMP(x + 3, 54, 16, 16, ico_wind);
    snprintf(buf, sizeof(buf), "%.0fm/s", forecast[i].windMax);
    u8g2.drawStr(x + 20, 64, buf);

    // Разделитель
    if (i < 2) u8g2.drawVLine(x + 42, 12, 52);
  }
  u8g2.sendBuffer();
}

// ============================================================
//  ДИСПЛЕЙ — ДАТЧИК
// ============================================================
void drawSensor() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(0, 12, "Датчик:");
  u8g2.drawHLine(0, 15, 128);
  if (isnan(sensorTemp)) {
    u8g2.drawUTF8(0, 40, "Нет данных");
    u8g2.sendBuffer(); return;
  }
  u8g2.drawXBMP(0, 18, 16, 16, ico_thermometer);
  u8g2.setFont(u8g2_font_logisoso24_tf);
  char buf[10];
  snprintf(buf, sizeof(buf), "%.1f`", sensorTemp);
  u8g2.drawUTF8(20, 44, buf);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "Влаж: %.0f%%", sensorHum);
  u8g2.drawUTF8(0, 62, buf);
  u8g2.sendBuffer();
}
