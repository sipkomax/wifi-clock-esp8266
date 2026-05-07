#pragma once

// ВАЖНО: Сохранить в кодировке UTF-8 (Arduino IDE делает это по умолчанию)
/*
 * ============================================================
 *  WiFi Часы на ESP8266 + ST7920 128x64
 *
 *  Экраны (авточеродование, настраивается через веб):
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
  bool scr_holiday     = true;
  bool scr_button      = false;
  // Время показа каждого экрана (сек)
  int  time_clock      = 10;
  int  time_weather    = 5;
  int  time_forecast   = 5;
  int  time_telegram   = 15;
  int  time_sensor     = 5;
  int  time_holiday    = 10;
  // Telegram
  char tg_channel[33]  = "rf4map";
  int  tg_interval     = 2;          // мин
  int  tg_scroll       = 80;         // мс между шагами прокрутки
  // Датчик
  int  sensor_type     = 0;          // 0=нет 1=DHT22 2=DHT11 3=SHT30
  int  sensor_pin      = 2;          // GPIO (для DHT)
  int  tempOffset      = 0;          // коррекция * 10
  // Маркер
  char magic[4]        = "CF3";
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
  char  day[6]   = "---";   // 2 кирил. символа = 4 байта UTF-8 + 
  float tempMin  = 0;
  float tempMax  = 0;
  float windMax  = 0;
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
  SCREEN_TELEGRAM, SCREEN_SENSOR, SCREEN_HOLIDAY, SCREEN_IP
};
Screen nextActiveScreen(Screen from);  // forward declaration
void drawHoliday();

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
//  КОНСТАНТЫ БЕГУЩЕЙ СТРОКИ И ТАЙМЕРЫ
// ============================================================
#define SCROLL_SPEED  35    // мс между шагами прокрутки (бегущая строка на экране часов)
#define SCROLL_STEP   1     // пикселей за шаг
#define SCREEN_BUTTON_PIN D6 // button: D6 -> GND (INPUT_PULLUP)

unsigned long bootMs        = 0;       // время загрузки (мс)
String        bottomLine    = "";      // текст бегущей строки на экране часов
int           scrollPos     = 128;     // позиция бегущей строки (пикс)
unsigned long scrollTimer   = 0;       // таймер шага прокрутки

int           holScrollPos    = 0;     // позиция скролла праздников
unsigned long holScrollTimer  = 0;     // таймер скролла праздников
unsigned long holTimer        = 0;     // таймер скролла праздников

// ============================================================
//  XBM ИКОНКИ ПОГОДЫ 16x16
// ============================================================
static const uint8_t PROGMEM ico_rain[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x0d,0xf8,0x1f,0xf8,0x3f,0xf8,0x3f,0xf0,0x1f,0x00,0x00,0x10,0x04,0x88,0x02,0x40,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_snow[] = {0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x0d,0xf8,0x1f,0xf8,0x3f,0xf8,0x3f,0xf0,0x1f,0x00,0x00,0x00,0x05,0x00,0x02,0x50,0x05,0x20,0x00,0x50,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_partly[] = {0x00,0x00,0x00,0x02,0x20,0x20,0x40,0x17,0x80,0x0f,0xc0,0x1f,0xb8,0x5c,0x7c,0x1b,0xfe,0x07,0xfe,0x0f,0xfe,0x0f,0xfc,0x07,0x00,0x00,0x08,0x02,0x44,0x01,0x20,0x00};
static const uint8_t PROGMEM ico_wind[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x00,0x04,0x80,0x0f,0xc0,0x04,0x60,0x12,0x3c,0x20,0x00,0x7c,0x00,0x26,0x00,0x13,0xe0,0x01,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_thermometer[] = {0x00,0x00,0x00,0x00,0x80,0x48,0x40,0x19,0x40,0x09,0x40,0x09,0xc0,0x11,0xc0,0x01,0xc0,0x01,0xc0,0x01,0xe0,0x03,0xa0,0x03,0xe0,0x03,0xc0,0x01,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_sun[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x08,0x08,0xd0,0x05,0xe0,0x03,0xf0,0x07,0xf4,0x17,0xf0,0x07,0xe0,0x03,0xd0,0x05,0x08,0x08,0x80,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_storm[] = {0x00,0x00,0x00,0x00,0x80,0x01,0x40,0x02,0x40,0x00,0x80,0x7f,0x00,0x00,0xf8,0x7f,0x04,0x00,0x34,0x7e,0x24,0x01,0x18,0x09,0x00,0x06,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_lightning[] = {0x00,0x00,0x00,0x00,0x00,0x00,0xe0,0x00,0xf0,0x0d,0xf8,0x1f,0xf8,0x3f,0xf8,0x3e,0x70,0x1d,0x80,0x01,0xc0,0x00,0xe0,0x01,0xc0,0x00,0x60,0x00,0x20,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_heat[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x40,0x11,0xa0,0x08,0x90,0x04,0x10,0x05,0x20,0x0a,0x40,0x12,0x40,0x11,0xa0,0x08,0x10,0x04,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_cold[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x10,0x06,0x20,0x09,0xc0,0x10,0x08,0x03,0x90,0x04,0x60,0x08,0x10,0x06,0x20,0x09,0xc0,0x10,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_cloudy[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x20,0x20,0x40,0x17,0x80,0x0f,0xc0,0x1f,0xb8,0x5c,0x7c,0x1b,0xfe,0x07,0xfe,0x0f,0xfe,0x0f,0xfc,0x07,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_cloud[] = {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x70,0x00,0xf8,0x06,0xfc,0x0f,0xfc,0x1f,0xfc,0x1f,0xf8,0x0f,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_fog[] = {0x00,0x00,0x00,0x00,0x00,0x00,0xfe,0x7f,0xfe,0x7f,0x00,0x00,0xfc,0x3f,0xfc,0x3f,0x00,0x00,0xfe,0x7f,0xfe,0x7f,0x00,0x00,0xfc,0x3f,0xfc,0x3f,0x00,0x00,0x00,0x00};
static const uint8_t PROGMEM ico_moon[] = {0x00,0x00,0x00,0x00,0x80,0x00,0x60,0x00,0x70,0x00,0x30,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x38,0x00,0x78,0x00,0x70,0x00,0xf0,0x00,0xe0,0x01,0x80,0x03,0x00,0x00};

const uint8_t* getWeatherIcon(const char* code) {
  char c = code[0], n = code[1], d = strlen(code)>2 ? code[2] : 'd';
  bool night = (d == 'n');
  if (c=='0'&&n=='1') return night ? ico_moon     : ico_sun;
  if (c=='0'&&n=='2') return ico_partly;
  if (c=='0'&&n=='3') return ico_cloudy;
  if (c=='0'&&n=='4') return ico_cloud;
  if (c=='0'&&n=='9') return ico_rain;
  if (c=='1'&&n=='0') return ico_rain;
  if (c=='1'&&n=='1') return ico_lightning;
  if (c=='1'&&n=='3') return ico_snow;
  if (c=='5'&&n=='0') return ico_fog;
  return ico_cloud;
}





