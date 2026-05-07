#pragma once
#include "config.h"

// ============================================================
//  UTF-8 утилиты для бегущей строки
// ============================================================
int utf8CharLen(const char* s) {
  unsigned char c = (unsigned char)s[0];
  if (!c) return 0;
  if (c < 0x80) return 1;
  if ((c & 0xE0) == 0xC0) return 2;
  if ((c & 0xF0) == 0xE0) return 3;
  return 1;
}
int utf8ByteOffset(const char* s, int skipChars) {
  int pos = 0;
  for (int i = 0; i < skipChars; i++) {
    int cl = utf8CharLen(s+pos);
    if (!cl) break;
    pos += cl;
  }
  return pos;
}
int utf8Strlen(const char* s) {
  int n = 0; const char* p = s;
  while (*p) { int cl = utf8CharLen(p); if (!cl) break; n++; p += cl; }
  return n;
}

// ============================================================
//  ДИСПЛЕЙ — ПРАЗДНИКИ
// ============================================================
void drawHoliday() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  time_t t = timeClient.getEpochTime() + cfg.timezone * 3600L;
  struct tm* ti = gmtime(&t);
  char dateBuf[24];
  const char* mo[] = {"Янв","Фев","Мар","Апр","Май","Июн","Июл","Авг","Сен","Окт","Ноя","Дек"};
  snprintf(dateBuf, sizeof(dateBuf), "%d %s - праздники", ti->tm_mday, mo[ti->tm_mon]);
  u8g2.drawUTF8(0, 10, dateBuf);
  u8g2.drawHLine(0, 13, 128);

  const char* hol = getTodayHoliday();
  if (!hol || hol[0] == '\0') {
    u8g2.drawUTF8(10, 40, "Нет праздников");
    u8g2.sendBuffer(); return;
  }

  static String holLines[10];
  static int    holLineCount = 0;
  static char   holCached[8] = {0};
  char dateKey[8];
  snprintf(dateKey, sizeof(dateKey), "%02d%02d", ti->tm_mday, ti->tm_mon);

  if (strcmp(holCached, dateKey) != 0) {
    strncpy(holCached, dateKey, sizeof(holCached) - 1);
    holCached[sizeof(holCached) - 1] = '\0';
    holLineCount = 0;
    String src = String(hol);
    int start = 0;
    while (holLineCount < 10) {
      int sep = src.indexOf(" \xe2\x80\xa2 ", start);
      String part = (sep < 0) ? src.substring(start) : src.substring(start, sep);
      part.trim();
      if (part.length() > 0) {
        if (part.length() <= 21) {
          holLines[holLineCount++] = part;
        } else {
          holLines[holLineCount++] = part.substring(0, 21);
          if (holLineCount < 10) holLines[holLineCount++] = " " + part.substring(21);
        }
      }
      if (sep < 0) break;
      start = sep + 5;
    }
    holScrollPos = 0;
  }

  const int LINES_ON_SCREEN = 3;
  const int Y_START = 27;
  const int LINE_H  = 13;
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  for (int i = 0; i < LINES_ON_SCREEN; i++) {
    int lineIdx = holScrollPos + i;
    if (lineIdx < holLineCount)
      u8g2.drawUTF8(0, Y_START + i * LINE_H, holLines[lineIdx].c_str());
  }

  int maxScroll = max(0, holLineCount - LINES_ON_SCREEN);
  if (maxScroll > 0) {
    int barH = constrain(map(holLineCount, 3, 10, 48, 8), 6, 48);
    int barY = 15 + map(holScrollPos, 0, maxScroll, 0, 48 - barH);
    u8g2.drawVLine(126, 15, 49);
    u8g2.drawBox(124, barY, 3, barH);
  }
  u8g2.sendBuffer();

  if (maxScroll > 0 && millis() - holScrollTimer > 2000UL) {
    holScrollTimer = millis();
    if (holScrollPos < maxScroll) holScrollPos++;
  }
}
