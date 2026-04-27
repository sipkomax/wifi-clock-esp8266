// ВАЖНО: Сохранить в кодировке UTF-8 (Arduino IDE делает это по умолчанию)
/*
 * ============================================================
 *  WiFi Часы на ESP8266 + ST7920 128x64
 *
 *  Экраны (авточередование, настраивается через веб):
 *    - Часы + дата + IP
 *    - Погода (OpenWeatherMap)
 *    - Прогноз на 3 дня
 *    - Последний пост Telegram канала (бегущая строка)
 *    - Датчик (DHT22/DHT11/SHT30 — включается в настройках)
 *
 *  Библиотеки (Library Manager):
 *    U8g2, ArduinoJson v6, NTPClient, DHT sensor library (Adafruit)
 *    ESP8266WiFi / WebServer / HTTPClient / WiFiUdp (в составе ESP8266 core)
 *
 *  Подключение ST7920 HW SPI:
 *    CS(EN) -> D8 (GPIO15)   MOSI(SID) -> D7 (GPIO13)
 *    CLK    -> D5 (GPIO14)   PSB       -> GND  (SPI режим!)
 *    VCC    -> VIN (5V)      GND       -> GND
 *    BLA    -> 3.3V через 33-100 Ом резистор
 * ============================================================
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <ArduinoOTA.h>

// ============================================================
//  СТРУКТУРА НАСТРОЕК
// ============================================================
struct Config {
  // WiFi
  char ssid[32]        = "ВАШ_WIFI";
  char pass[64]        = "ВАШ_ПАРОЛЬ";
  int  timezone        = 3;          // МСК = 3
  // Погода
  char city[32]        = "Moscow";
  char apiKey[48]      = "ВАШ_OWM_KEY";
  int  wx_interval     = 10;         // мин
  bool wx_celsius      = true;
  // Дисплей
  bool use24h          = true;
  bool showSeconds     = true;
  bool nightMode       = true;
  int  nightFrom       = 23;
  int  nightTo         = 7;
  // Экраны — включить/выключить
  bool scr_clock       = true;
  bool scr_weather     = true;
  bool scr_forecast    = true;
  bool scr_telegram    = true;
  bool scr_sensor      = false;
  // Время показа каждого экрана (сек)
  int  time_clock      = 10;
  int  time_weather    = 5;
  int  time_forecast   = 5;
  int  time_telegram   = 15;
  int  time_sensor     = 5;
  // Telegram
  char tg_channel[33]  = "rf4map";
  int  tg_interval     = 5;          // мин
  int  tg_scroll       = 80;         // мс между шагами прокрутки
  // Датчик
  int  sensor_type     = 0;          // 0=нет 1=DHT22 2=DHT11 3=SHT30
  int  sensor_pin      = 2;          // GPIO (для DHT)
  int  tempOffset      = 0;          // коррекция * 10
  // Маркер
  char magic[4]        = "CF2";
};

Config cfg;

// ============================================================
//  ОБЪЕКТЫ
// ============================================================
U8G2_ST7920_128X64_F_HW_SPI u8g2(U8G2_R0, /*CS=*/D8, /*reset=*/U8X8_PIN_NONE);
WiFiUDP      ntpUDP;
NTPClient    timeClient(ntpUDP, "ru.pool.ntp.org", 0, 60000);
ESP8266WebServer server(80);
WiFiClient   wifiClient;

// ============================================================
//  ПОГОДА
// ============================================================
struct Weather {
  float temp       = 0;
  float feels_like = 0;
  float humid      = 0;
  float wind       = 0;
  char  desc[32]   = "---";
  char  icon[4]    = "01d";
  bool  valid      = false;
};
Weather wx;

struct ForecastDay {
  char  day[4]   = "---";
  float tempMin  = 0;
  float tempMax  = 0;
  char  icon[4]  = "01d";
};
ForecastDay forecast[3];
bool forecastValid = false;

// ============================================================
//  TELEGRAM
// ============================================================
char          tgText[256]    = "Загрузка...";
char          tgDate[16]     = "";
bool          tgValid        = false;
unsigned long lastTgUpdate   = 0;
int           tgScrollPos    = 0;
unsigned long tgScrollTimer  = 0;

// ============================================================
//  ДАТЧИК
// ============================================================
float sensorTemp = NAN;
float sensorHum  = NAN;

// ============================================================
//  ЭКРАНЫ
// ============================================================
enum Screen {
  SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_FORECAST,
  SCREEN_TELEGRAM, SCREEN_SENSOR, SCREEN_IP
};
Screen        currentScreen = SCREEN_CLOCK;
unsigned long screenTimer   = 0;

// ============================================================
//  ТАЙМЕРЫ
// ============================================================
unsigned long lastWeatherUpdate = 0;
unsigned long lastSensorRead    = 0;
unsigned long lastNTPSync       = 0;
const unsigned long NTP_INTERVAL = 3600000UL;
bool wifiConnected = false;

