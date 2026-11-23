// The MIT License (MIT)
//
// Copyright (c) 2024-2026 Quarkifi Technologies Pvt Ltd
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// Single definitions of the global hardware doubles used by the mocks.
#include <Arduino.h>
#include <FS.h>

#include <chrono>
#include <random>

HardwareSerial Serial;
fs::MockFS SPIFFS;

static uint32_t mockRandom() {
  static std::mt19937 engine{std::random_device{}()};
  return engine();
}

#if defined(ESP8266)
EspClass ESP;
uint32_t EspClass::random() const { return mockRandom(); }
#elif defined(ESP32)
uint32_t esp_random() { return mockRandom(); }
#endif

static unsigned long millisOffset = 0;

unsigned long millis() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count()) + millisOffset;
}

void advanceMillis(unsigned long ms) { millisOffset += ms; }
