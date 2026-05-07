#pragma once
#include "config.h"

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
  <div class="chk"><input type="checkbox" name="scr_button" TPLC_SBUTTON><span>Переключать экраны тактовой кнопкой (D6/GPIO12)</span></div>
  <div style="margin-top:10px">
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_clock" TPLC_SCLOCK> &#9200; Часы</div><div class="st"><input name="time_clock" type="number" value="TPLC_TCLOCK" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_weather" TPLC_SWEATHER> &#127780; Погода</div><div class="st"><input name="time_weather" type="number" value="TPLC_TWEATHER" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_forecast" TPLC_SFORECAST> &#128197; Прогноз</div><div class="st"><input name="time_forecast" type="number" value="TPLC_TFORECAST" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_telegram" TPLC_STELEGRAM> &#128240; Telegram</div><div class="st"><input name="time_telegram" type="number" value="TPLC_TTELEGRAM" min="3" max="300"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_sensor" TPLC_SSENSOR> &#127777; Датчик</div><div class="st"><input name="time_sensor" type="number" value="TPLC_TSENSOR" min="1" max="120"><span>сек</span></div></div>
  <div class="srow"><div class="sn"><input type="checkbox" name="scr_holiday" TPLC_SHOLIDAY> &#127881; Праздники</div><div class="st"><input name="time_holiday" type="number" value="TPLC_THOLIDAY" min="1" max="120"><span>сек</span></div></div>
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
  h.replace("TPLC_SHOLIDAY", cfg.scr_holiday  ? "checked" : "");
  h.replace("TPLC_SBUTTON",  cfg.scr_button   ? "checked" : "");
  h.replace("TPLC_THOLIDAY", String(cfg.time_holiday));
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