// ============================================================
//  XBM ИКОНКИ 16x16
// ============================================================
static const uint8_t PROGMEM ico_sun[]    = {0x80,0x01,0x80,0x01,0x10,0x08,0x08,0x10,0xf4,0x2f,0xfe,0x7f,0x07,0xe0,0x03,0xc0,0x03,0xc0,0x07,0xe0,0xfe,0x7f,0xf4,0x2f,0x08,0x10,0x10,0x08,0x80,0x01,0x80,0x01};
static const uint8_t PROGMEM ico_cloud[]  = {0x00,0x00,0x00,0x00,0xc0,0x03,0x30,0x0c,0x18,0x18,0x08,0x10,0xfc,0x3f,0xfe,0x7f,0xff,0xff,0xff,0xff,0xfe,0x7f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_rain[]   = {0x00,0x00,0xc0,0x03,0x30,0x0c,0x18,0x18,0xfc,0x3f,0xfe,0x7f,0xff,0xff,0xfe,0x7f,0x00,0x00,0x22,0x44,0x22,0x44,0x11,0x88,0x11,0x88,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_snow[]   = {0x80,0x01,0x80,0x01,0xa0,0x05,0xd0,0x0b,0xe8,0x17,0xfc,0x3f,0x87,0xe1,0x81,0x81,0x81,0x81,0x87,0xe1,0xfc,0x3f,0xe8,0x17,0xd0,0x0b,0xa0,0x05,0x80,0x01,0x80,0x01};
static const uint8_t PROGMEM ico_storm[]  = {0x00,0x00,0xc0,0x03,0x30,0x0c,0xfc,0x3f,0xfe,0x7f,0xff,0xff,0xfe,0x7f,0x00,0x00,0xe0,0x01,0x70,0x00,0x38,0x00,0xfc,0x01,0xfe,0x01,0x07,0x00,0x03,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_fog[]    = {0x00,0x00,0x00,0x00,0xf8,0x1f,0x00,0x00,0xfc,0x3f,0x00,0x00,0xfe,0x7f,0x00,0x00,0xfc,0x3f,0x00,0x00,0xf8,0x1f,0x00,0x00,0xf0,0x0f,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_partly[] = {0x80,0x00,0xc0,0x01,0x40,0x05,0x00,0x03,0xe0,0x03,0x10,0x06,0xf8,0x0c,0xfc,0x1f,0xfe,0x3f,0xff,0x3f,0xfe,0x1f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

const uint8_t* getWeatherIcon(const char* code) {
  char c = code[0], n = code[1];
  if (c=='0'&&n=='1') return ico_sun;
  if (c=='0'&&n=='2') return ico_partly;
  if (c=='0'&&(n=='3'||n=='4')) return ico_cloud;
  if ((c=='0'&&n=='9')||(c=='1'&&n=='0')) return ico_rain;
  if (c=='1'&&n=='1') return ico_storm;
  if (c=='1'&&n=='3') return ico_snow;
  if (c=='5'&&n=='0') return ico_fog;
  return ico_cloud;
}

// ============================================================
//  EEPROM
// ============================================================
void saveConfig() {
  EEPROM.begin(sizeof(Config));
  EEPROM.put(0, cfg);
  EEPROM.commit();
  EEPROM.end();
}
void loadConfig() {
  EEPROM.begin(sizeof(Config));
  Config tmp;
  EEPROM.get(0, tmp);
  EEPROM.end();
  if (strcmp(tmp.magic, "CF2") == 0) cfg = tmp;
}

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

// ============================================================
//  ДИСПЛЕЙ — ЧАСЫ
// ============================================================
void drawClock() {
  time_t t = timeClient.getEpochTime() + cfg.timezone * 3600L;
  struct tm* ti = gmtime(&t);
  char timeBuf[9], dateBuf[20];
  if (cfg.showSeconds)
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d:%02d", ti->tm_hour, ti->tm_min, ti->tm_sec);
  else
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", ti->tm_hour, ti->tm_min);
  const char* mo[] = {"Янв","Фев","Мар","Апр","Май","Июн","Июл","Авг","Сен","Окт","Ноя","Дек"};
  const char* wd[] = {"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};
  snprintf(dateBuf, sizeof(dateBuf), "%s %d %s %d", wd[ti->tm_wday], ti->tm_mday, mo[ti->tm_mon], 1900+ti->tm_year);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(2, 10, dateBuf);
  u8g2.drawHLine(0, 13, 128);
  if (cfg.showSeconds) {
    u8g2.setFont(u8g2_font_logisoso24_tf);
    u8g2.drawUTF8(4, 44, timeBuf);
  } else {
    u8g2.setFont(u8g2_font_logisoso32_tf);
    int tw = u8g2.getStrWidth(timeBuf);
    u8g2.drawUTF8((128-tw)/2, 50, timeBuf);
  }
  u8g2.drawHLine(0, 51, 128);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(2, 62, wifiConnected ? WiFi.localIP().toString().c_str() : "AP: 192.168.4.1");
  u8g2.sendBuffer();
}

// ============================================================
//  ДИСПЛЕЙ — ПОГОДА
// ============================================================
void drawWeather() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(0, 9, "Погода:");
  u8g2.drawUTF8(50, 9, cfg.city);
  u8g2.drawHLine(0, 12, 128);
  if (!wx.valid) { u8g2.drawUTF8(10,40,"Нет данных"); u8g2.sendBuffer(); return; }
  u8g2.drawXBMP(0, 15, 16, 16, getWeatherIcon(wx.icon));
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(20, 25, wx.desc);
  u8g2.setFont(u8g2_font_logisoso24_tf);
  char tBuf[8];
  snprintf(tBuf, sizeof(tBuf), "%.0f`", wx.temp);
  u8g2.drawUTF8(0, 52, tBuf);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  char buf[24];
  snprintf(buf, sizeof(buf), "Ощущ: %.0f`C", wx.feels_like); u8g2.drawUTF8(52, 36, buf);
  snprintf(buf, sizeof(buf), "Влаж: %.0f%%",  wx.humid);     u8g2.drawUTF8(52, 47, buf);
  snprintf(buf, sizeof(buf), "Ветер:%.1fm/s", wx.wind);      u8g2.drawUTF8(52, 58, buf);
  u8g2.sendBuffer();
}

// ============================================================
//  ДИСПЛЕЙ — ПРОГНОЗ 3 ДНЯ
// ============================================================
void drawForecast() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  u8g2.drawUTF8(0, 10, "Прогноз:");
  u8g2.drawHLine(0, 13, 128);
  if (!forecastValid) { u8g2.drawUTF8(10,40,"Нет данных"); u8g2.sendBuffer(); return; }
  for (int i = 0; i < 3; i++) {
    int x = i * 43;
    u8g2.drawXBMP(x+13, 16, 16, 16, getWeatherIcon(forecast[i].icon));
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(x+13, 14, forecast[i].day);
    char buf[8];
    snprintf(buf, sizeof(buf), "%.0f`", forecast[i].tempMax); u8g2.drawUTF8(x+2,  46, buf);
    snprintf(buf, sizeof(buf), "%.0f`", forecast[i].tempMin); u8g2.drawUTF8(x+22, 56, buf);
    if (i < 2) u8g2.drawVLine(x+42, 14, 50);
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
  u8g2.setFont(u8g2_font_logisoso24_tf);
  char buf[10];
  snprintf(buf, sizeof(buf), "%.1f`", sensorTemp);
  u8g2.drawUTF8(0, 50, buf);
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  snprintf(buf, sizeof(buf), "%.0f%%", sensorHum);
  u8g2.drawUTF8(80, 50, buf);
  u8g2.sendBuffer();
}

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
    u8g2.drawStr(128-55, 10, tgDate);
  }
  u8g2.drawHLine(0, 13, 128);

  if (!tgValid) {
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(10, 40, "Нет данных");
    u8g2.sendBuffer(); return;
  }

  const int CHARS_PER_LINE = 21;
  const int LINES = 3;
  const int Y_START = 26;
  const int LINE_H  = 14;

  int startByte = utf8ByteOffset(tgText, tgScrollPos);
  const char* ptr = tgText + startByte;
  u8g2.setFont(u8g2_font_6x13_t_cyrillic);
  for (int line = 0; line < LINES && *ptr; line++) {
    char lineBuf[70] = {0};
    int bc = 0, cc = 0;
    const char* p = ptr;
    while (*p && cc < CHARS_PER_LINE) {
      int cl = utf8CharLen(p);
      if (!cl) break;
      memcpy(lineBuf+bc, p, cl);
      bc += cl; cc++; p += cl;
    }
    lineBuf[bc] = 0;
    ptr = p;
    u8g2.drawUTF8(0, Y_START + line * LINE_H, lineBuf);
  }

  // Прогресс-бар
  int total = utf8Strlen(tgText);
  int maxSc = max(0, total - CHARS_PER_LINE);
  if (maxSc > 0) {
    int bw = map(tgScrollPos, 0, maxSc, 0, 128);
    u8g2.drawHLine(0, 63, bw);
  }
  u8g2.sendBuffer();

  // Прокрутка
  unsigned long now = millis();
  if (now - tgScrollTimer > (unsigned long)cfg.tg_scroll) {
    int maxScroll = max(0, total - CHARS_PER_LINE);
    if (tgScrollPos < maxScroll) tgScrollPos++;
    tgScrollTimer = now;
  }
}

// ============================================================
//  АВТОЧЕРЕДОВАНИЕ ЭКРАНОВ
// ============================================================
Screen nextActiveScreen(Screen from) {
  const Screen order[]  = {SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_FORECAST, SCREEN_TELEGRAM, SCREEN_SENSOR};
  const bool* enabled[] = {&cfg.scr_clock, &cfg.scr_weather, &cfg.scr_forecast, &cfg.scr_telegram, &cfg.scr_sensor};
  const int n = 5;
  int startIdx = 0;
  for (int i = 0; i < n; i++) if (order[i] == from) { startIdx = i; break; }
  for (int i = 1; i <= n; i++) {
    int idx = (startIdx + i) % n;
    if (*enabled[idx]) return order[idx];
  }
  return SCREEN_CLOCK;
}

void handleScreenCycle() {
  unsigned long now     = millis();
  unsigned long elapsed = now - screenTimer;
  unsigned long limits[5] = {
    (unsigned long)cfg.time_clock    * 1000UL,
    (unsigned long)cfg.time_weather  * 1000UL,
    (unsigned long)cfg.time_forecast * 1000UL,
    (unsigned long)cfg.time_telegram * 1000UL,
    (unsigned long)cfg.time_sensor   * 1000UL,
  };
  const Screen order[5] = {SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_FORECAST, SCREEN_TELEGRAM, SCREEN_SENSOR};
  for (int i = 0; i < 5; i++) {
    if (currentScreen == order[i] && elapsed > limits[i]) {
      currentScreen = nextActiveScreen(order[i]);
      screenTimer   = now;
      if (currentScreen == SCREEN_TELEGRAM) tgScrollPos = 0;
      break;
    }
  }
}

// ============================================================
//  HTTP — ПОГОДА
// ============================================================
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
    if (!deserializeJson(doc, http.getString())) {
      wx.temp       = doc["main"]["temp"]       | 0.0f;
      wx.feels_like = doc["main"]["feels_like"] | 0.0f;
      wx.humid      = doc["main"]["humidity"]   | 0.0f;
      wx.wind       = doc["wind"]["speed"]      | 0.0f;
      const char* d = doc["weather"][0]["description"] | "---";
      const char* ic= doc["weather"][0]["icon"]        | "01d";
      strncpy(wx.desc, d,  31); wx.desc[31] = 0;
      strncpy(wx.icon, ic,  3); wx.icon[3]  = 0;
      wx.valid = true;
    }
  }
  http.end();
}

// ============================================================
//  HTTP — ПРОГНОЗ
// ============================================================
void fetchForecast() {
  if (!wifiConnected || strlen(cfg.apiKey) < 10) return;
  String url = "http://api.openweathermap.org/data/2.5/forecast?";
  bool isId = true;
  for (int i = 0; i < (int)strlen(cfg.city); i++) if (!isDigit(cfg.city[i])) { isId = false; break; }
  url += isId ? "id=" : "q=";
  url += cfg.city;
  url += "&appid=" + String(cfg.apiKey) + "&units=" + (cfg.wx_celsius ? "metric" : "imperial") + "&lang=ru&cnt=24";
  HTTPClient http; http.begin(wifiClient, url);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(4096);
    if (!deserializeJson(doc, http.getString())) {
      const char* wdn[] = {"Вс","Пн","Вт","Ср","Чт","Пт","Сб"};
      for (int i = 0; i < 3; i++) { forecast[i].tempMin=99; forecast[i].tempMax=-99; }
      time_t now_t = timeClient.getEpochTime() + cfg.timezone * 3600L;
      int todayMday = gmtime(&now_t)->tm_mday;
      int dayIdx = -1; int lastMday = -1;
      for (JsonObject item : doc["list"].as<JsonArray>()) {
        long dt = (item["dt"]|0L) + cfg.timezone*3600L;
        struct tm* ti = gmtime((time_t*)&dt);
        if (ti->tm_mday == todayMday) continue;
        if (ti->tm_mday != lastMday) {
          dayIdx++; lastMday = ti->tm_mday;
          if (dayIdx >= 3) break;
          strncpy(forecast[dayIdx].day, wdn[ti->tm_wday], 3); forecast[dayIdx].day[3]=0;
          const char* ic = item["weather"][0]["icon"]|"01d";
          strncpy(forecast[dayIdx].icon, ic, 3); forecast[dayIdx].icon[3]=0;
        }
        if (dayIdx < 0 || dayIdx >= 3) continue;
        float t = item["main"]["temp"]|0.0f;
        if (t < forecast[dayIdx].tempMin) forecast[dayIdx].tempMin = t;
        if (t > forecast[dayIdx].tempMax) forecast[dayIdx].tempMax = t;
      }
      forecastValid = (dayIdx >= 0);
    }
  }
  http.end();
}

