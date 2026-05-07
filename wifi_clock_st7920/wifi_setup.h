#pragma once
#include "config.h"

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
