#pragma once
#include "config.h"

// ============================================================
//  BOOT ЭКРАН
// ============================================================
void drawBoot(const char* msg, int pct) {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(0, 12, "WiFi Clock v2.0");
  u8g2.drawHLine(0, 15, 128);
  u8g2.drawUTF8(0, 32, msg);
  u8g2.drawFrame(0, 40, 128, 12);
  u8g2.drawBox(0, 40, map(pct, 0, 100, 0, 128), 12);
  u8g2.sendBuffer();
}