// ============================================================
//  HTTP — TELEGRAM
// ============================================================
void fetchTelegram() {
  if (!wifiConnected) return;
  String url = "http://tg.i-c-a.su/json/";
  url += cfg.tg_channel;
  url += "/10";
  HTTPClient http; http.begin(wifiClient, url); http.setTimeout(8000);
  if (http.GET() == 200) {
    DynamicJsonDocument doc(12288);
    StaticJsonDocument<256> filter;
    for (int fi = 0; fi < 10; fi++) {
      filter["messages"][fi]["message"] = true;
      filter["messages"][fi]["date"]    = true;
    }
    DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
    if (!err) {
      String foundText = ""; long foundDate = 0;
      for (JsonObject m : doc["messages"].as<JsonArray>()) {
        const char* txt = m["message"]|"";
        if (strlen(txt) > 3) { foundText = txt; foundDate = m["date"]|0L; break; }
      }
      if (foundText.length() > 0) {
        // Чистим хэштеги и переносы
        String clean = ""; bool skip = false;
        for (int i = 0; i < (int)foundText.length(); i++) {
          char c = foundText[i];
          if (c == '#') { skip = true; continue; }
          if (skip && (c == ' ' || c == '\n')) skip = false;
          if (skip) continue;
          if (c == '\n') c = ' ';
          clean += c;
        }
        clean.trim();
        if (clean.length() > 200) clean = clean.substring(0, 200) + "...";
        strncpy(tgText, clean.c_str(), 255); tgText[255] = 0;
        if (foundDate > 0) {
          foundDate += cfg.timezone * 3600L;
          struct tm* ti = gmtime((time_t*)&foundDate);
          snprintf(tgDate, sizeof(tgDate), "%02d.%02d %02d:%02d", ti->tm_mday, ti->tm_mon+1, ti->tm_hour, ti->tm_min);
        }
        tgValid = true; tgScrollPos = 0;
      }
    }
  }
  http.end();
}

