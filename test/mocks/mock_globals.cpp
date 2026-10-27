// Single definitions of the global hardware doubles used by the mocks.
#include <Arduino.h>
#include <FS.h>

#include <chrono>

HardwareSerial Serial;
fs::MockFS SPIFFS;

unsigned long millis() {
  using namespace std::chrono;
  static const auto start = steady_clock::now();
  return static_cast<unsigned long>(duration_cast<milliseconds>(steady_clock::now() - start).count());
}
