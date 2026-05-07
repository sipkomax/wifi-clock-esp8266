#pragma once
#include "config.h"

// ============================================================
//  HTTP — ПРОГНОЗ
// ============================================================
void fetchForecast() {
  if (!wifiConnected || strlen(cfg.apiKey) < 10) return;

  // Проверяем NTP синхронизацию
  unsigned long epoch = timeClient.getEpochTime();
  if (epoch < 1000000000UL) return;

  // Формируем URL
  String url = "http://api.openweathermap.org/data/2.5/forecast?";
  bool isId = true;
  for (int i = 0; i < (int)strlen(cfg.city); i++)
    if (!isDigit(cfg.city[i])) { isId = false; break; }
  url += isId ? "id=" : "q=";
  url += cfg.city;
  url += "&appid=" + String(cfg.apiKey)
       + "&units=" + (cfg.wx_celsius ? "metric" : "imperial")
       // 24 интервала по 3 ч = 72 ч — хватает на 3 календарных дня после сегодня, меньше трафика/памяти
       + "&lang=ru&cnt=24";

  HTTPClient http;
  http.begin(wifiClient, url);
  http.useHTTP10(true);
  http.setTimeout(12000);
  http.addHeader("Accept-Encoding", "identity");

  int code = http.GET();
  if (code != 200) { http.end(); return; }

  // Парсим с потока (как текущая погода): без большого String буфера, быстрее и надёжнее на ESP8266
  StaticJsonDocument<128> filter;
  filter["list"][0]["dt"]                 = true;
  filter["list"][0]["main"]["temp"]       = true;
  filter["list"][0]["wind"]["speed"]      = true;
  filter["list"][0]["weather"][0]["icon"] = true;

  DynamicJsonDocument doc(6144);
  auto err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("[Forecast] JSON err=%s heap=%u\n", err.c_str(), ESP.getFreeHeap());
    return;
  }

  // Вычисляем 3 следующих дня
  long today_day = (long)(epoch / 86400L);
  const char* wdn[] = {"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};
  for (int i = 0; i < 3; i++) {
    forecast[i].tempMin = 99; forecast[i].tempMax = -99; forecast[i].windMax = 0;
  }
  int dayIdx = -1;
  long lastDay = -1;
  for (JsonObject item : doc["list"].as<JsonArray>()) {
    long dt   = item["dt"] | 0L;
    long iday = dt / 86400L;
    if (iday <= today_day) continue;
    if (iday != lastDay) {
      lastDay = iday;
      if (dayIdx + 1 >= 3) break;
      dayIdx++;
      long dt_local = dt + cfg.timezone * 3600L;
      int wday = (int)((dt_local / 86400L + 4) % 7);
      strncpy(forecast[dayIdx].day, wdn[wday], 5); forecast[dayIdx].day[5] = 0;
      const char* ic = item["weather"][0]["icon"] | "01d";
      strncpy(forecast[dayIdx].icon, ic, 3); forecast[dayIdx].icon[3] = 0;
    }
    if (dayIdx < 0 || dayIdx >= 3) continue;
    float t = item["main"]["temp"] | 0.0f;
    float w = item["wind"]["speed"] | 0.0f;
    if (t < forecast[dayIdx].tempMin) forecast[dayIdx].tempMin = t;
    if (t > forecast[dayIdx].tempMax) forecast[dayIdx].tempMax = t;
    if (w > forecast[dayIdx].windMax) forecast[dayIdx].windMax = w;
  }
  forecastValid = (dayIdx >= 0);
}
void fetchTelegram() {
  if (!wifiConnected) return;
  if (strlen(cfg.tg_channel) < 2) { Serial.println("[TG] channel not set"); return; }

  String url = "http://tg.i-c-a.su/rss/";
  url += cfg.tg_channel;
  Serial.print("[TG] GET "); Serial.println(url);

  HTTPClient http;
  http.begin(wifiClient, url);
  http.setTimeout(15000);
  int code = http.GET();
  Serial.printf("[TG] HTTP code=%d heap=%u\n", code, ESP.getFreeHeap());
  if (code != 200) { http.end(); Serial.println("[TG] HTTP error"); return; }

  // Читаем поток — ищем первый <item> и берём его <description>
  WiFiClient* stream = http.getStreamPtr();
  String foundDesc = "";
  String foundDate  = "";
  String buf = "";
  bool   inItem        = false;
  bool   inDesc        = false;
  bool   channelDescDone = false;  // первый <description> — это описание канала, пропускаем
  unsigned long t0 = millis();

  while (http.connected() && millis() - t0 < 12000UL) {
    int av = stream->available();
    if (!av) { delay(10); continue; }
    int toRead = min(av, 256);
    char tmp[257]; int n = stream->readBytes(tmp, toRead); tmp[n] = 0;
    buf += tmp;
    if (buf.length() > 2000) buf = buf.substring(buf.length() - 1000);

    // Ждём начала первого <item>
    if (!inItem) {
      int ip = buf.indexOf("<item>");
      if (ip >= 0) { inItem = true; buf = buf.substring(ip); }
      else continue;
    }

    // Ищем <pubDate> для даты
    if (foundDate.isEmpty()) {
      int ds = buf.indexOf("<pubDate>"); int de = buf.indexOf("</pubDate>", ds);
      if (ds >= 0 && de > ds) foundDate = buf.substring(ds + 9, de);
    }

    // Ищем <description>...</description>
    if (foundDesc.isEmpty()) {
      int ds = buf.indexOf("<description>");
      if (ds >= 0) {
        int de = buf.indexOf("</description>", ds);
        if (de > ds) {
          foundDesc = buf.substring(ds + 13, de);
        }
      }
    }

    if (!foundDesc.isEmpty() && !foundDate.isEmpty()) break;
  }
  http.end();

  Serial.printf("[TG] desc_len=%d date=%s\n", foundDesc.length(), foundDate.c_str());

  if (foundDesc.length() > 0) {
    // Декодируем HTML entities
    foundDesc.replace("&lt;",  "<");
    foundDesc.replace("&gt;",  ">");
    foundDesc.replace("&amp;", "&");
    foundDesc.replace("&quot;","\"");
    foundDesc.replace("&#39;", "'");
    foundDesc.replace("<br>",  " ");
    foundDesc.replace("<br/>", " ");

    // Убираем HTML теги
    String clean = ""; bool inTag = false;
    for (int i = 0; i < (int)foundDesc.length(); ) {
      unsigned char c = (unsigned char)foundDesc[i];
      if (c > 127) { if (!inTag) clean += (char)c; i++; continue; }
      char ch = (char)c;
      if (ch == '<') { inTag = true; i++; continue; }
      if (ch == '>') { inTag = false; i++;
        if (clean.length() > 0 && clean[clean.length()-1] != ' ') clean += ' ';
        continue; }
      if (inTag) { i++; continue; }
      if (ch == '\n') ch = ' ';
      clean += ch; i++;
    }

    // Убираем служебные фразы
    clean.replace("ПРЕДЛОЖИТЬ ПОСТ", "");
    clean.replace("ПОИСК ТОЧКИ", "");
    clean.replace("РОЗЫГРЫШ", "");
    clean.replace("БАРАХОЛКА", "");
    clean.replace("СКАЗАТЬ СПАСИБО", "");
    clean.replace("| |", "");

    // Убираем ссылки (http://... или https://...)
    int lp;
    while ((lp = clean.indexOf("http")) >= 0) {
      int le = clean.indexOf(' ', lp);
      if (le < 0) le = clean.length();
      clean = clean.substring(0, lp) + clean.substring(le);
    }

    while (clean.indexOf("  ") >= 0) clean.replace("  ", " ");
    while (clean.indexOf("| ") >= 0) clean.replace("| ", "");
    clean.trim();

    if (clean.length() > 250) clean = clean.substring(0, 250) + "...";
    strncpy(tgText, clean.c_str(), 255); tgText[255] = 0;

    // Парсим дату RSS: "Wed, 29 Apr 2026 05:42:00 +0300"
    if (foundDate.length() > 0) {
      const char* months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
      int mo = 0, d = 0, h = 0, mi = 0;
      for (int i = 0; i < 12; i++) if (foundDate.indexOf(months[i]) >= 0) { mo = i+1; break; }
      sscanf(foundDate.c_str() + 5, "%d", &d);
      sscanf(foundDate.c_str() + 17, "%d:%d", &h, &mi);
      snprintf(tgDate, sizeof(tgDate), "%02d.%02d %02d:%02d", d, mo, h, mi);
    }
    tgValid = true; tgScrollPos = 0;
    Serial.printf("[TG] OK: %s\n", tgText);
  } else {
    Serial.println("[TG] no description found");
  }
}

// ============================================================
//  ДАТЧИК — заглушка (включается в настройках)
// ============================================================
void readSensor() { /* реализуется после выбора типа в веб-интерфейсе */ }