// ============================================================
//  ДАТЧИК — заглушка (включается в настройках)
// ============================================================
void readSensor() { /* реализуется после выбора типа в веб-интерфейсе */ }

// ============================================================
//  ВЕБ-ИНТЕРФЕЙС — HTML
// ============================================================
const char HTML_PAGE[] PROGMEM = R"HTMLEOF(<!DOCTYPE html>
<html lang="ru"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>WiFi Clock</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:'Segoe UI',sans-serif;background:#0f172a;color:#e2e8f0;padding:12px}
.wrap{max-width:560px;margin:0 auto}
h1{text-align:center;color:#38bdf8;padding:14px 0 6px;font-size:1.35rem}
h1 small{display:block;color:#475569;font-size:.8rem;margin-top:3px}
.tabs{display:flex;flex-wrap:wrap;gap:4px;margin-bottom:14px}
.tab{padding:7px 13px;border-radius:6px;border:none;background:#1e293b;color:#94a3b8;cursor:pointer;font-size:.82rem;transition:.18s}
.tab.on{background:#0369a1;color:#fff}
.tab:hover:not(.on){background:#334155;color:#e2e8f0}
.sec{display:none}.sec.show{display:block}
.card{background:#1e293b;border-radius:10px;padding:14px;margin-bottom:12px}
.card h3{color:#38bdf8;font-size:.9rem;margin-bottom:10px;padding-bottom:6px;border-bottom:1px solid #334155}
label{display:block;color:#94a3b8;font-size:.78rem;margin:8px 0 3px}
input,select{width:100%;padding:7px 9px;background:#0f172a;color:#e2e8f0;border:1px solid #334155;border-radius:6px;font-size:.88rem;outline:none}
input:focus,select:focus{border-color:#38bdf8}
.g2{display:grid;grid-template-columns:1fr 1fr;gap:10px}
.g3{display:grid;grid-template-columns:1fr 1fr 1fr;gap:8px}
.chk{display:flex;align-items:center;gap:9px;margin:8px 0}
.chk input[type=checkbox]{width:17px;height:17px;accent-color:#38bdf8;cursor:pointer;flex-shrink:0}
.chk span{color:#cbd5e1;font-size:.88rem}
.srow{display:flex;align-items:center;justify-content:space-between;padding:7px 0;border-bottom:1px solid #1a2540}
.srow:last-child{border-bottom:none}
.sn{font-size:.88rem;color:#cbd5e1;display:flex;align-items:center;gap:8px}
.sn input[type=checkbox]{width:16px;height:16px;accent-color:#38bdf8;cursor:pointer}
.st{display:flex;align-items:center;gap:5px}
.st input{width:54px;text-align:center;padding:5px}
.st span{font-size:.78rem;color:#64748b}
.btn{width:100%;padding:10px;border:none;border-radius:7px;font-weight:600;font-size:.9rem;cursor:pointer;margin-top:9px;transition:.18s}
.bp{background:#0369a1;color:#fff}.bp:hover{background:#0284c7}
.bd{background:#7f1d1d;color:#fca5a5}.bd:hover{background:#991b1b}
.bi{background:#164e63;color:#67e8f9}.bi:hover{background:#155e75}
.stat-g{display:grid;grid-template-columns:1fr 1fr;gap:8px}
.sb{background:#0f172a;border-radius:8px;padding:10px;text-align:center}
.sv{font-size:1.3rem;font-weight:700;color:#38bdf8}
.sl{font-size:.72rem;color:#64748b;margin-top:2px}
.hint{font-size:.75rem;color:#475569;margin-top:8px;line-height:1.5}
.ok{background:#14532d;color:#86efac;padding:9px;border-radius:6px;text-align:center;margin-top:10px;font-size:.88rem}
.tg-pre{font-size:.83rem;color:#94a3b8;word-break:break-word;line-height:1.5}
.tg-dt{font-size:.73rem;color:#475569;margin-top:5px}
a{color:#38bdf8}
</style></head>
<body><div class="wrap">
<h1>&#9200; WiFi Clock <small>TPLC_IP</small></h1>
<div class="tabs">
  <button class="tab on" onclick="tab(this,'tw')">&#127760; WiFi</button>
  <button class="tab" onclick="tab(this,'tW')">&#127780; Погода</button>
  <button class="tab" onclick="tab(this,'ts')">&#128250; Экраны</button>
  <button class="tab" onclick="tab(this,'tg')">&#128240; Telegram</button>
  <button class="tab" onclick="tab(this,'td')">&#127777; Датчик</button>
  <button class="tab" onclick="tab(this,'ti')">&#128200; Статус</button>
</div>
<form action="/save" method="post">

<div id="tw" class="sec show">
<div class="card"><h3>&#127760; WiFi</h3>
  <label>SSID (имя сети)</label><input name="ssid" value="TPLC_SSID" maxlength="31">
  <label>Пароль</label><input name="pass" type="password" value="TPLC_PASS" maxlength="63">
  <label>Часовой пояс (МСК = 3)</label><input name="tz" type="number" value="TPLC_TZ" min="-12" max="14">
</div></div>

<div id="tW" class="sec">
<div class="card"><h3>&#127780; OpenWeatherMap</h3>
  <label>API Key (<a href="https://openweathermap.org/api" target="_blank">получить бесплатно</a>)</label>
  <input name="api" value="TPLC_API" maxlength="47">
  <label>Город (латиницей или ID)</label>
  <input name="city" value="TPLC_CITY" maxlength="31" placeholder="Moscow">
  <div class="g2">
    <div><label>Обновление (мин)</label><input name="wx_interval" type="number" value="TPLC_WXINT" min="1" max="60"></div>
    <div><label>Единицы</label>
      <select name="wx_unit">
        <option value="1" TPLC_WXCEL>&#176;C Цельсий</option>
        <option value="0" TPLC_WXFAR>&#176;F Фаренгейт</option>
      </select>
    </div>
  </div>
</div></div>

<div id="ts" class="sec">
<div class="card"><h3>&#128250; Экраны — порядок и время</h3>
  <p class="hint">&#10003; галка = экран включён &nbsp;|&nbsp; число = сколько секунд показывать</p>
  <div style="margin-top:10px">
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_clock" TPLC_SCLOCK> &#9200; Часы</div><div class="st"><input name="time_clock" type="number" value="TPLC_TCLOCK" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_weather" TPLC_SWEATHER> &#127780; Погода</div><div class="st"><input name="time_weather" type="number" value="TPLC_TWEATHER" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_forecast" TPLC_SFORECAST> &#128197; Прогноз</div><div class="st"><input name="time_forecast" type="number" value="TPLC_TFORECAST" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_telegram" TPLC_STELEGRAM> &#128240; Telegram</div><div class="st"><input name="time_telegram" type="number" value="TPLC_TTELEGRAM" min="3" max="300"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_sensor" TPLC_SSENSOR> &#127777; Датчик</div><div class="st"><input name="time_sensor" type="number" value="TPLC_TSENSOR" min="1" max="120"><span>сек</span></div></div>
  </div>
</div>
<div class="card"><h3>&#9881; Дисплей</h3>
  <div class="chk"><input type="checkbox" name="use24h" TPLC_24H><span>24-часовой формат</span></div>
  <div class="chk"><input type="checkbox" name="sec" TPLC_SEC><span>Показывать секунды</span></div>
  <div class="chk"><input type="checkbox" name="night" TPLC_NIGHT><span>Ночной режим (снизить яркость)</span></div>
  <div class="g2" style="margin-top:6px">
    <div><label>Начало ночи (ч)</label><input name="nfrom" type="number" value="TPLC_NFROM" min="0" max="23"></div>
    <div><label>Конец ночи (ч)</label><input name="nto" type="number" value="TPLC_NTO" min="0" max="23"></div>
  </div>
</div></div>

<div id="tg" class="sec">
<div class="card"><h3>&#128240; Telegram канал</h3>
  <label>Username (без @)</label>
  <input name="tg_channel" value="TPLC_TGCH" maxlength="32" placeholder="rf4map">
  <div class="g2">
    <div><label>Обновление (мин)</label><input name="tg_interval" type="number" value="TPLC_TGINT" min="1" max="60"></div>
    <div><label>Скорость прокрутки (мс)</label><input name="tg_scroll" type="number" value="TPLC_TGSCR" min="30" max="500"></div>
  </div>
  <p class="hint">Только публичные каналы. Меньше мс = быстрее прокрутка (рекомендуется 60–120).</p>
</div>
<div class="card"><h3>&#128202; Последний пост</h3>
  <div class="tg-pre">TPLC_TGPRE</div>
  <div class="tg-dt">TPLC_TGDAT</div>
</div></div>

<div id="td" class="sec">
<div class="card"><h3>&#127777; Датчик температуры</h3>
  <label>Тип датчика</label>
  <select name="sensor_type">
    <option value="0" TPLC_SNS0>Нет датчика</option>
    <option value="1" TPLC_SNS1>DHT22 / AM2302</option>
    <option value="2" TPLC_SNS2>DHT11</option>
    <option value="3" TPLC_SNS3>SHT30 (I2C)</option>
  </select>
  <label>GPIO пин (для DHT; SHT30 — I2C: SDA=D2, SCL=D1)</label>
  <input name="sensor_pin" type="number" value="TPLC_SPIN" min="0" max="16">
  <label>Коррекция температуры (&#176;C)</label>
  <input name="toff" type="number" value="TPLC_TOFF" min="-50" max="50">
  <p class="hint">После выбора типа и пина — сохранить и перезагрузить. Экран датчика включается выше (вкладка Экраны).</p>
</div>
<div class="card"><h3>&#128202; Показания</h3>
  <div class="stat-g">
    <div class="sb"><div class="sv">TPLC_STEMP&#176;</div><div class="sl">Температура</div></div>
    <div class="sb"><div class="sv">TPLC_SHUM%</div><div class="sl">Влажность</div></div>
  </div>
</div></div>

<div id="ti" class="sec">
<div class="card"><h3>&#128200; Система</h3>
  <div style="background:#0c4a6e;color:#7dd3fc;padding:9px 12px;border-radius:7px;font-size:.82rem;margin-bottom:10px">
    &#128225; OTA: <b>wifi-clock.local</b> &nbsp;|&nbsp; Пароль: <b>ota1234</b><br>
    Arduino IDE: Инструменты → Порт → wifi-clock.local
  </div>
  <div class="stat-g">
    <div class="sb"><div class="sv" style="font-size:.95rem">TPLC_IP</div><div class="sl">IP адрес</div></div>
    <div class="sb"><div class="sv">TPLC_RSSI</div><div class="sl">WiFi (dBm)</div></div>
    <div class="sb"><div class="sv">TPLC_HEAP</div><div class="sl">Free RAM</div></div>
    <div class="sb"><div class="sv" style="font-size:1rem">TPLC_UPTIME</div><div class="sl">Uptime</div></div>
  </div>
</div>
<div class="card"><h3>&#127780; Погода сейчас</h3>
  <div class="stat-g">
    <div class="sb"><div class="sv">TPLC_WXTEMP&#176;</div><div class="sl">Температура</div></div>
    <div class="sb"><div class="sv" style="font-size:.9rem">TPLC_WXDESC</div><div class="sl">Описание</div></div>
    <div class="sb"><div class="sv">TPLC_WXHUM%</div><div class="sl">Влажность</div></div>
    <div class="sb"><div class="sv">TPLC_WXWIND</div><div class="sl">Ветер м/с</div></div>
  </div>
</div></div>

<button type="submit" class="btn bp">&#128190; Сохранить настройки</button>
</form>
<form action="/refresh" method="post">
  <button type="submit" class="btn bi">&#8635; Обновить погоду и Telegram</button>
</form>
<form action="/reboot" method="post">
  <button type="submit" class="btn bd">&#128260; Перезагрузить устройство</button>
</form>
TPLC_STATUS
</div>
<script>
function tab(el,id){
  document.querySelectorAll('.tab').forEach(t=>t.classList.remove('on'));
  document.querySelectorAll('.sec').forEach(s=>s.classList.remove('show'));
  el.classList.add('on');
  document.getElementById(id).classList.add('show');
}
</script>
</body></html>)HTMLEOF";

// ============================================================
//  buildPage() — подставляет все значения
// ============================================================
String buildPage(const char* status = "") {
  String h = FPSTR(HTML_PAGE);
  char buf[32];

  // WiFi
  h.replace("TPLC_SSID", cfg.ssid);
  h.replace("TPLC_PASS", cfg.pass);
  h.replace("TPLC_TZ",   String(cfg.timezone));
  // Погода
  h.replace("TPLC_API",   cfg.apiKey);
  h.replace("TPLC_CITY",  cfg.city);
  h.replace("TPLC_WXINT", String(cfg.wx_interval));
  h.replace("TPLC_WXCEL", cfg.wx_celsius ? "selected" : "");
  h.replace("TPLC_WXFAR", cfg.wx_celsius ? "" : "selected");
  // Экраны
  h.replace("TPLC_SCLOCK",    cfg.scr_clock    ? "checked" : "");
  h.replace("TPLC_SWEATHER",  cfg.scr_weather  ? "checked" : "");
  h.replace("TPLC_SFORECAST", cfg.scr_forecast ? "checked" : "");
  h.replace("TPLC_STELEGRAM", cfg.scr_telegram ? "checked" : "");
  h.replace("TPLC_SSENSOR",   cfg.scr_sensor   ? "checked" : "");
  h.replace("TPLC_TCLOCK",    String(cfg.time_clock));
  h.replace("TPLC_TWEATHER",  String(cfg.time_weather));
  h.replace("TPLC_TFORECAST", String(cfg.time_forecast));
  h.replace("TPLC_TTELEGRAM", String(cfg.time_telegram));
  h.replace("TPLC_TSENSOR",   String(cfg.time_sensor));
  // Дисплей
  h.replace("TPLC_24H",   cfg.use24h      ? "checked" : "");
  h.replace("TPLC_SEC",   cfg.showSeconds ? "checked" : "");
  h.replace("TPLC_NIGHT", cfg.nightMode   ? "checked" : "");
  h.replace("TPLC_NFROM", String(cfg.nightFrom));
  h.replace("TPLC_NTO",   String(cfg.nightTo));
  // Telegram
  h.replace("TPLC_TGCH",  cfg.tg_channel);
  h.replace("TPLC_TGINT", String(cfg.tg_interval));
  h.replace("TPLC_TGSCR", String(cfg.tg_scroll));
  if (tgValid && strlen(tgText) > 0) {
    String pre = String(tgText).substring(0, 150);
    if (strlen(tgText) > 150) pre += "...";
    h.replace("TPLC_TGPRE", pre);
    h.replace("TPLC_TGDAT", tgDate);
  } else {
    h.replace("TPLC_TGPRE", "Ещё не загружено");
    h.replace("TPLC_TGDAT", "");
  }
  // Датчик
  h.replace("TPLC_SNS0", cfg.sensor_type==0 ? "selected":"");
  h.replace("TPLC_SNS1", cfg.sensor_type==1 ? "selected":"");
  h.replace("TPLC_SNS2", cfg.sensor_type==2 ? "selected":"");
  h.replace("TPLC_SNS3", cfg.sensor_type==3 ? "selected":"");
  h.replace("TPLC_SPIN", String(cfg.sensor_pin));
  h.replace("TPLC_TOFF", String(cfg.tempOffset / 10));
  if (!isnan(sensorTemp)) {
    snprintf(buf,sizeof(buf),"%.1f",sensorTemp); h.replace("TPLC_STEMP",buf);
    snprintf(buf,sizeof(buf),"%.0f",sensorHum);  h.replace("TPLC_SHUM", buf);
  } else { h.replace("TPLC_STEMP","--"); h.replace("TPLC_SHUM","--"); }
  // Погода
  if (wx.valid) {
    snprintf(buf,sizeof(buf),"%.1f",wx.temp);  h.replace("TPLC_WXTEMP",buf);
    h.replace("TPLC_WXDESC", wx.desc);
    snprintf(buf,sizeof(buf),"%.0f",wx.humid); h.replace("TPLC_WXHUM",buf);
    snprintf(buf,sizeof(buf),"%.1f",wx.wind);  h.replace("TPLC_WXWIND",buf);
  } else { h.replace("TPLC_WXTEMP","--"); h.replace("TPLC_WXDESC","нет данных"); h.replace("TPLC_WXHUM","--"); h.replace("TPLC_WXWIND","--"); }
  // Система
  String ip = wifiConnected ? WiFi.localIP().toString() : "192.168.4.1";
  h.replace("TPLC_IP",   ip);
  h.replace("TPLC_RSSI", wifiConnected ? String(WiFi.RSSI()) : "--");
  h.replace("TPLC_HEAP", String(ESP.getFreeHeap()));
  unsigned long sec = millis()/1000; unsigned long m=sec/60; sec%=60; unsigned long hh=m/60; m%=60; unsigned long d=hh/24; hh%=24;
  char up[20];
  if (d>0) snprintf(up,sizeof(up),"%lud %02lu:%02lu",d,hh,m);
  else     snprintf(up,sizeof(up),"%02lu:%02lu:%02lu",hh,m,sec);
  h.replace("TPLC_UPTIME", up);
  // Статус
  if (strlen(status)>0) h.replace("TPLC_STATUS", String("<div class='ok'>")+status+"</div>");
  else                  h.replace("TPLC_STATUS", "");
  return h;
}

// ============================================================
//  ВЕБ-СЕРВЕР
// ============================================================
// ============================================================
//  OTA — обновление прошивки по WiFi
// ============================================================
void setupOTA() {
  ArduinoOTA.setHostname("wifi-clock");   // mDNS имя: wifi-clock.local
  ArduinoOTA.setPassword("ota1234");       // пароль для загрузки (измени!)

  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "прошивку" : "файловую систему";
    Serial.println("OTA: начало загрузки (" + type + ")");
    // Показываем на дисплее
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 12, "OTA обновление");
    u8g2.drawHLine(0, 15, 128);
    u8g2.drawUTF8(0, 32, "Загрузка...");
    u8g2.drawFrame(0, 40, 128, 12);
    u8g2.sendBuffer();
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA: завершено!");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 12, "OTA обновление");
    u8g2.drawHLine(0, 15, 128);
    u8g2.drawUTF8(0, 35, "Готово!");
    u8g2.drawUTF8(0, 52, "Перезагрузка...");
    u8g2.sendBuffer();
    delay(800);
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = progress * 100 / total;
    Serial.printf("OTA: %d%%\n", pct);
    // Обновляем прогресс-бар на дисплее
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 12, "OTA обновление");
    u8g2.drawHLine(0, 15, 128);
    char buf[20];
    snprintf(buf, sizeof(buf), "Прогресс: %d%%", pct);
    u8g2.drawUTF8(0, 32, buf);
    u8g2.drawFrame(0, 40, 128, 12);
    u8g2.drawBox(0, 40, map(pct, 0, 100, 0, 128), 12);
    u8g2.sendBuffer();
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("OTA ошибка [%u]: ", error);
    const char* msg = "Неизвестная ошибка";
    if      (error == OTA_AUTH_ERROR)    msg = "Ошибка пароля";
    else if (error == OTA_BEGIN_ERROR)   msg = "Ошибка начала";
    else if (error == OTA_CONNECT_ERROR) msg = "Ошибка соединения";
    else if (error == OTA_RECEIVE_ERROR) msg = "Ошибка приёма";
    else if (error == OTA_END_ERROR)     msg = "Ошибка завершения";
    Serial.println(msg);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x13_t_cyrillic);
    u8g2.drawUTF8(0, 12, "OTA ОШИБКА");
    u8g2.drawHLine(0, 15, 128);
    u8g2.drawUTF8(0, 35, msg);
    u8g2.sendBuffer();
    delay(3000);
  });

  ArduinoOTA.begin();
  Serial.println("OTA готов. Хост: wifi-clock.local");
}

void setupWebServer() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html; charset=utf-8", buildPage());
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid"))  strncpy(cfg.ssid,   server.arg("ssid").c_str(),  31);
    if (server.hasArg("pass"))  strncpy(cfg.pass,   server.arg("pass").c_str(),  63);
    if (server.hasArg("tz"))    cfg.timezone    = server.arg("tz").toInt();
    if (server.hasArg("api"))   strncpy(cfg.apiKey, server.arg("api").c_str(),   47);
    if (server.hasArg("city"))  strncpy(cfg.city,   server.arg("city").c_str(),  31);
    if (server.hasArg("wx_interval")) cfg.wx_interval = constrain(server.arg("wx_interval").toInt(),1,60);
    cfg.wx_celsius  = server.hasArg("wx_unit") ? (server.arg("wx_unit").toInt()==1) : true;
    // Экраны
    cfg.scr_clock    = server.hasArg("scr_clock");
    cfg.scr_weather  = server.hasArg("scr_weather");
    cfg.scr_forecast = server.hasArg("scr_forecast");
    cfg.scr_telegram = server.hasArg("scr_telegram");
    cfg.scr_sensor   = server.hasArg("scr_sensor");
    if (!cfg.scr_clock&&!cfg.scr_weather&&!cfg.scr_forecast&&!cfg.scr_telegram&&!cfg.scr_sensor) cfg.scr_clock=true;
    if (server.hasArg("time_clock"))    cfg.time_clock    = constrain(server.arg("time_clock").toInt(),   1,120);
    if (server.hasArg("time_weather"))  cfg.time_weather  = constrain(server.arg("time_weather").toInt(), 1,120);
    if (server.hasArg("time_forecast")) cfg.time_forecast = constrain(server.arg("time_forecast").toInt(),1,120);
    if (server.hasArg("time_telegram")) cfg.time_telegram = constrain(server.arg("time_telegram").toInt(),3,300);
    if (server.hasArg("time_sensor"))   cfg.time_sensor   = constrain(server.arg("time_sensor").toInt(),  1,120);
    // Дисплей
    cfg.use24h      = server.hasArg("use24h");
    cfg.showSeconds = server.hasArg("sec");
    cfg.nightMode   = server.hasArg("night");
    if (server.hasArg("nfrom")) cfg.nightFrom = constrain(server.arg("nfrom").toInt(),0,23);
    if (server.hasArg("nto"))   cfg.nightTo   = constrain(server.arg("nto").toInt(),  0,23);
    // Telegram
    if (server.hasArg("tg_channel")) strncpy(cfg.tg_channel, server.arg("tg_channel").c_str(), 32);
    if (server.hasArg("tg_interval")) cfg.tg_interval = constrain(server.arg("tg_interval").toInt(),1,60);
    if (server.hasArg("tg_scroll"))   cfg.tg_scroll   = constrain(server.arg("tg_scroll").toInt(), 30,500);
    // Датчик
    if (server.hasArg("sensor_type")) cfg.sensor_type = constrain(server.arg("sensor_type").toInt(),0,3);
    if (server.hasArg("sensor_pin"))  cfg.sensor_pin  = constrain(server.arg("sensor_pin").toInt(), 0,16);
    if (server.hasArg("toff"))        cfg.tempOffset  = server.arg("toff").toInt() * 10;

    saveConfig();
    server.send(200,"text/html; charset=utf-8", buildPage("&#10003; Сохранено! Перезагрузка..."));
    delay(1500);
    ESP.restart();
  });

  server.on("/refresh", HTTP_POST, []() {
    fetchWeather(); fetchForecast(); fetchTelegram();
    lastWeatherUpdate = millis(); lastTgUpdate = millis();
    server.send(200,"text/html; charset=utf-8", buildPage("&#8635; Данные обновлены!"));
  });

  server.on("/reboot", HTTP_POST, []() {
    server.send(200,"text/plain","Перезагрузка...");
    delay(500); ESP.restart();
  });

  server.on("/data", HTTP_GET, []() {
    StaticJsonDocument<256> doc;
    doc["temp_sensor"] = sensorTemp;
    doc["hum_sensor"]  = sensorHum;
    doc["temp_wx"]     = wx.temp;
    doc["desc_wx"]     = wx.desc;
    doc["heap"]        = ESP.getFreeHeap();
    doc["rssi"]        = WiFi.RSSI();
    String out; serializeJson(doc, out);
    server.send(200,"application/json",out);
  });

  server.begin();
}

// ============================================================
//  WiFi
// ============================================================
void connectWiFi() {
  drawBoot("Подключение WiFi...", 10);
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg.ssid, cfg.pass);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 40) {
    delay(500); attempts++;
    drawBoot("Подключение WiFi...", 10 + attempts);
  }
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("WiFi: " + WiFi.localIP().toString());
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("WiFiClock-AP", "12345678");
    Serial.println("AP mode: 192.168.4.1");
  }
}

// ============================================================
//  SETUP
// ============================================================
void setup() {
  Serial.begin(115200);
  EEPROM.begin(sizeof(Config));

  u8g2.begin();
  u8g2.enableUTF8Print();

  drawBoot("Инициализация...", 5);
  loadConfig();
  drawBoot("Конфиг загружен", 15);

  connectWiFi();
  drawBoot("WiFi OK", 40);

  timeClient.begin();
  timeClient.update();
  drawBoot("NTP синхронизация", 55);

  setupWebServer();
  drawBoot("Веб-сервер OK", 65);

  if (wifiConnected) {
    setupOTA();
    drawBoot("OTA готов", 70);
  }

  fetchWeather();
  drawBoot("Погода загружена", 82);

  fetchForecast();
  drawBoot("Прогноз загружен", 90);

  fetchTelegram();
  drawBoot("Telegram OK", 100);

  delay(600);
  screenTimer = millis();

  // Установить начальный экран — первый активный
  currentScreen = nextActiveScreen(SCREEN_SENSOR); // даст первый включённый с начала
}

// ============================================================
//  LOOP
// ============================================================
void loop() {
  server.handleClient();
  if (wifiConnected) ArduinoOTA.handle();
  unsigned long now = millis();

  // NTP
  if (now - lastNTPSync > NTP_INTERVAL) {
    timeClient.update(); lastNTPSync = now;
  }

  // Погода
  if (now - lastWeatherUpdate > (unsigned long)cfg.wx_interval * 60000UL) {
    fetchWeather(); fetchForecast(); lastWeatherUpdate = now;
  }

  // Telegram
  if (now - lastTgUpdate > (unsigned long)cfg.tg_interval * 60000UL) {
    fetchTelegram(); lastTgUpdate = now;
  }

  // Датчик
  if (cfg.scr_sensor && cfg.sensor_type > 0 && now - lastSensorRead > 10000UL) {
    readSensor(); lastSensorRead = now;
  }

  // Смена экранов
  handleScreenCycle();

  // Рисуем
  switch (currentScreen) {
    case SCREEN_CLOCK:    drawClock();    break;
    case SCREEN_WEATHER:  drawWeather();  break;
    case SCREEN_FORECAST: drawForecast(); break;
    case SCREEN_TELEGRAM: drawTelegram(); break;
    case SCREEN_SENSOR:   drawSensor();   break;
    default:              drawClock();    break;
  }

  delay(30);
}
