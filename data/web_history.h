#pragma once

#include <Arduino.h>

struct HistoryPoint {
  uint32_t ts;
  float temperature;
  float humidity;
  uint16_t co2;
  uint16_t tvoc;
  uint8_t aqi;
};

static constexpr size_t HISTORY_CAPACITY = 60;

class WebHistory {
public:
  void add(const HistoryPoint& p) {
    _points[_head] = p;
    _head = (_head + 1) % HISTORY_CAPACITY;
    if (_count < HISTORY_CAPACITY) ++_count;
  }

  size_t count() const { return _count; }

  HistoryPoint at(size_t i) const {
    if (i >= _count) return {};
    size_t first = (_head + HISTORY_CAPACITY - _count) % HISTORY_CAPACITY;
    return _points[(first + i) % HISTORY_CAPACITY];
  }

  HistoryPoint latest() const {
    return _count ? at(_count - 1) : HistoryPoint{};
  }

private:
  HistoryPoint _points[HISTORY_CAPACITY]{};
  size_t _head = 0;
  size_t _count = 0;
};
