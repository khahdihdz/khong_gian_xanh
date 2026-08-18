#pragma once

#include <Arduino.h>
#include <WebServer.h>
#include "../data/web_history.h"

// Web API helpers. Call webApiBegin(server, history, dataProvider) from setup
// and webApiHandle(server) from loop after wiring the existing SensorData source.
using WebDataJsonFn = String (*)();

inline void webApiBegin(WebServer& server, WebHistory& history, WebDataJsonFn dataProvider) {
  server.on("/api/status", HTTP_GET, [&, dataProvider]() {
    server.send(200, "application/json; charset=utf-8", dataProvider ? dataProvider() : "{}");
  });

  server.on("/api/history", HTTP_GET, [&]() {
    String json = "[";
    for (size_t i = 0; i < history.count(); ++i) {
      if (i) json += ',';
      const auto p = history.at(i);
      json += "{\"ts\":" + String(p.ts);
      if (isnan(p.temperature)) json += ",\"temperature\":null";
      else json += ",\"temperature\":" + String(p.temperature, 1);
      if (isnan(p.humidity)) json += ",\"humidity\":null";
      else json += ",\"humidity\":" + String(p.humidity, 1);
      json += ",\"co2\":" + String(p.co2);
      json += ",\"tvoc\":" + String(p.tvoc);
      json += ",\"aqi\":" + String(p.aqi) + '}';
    }
    json += ']';
    server.send(200, "application/json; charset=utf-8", json);
  });
}

inline void webApiHandle(WebServer& server) { server.handleClient(); }
