#include "CasaCore.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace casa {

bool append(Log& log, const Sample& sample) {
  if (sample.epoch < kClockSetAfter) return false;
  if (log.count > 0) {
    const Sample& last = log.samples[log.count - 1];
    if (sample.epoch < last.epoch) {
      log.count = 0;
    } else if (sample.epoch - last.epoch < kMinGap) {
      return false;
    }
  }
  if (log.count == kLogCap) {
    std::memmove(&log.samples[0], &log.samples[1], sizeof(Sample) * (kLogCap - 1));
    log.count = kLogCap - 1;
  }
  log.samples[log.count++] = sample;
  return true;
}

size_t toBytes(const Log& log, uint8_t* out, const size_t cap) {
  const size_t need = 4 + sizeof(Sample) * static_cast<size_t>(log.count);
  if (cap < need) return 0;
  out[0] = 'C';
  out[1] = 1;
  out[2] = static_cast<uint8_t>(log.count & 0xFF);
  out[3] = static_cast<uint8_t>(log.count >> 8);
  std::memcpy(out + 4, log.samples, need - 4);
  return need;
}

bool fromBytes(const uint8_t* data, const size_t size, Log& log) {
  log.count = 0;
  if (size < 4 || data[0] != 'C' || data[1] != 1) return false;
  const int count = data[2] | (data[3] << 8);
  if (count > kLogCap || size < 4 + sizeof(Sample) * static_cast<size_t>(count)) return false;
  std::memcpy(log.samples, data + 4, sizeof(Sample) * static_cast<size_t>(count));
  // Out of order means it is not ours, or it was cut short mid-write.
  for (int i = 1; i < count; ++i) {
    if (log.samples[i].epoch < log.samples[i - 1].epoch) return false;
  }
  log.count = count;
  return true;
}

Range rangeSince(const Log& log, const uint32_t since) {
  Range r = {0, 0, 0, 0, 0};
  for (int i = 0; i < log.count; ++i) {
    const Sample& s = log.samples[i];
    if (s.epoch < since) continue;
    if (r.count == 0) {
      r.minTemp = r.maxTemp = s.tempCenti;
      r.minHum = r.maxHum = s.humCenti;
    } else {
      if (s.tempCenti < r.minTemp) r.minTemp = s.tempCenti;
      if (s.tempCenti > r.maxTemp) r.maxTemp = s.tempCenti;
      if (s.humCenti < r.minHum) r.minHum = s.humCenti;
      if (s.humCenti > r.maxHum) r.maxHum = s.humCenti;
    }
    ++r.count;
  }
  return r;
}

bool changeOver(const Log& log, const Sample& now, const uint32_t seconds, int& tempCenti, int& humCenti) {
  if (now.epoch < seconds) return false;
  const uint32_t want = now.epoch - seconds;
  int best = -1;
  uint32_t bestGap = seconds / 4 + 1;
  for (int i = 0; i < log.count; ++i) {
    const uint32_t e = log.samples[i].epoch;
    const uint32_t gap = e > want ? e - want : want - e;
    if (gap < bestGap) {
      bestGap = gap;
      best = i;
    }
  }
  if (best < 0) return false;
  tempCenti = now.tempCenti - log.samples[best].tempCenti;
  humCenti = static_cast<int>(now.humCenti) - static_cast<int>(log.samples[best].humCenti);
  return true;
}

float dewPoint(const float tempC, float humidityPct) {
  if (humidityPct < 1.0f) humidityPct = 1.0f;
  const float a = 17.62f;
  const float b = 243.12f;
  const float g = std::log(humidityPct / 100.0f) + a * tempC / (b + tempC);
  return b * g / (a - g);
}

Air airOf(const float humidityPct) {
  if (humidityPct < 35.0f) return Air::Dry;
  if (humidityPct > 65.0f) return Air::Humid;
  return Air::Good;
}

const char* airEs(const Air air) {
  switch (air) {
    case Air::Dry:
      return "AIRE SECO";
    case Air::Humid:
      return "MUCHA HUMEDAD";
    case Air::Good:
      break;
  }
  return "AIRE A GUSTO";
}

void formatTenths(const int centi, char* out, const size_t cap) {
  const int tenths = centi >= 0 ? (centi + 5) / 10 : -((-centi + 5) / 10);
  const int mag = tenths < 0 ? -tenths : tenths;
  std::snprintf(out, cap, "%s%d.%d", tenths < 0 ? "-" : "", mag / 10, mag % 10);
}

Scale scaleFor(const int loCenti, const int hiCenti, const int minSpan) {
  Scale s;
  s.lo = static_cast<int>(std::floor(static_cast<float>(loCenti) / 100.0f));
  s.hi = static_cast<int>(std::ceil(static_cast<float>(hiCenti) / 100.0f));
  while (s.hi - s.lo < minSpan) {
    // Grow on the side with less room, so the line sits in the middle.
    if (hiCenti - s.lo * 100 > s.hi * 100 - loCenti) {
      ++s.hi;
    } else {
      --s.lo;
    }
  }
  return s;
}

}  // namespace casa
