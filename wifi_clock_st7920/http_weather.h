#pragma once
#include "config.h"

// ============================================================
//  HTTP — ПОГОДА
// ============================================================
Screen nextActiveScreen(Screen from) {
  const Screen order[]  = {SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_FORECAST, SCREEN_TELEGRAM, SCREEN_SENSOR, SCREEN_HOLIDAY};
  const bool* enabled[] = {&cfg.scr_clock, &cfg.scr_weather, &cfg.scr_forecast, &cfg.scr_telegram, &cfg.scr_sensor, &cfg.scr_holiday};
  const int n = 6;
  int startIdx = 0;
  for (int i = 0; i < n; i++) if (order[i] == from) { startIdx = i; break; }
  for (int i = 1; i <= n; i++) {
    int idx = (startIdx + i) % n;
    if (*enabled[idx]) return order[idx];
  }
  return SCREEN_CLOCK;
}


void fetchWeather() {
  if (!wifiConnected || strlen(cfg.apiKey) < 10) return;
  String url = "http://api.openweathermap.org/data/2.5/weather?";
  bool isId = true;
  for (int i = 0; i < (int)strlen(cfg.city); i++) if (!isDigit(cfg.city[i])) { isId = false; break; }
  url += isId ? "id=" : "q=";
  url += cfg.city;
  url += "&appid=" + String(cfg.apiKey) + "&units=" + (cfg.wx_celsius ? "metric" : "imperial") + "&lang=ru";
  HTTPClient http; http.begin(wifiClient, url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(2048);
    auto err = deserializeJson(doc, http.getStream());
    if (!err) {
      wx.temp       = doc["main"]["temp"]       | 0.0f;
      wx.feels_like = doc["main"]["feels_like"] | 0.0f;
      wx.humid      = doc["main"]["humidity"]   | 0.0f;
      wx.wind       = doc["wind"]["speed"]      | 0.0f;
      const char* d = doc["weather"][0]["description"] | "---";
      const char* ic= doc["weather"][0]["icon"]        | "01d";
      strncpy(wx.desc, d,  31); wx.desc[31] = 0;
      strncpy(wx.icon, ic,  3); wx.icon[3]  = 0;
      wx.valid = true;
      Serial.printf("[Weather] OK, heap=%u\n", ESP.getFreeHeap());
    }
  }
  http.end();
}
