#pragma once
#include "config.h"

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
    if (server.hasArg("ssid"))  { strncpy(cfg.ssid,   server.arg("ssid").c_str(),  31); cfg.ssid[31] = 0; }
    if (server.hasArg("pass"))  { strncpy(cfg.pass,   server.arg("pass").c_str(),  63); cfg.pass[63] = 0; }
    if (server.hasArg("tz"))    cfg.timezone    = server.arg("tz").toInt();
    if (server.hasArg("api"))   { strncpy(cfg.apiKey, server.arg("api").c_str(),   47); cfg.apiKey[47] = 0; }
    if (server.hasArg("city"))  { strncpy(cfg.city,   server.arg("city").c_str(),  31); cfg.city[31] = 0; }
    if (server.hasArg("wx_interval")) cfg.wx_interval = constrain(server.arg("wx_interval").toInt(),1,60);
    cfg.wx_celsius  = server.hasArg("wx_unit") ? (server.arg("wx_unit").toInt()==1) : true;
    // Экраны
    cfg.scr_clock    = server.hasArg("scr_clock");
    cfg.scr_weather  = server.hasArg("scr_weather");
    cfg.scr_forecast = server.hasArg("scr_forecast");
    cfg.scr_telegram = server.hasArg("scr_telegram");
    cfg.scr_sensor   = server.hasArg("scr_sensor");
    cfg.scr_holiday  = server.hasArg("scr_holiday");
    cfg.scr_button   = server.hasArg("scr_button");
    if (server.hasArg("time_holiday")) cfg.time_holiday = constrain(server.arg("time_holiday").toInt(),1,120);
    if (!cfg.scr_clock&&!cfg.scr_weather&&!cfg.scr_forecast&&!cfg.scr_telegram&&!cfg.scr_sensor&&!cfg.scr_holiday) cfg.scr_clock=true;
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
    if (server.hasArg("tg_channel")) { strncpy(cfg.tg_channel, server.arg("tg_channel").c_str(), 32); cfg.tg_channel[32] = 0; }
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
