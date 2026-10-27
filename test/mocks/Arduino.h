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

// Host-side mock of the Arduino core used by QuarkDB unit tests.
// Provides just enough of String, Print, Stream and Serial for the library
// (and ArduinoJson's Arduino integrations) to compile and run on a PC.
#ifndef QUARKDB_MOCK_ARDUINO_H
#define QUARKDB_MOCK_ARDUINO_H

#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

typedef uint8_t byte;
typedef bool boolean;

class String {
 public:
  String() {}
  String(const char* s) : s_(s ? s : "") {}
  String(const std::string& s) : s_(s) {}
  String(char c) : s_(1, c) {}
  String(int v) : s_(std::to_string(v)) {}
  String(unsigned int v) : s_(std::to_string(v)) {}
  String(long v) : s_(std::to_string(v)) {}
  String(unsigned long v) : s_(std::to_string(v)) {}
  String(long long v) : s_(std::to_string(v)) {}
  String(unsigned long long v) : s_(std::to_string(v)) {}
  String(double v, unsigned char decimals = 2) {
    char buf[64];
    snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    s_ = buf;
  }

  String& operator=(const char* s) {
    s_ = s ? s : "";
    return *this;
  }

  const char* c_str() const { return s_.c_str(); }
  unsigned int length() const { return static_cast<unsigned int>(s_.size()); }
  bool isEmpty() const { return s_.empty(); }
  void reserve(unsigned int n) { s_.reserve(n); }
  const std::string& str() const { return s_; }

  bool concat(const char* s) {
    if (s) s_ += s;
    return true;
  }
  bool concat(const String& s) {
    s_ += s.s_;
    return true;
  }
  bool concat(char c) {
    s_ += c;
    return true;
  }
  String& operator+=(const String& s) { concat(s); return *this; }
  String& operator+=(const char* s) { concat(s); return *this; }
  String& operator+=(char c) { concat(c); return *this; }

  char operator[](unsigned int i) const { return i < s_.size() ? s_[i] : 0; }
  char charAt(unsigned int i) const { return (*this)[i]; }

  bool equals(const String& o) const { return s_ == o.s_; }
  bool operator==(const String& o) const { return s_ == o.s_; }
  bool operator==(const char* o) const { return s_ == (o ? o : ""); }
  bool operator!=(const String& o) const { return !(*this == o); }
  bool operator!=(const char* o) const { return !(*this == o); }
  bool operator<(const String& o) const { return s_ < o.s_; }

  bool startsWith(const String& prefix) const { return startsWith(prefix, 0); }
  bool startsWith(const String& prefix, unsigned int offset) const {
    if (offset > s_.size() || prefix.s_.size() > s_.size() - offset) return false;
    return s_.compare(offset, prefix.s_.size(), prefix.s_) == 0;
  }
  bool endsWith(const String& suffix) const {
    return suffix.s_.size() <= s_.size() &&
           s_.compare(s_.size() - suffix.s_.size(), suffix.s_.size(), suffix.s_) == 0;
  }

  int indexOf(char c) const { return indexOf(c, 0); }
  int indexOf(char c, unsigned int from) const { return toIdx(s_.find(c, from)); }
  int indexOf(const String& s) const { return indexOf(s, 0); }
  int indexOf(const String& s, unsigned int from) const { return toIdx(s_.find(s.s_, from)); }
  int lastIndexOf(char c) const { return toIdx(s_.rfind(c)); }
  int lastIndexOf(const String& s) const { return toIdx(s_.rfind(s.s_)); }

  // Arduino semantics: indices are clamped and swapped when reversed.
  String substring(unsigned int from) const { return substring(from, length()); }
  String substring(unsigned int from, unsigned int to) const {
    if (from > to) std::swap(from, to);
    if (from >= s_.size()) return String();
    if (to > s_.size()) to = length();
    return String(s_.substr(from, to - from));
  }

  void trim() {
    size_t b = 0, e = s_.size();
    while (b < e && isspace(static_cast<unsigned char>(s_[b]))) b++;
    while (e > b && isspace(static_cast<unsigned char>(s_[e - 1]))) e--;
    s_ = s_.substr(b, e - b);
  }
  void toLowerCase() { for (auto& c : s_) c = static_cast<char>(tolower(c)); }
  void toUpperCase() { for (auto& c : s_) c = static_cast<char>(toupper(c)); }
  long toInt() const { return atol(s_.c_str()); }
  float toFloat() const { return static_cast<float>(atof(s_.c_str())); }

 private:
  static int toIdx(size_t p) { return p == std::string::npos ? -1 : static_cast<int>(p); }
  std::string s_;
};

inline String operator+(const String& a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, const char* b) { String r(a); r += b; return r; }
inline String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
inline String operator+(const String& a, char b) { String r(a); r += b; return r; }
inline bool operator==(const char* a, const String& b) { return b == a; }

class Print;

class Printable {
 public:
  virtual ~Printable() {}
  virtual size_t printTo(Print& p) const = 0;
};

class Print {
 public:
  virtual ~Print() {}
  virtual size_t write(uint8_t c) = 0;
  virtual size_t write(const uint8_t* buf, size_t n) {
    size_t w = 0;
    for (size_t i = 0; i < n; i++) w += write(buf[i]);
    return w;
  }
  size_t write(const char* s) { return s ? write(reinterpret_cast<const uint8_t*>(s), strlen(s)) : 0; }

  size_t print(const String& s) { return write(s.c_str()); }
  size_t print(const char* s) { return write(s); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  size_t print(int v) { return print(String(v)); }
  size_t print(long v) { return print(String(v)); }
  size_t print(unsigned int v) { return print(String(v)); }
  size_t print(unsigned long v) { return print(String(v)); }
  size_t print(double v, int d = 2) { return print(String(v, static_cast<unsigned char>(d))); }
  size_t print(const Printable& p) { return p.printTo(*this); }

  size_t println() { return write("\r\n"); }
  template <typename T>
  size_t println(const T& v) { size_t n = print(v); return n + println(); }

  size_t printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0) return 0;
    return write(buf);
  }
};

class Stream : public Print {
 public:
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int peek() = 0;

  size_t readBytes(char* buf, size_t n) {
    size_t i = 0;
    while (i < n) {
      int c = read();
      if (c < 0) break;
      buf[i++] = static_cast<char>(c);
    }
    return i;
  }
  String readStringUntil(char terminator) {
    std::string out;
    int c;
    while ((c = read()) >= 0 && c != terminator) out += static_cast<char>(c);
    return String(out);
  }
  String readString() {
    std::string out;
    int c;
    while ((c = read()) >= 0) out += static_cast<char>(c);
    return String(out);
  }
};

// Serial port double: tests push input with feed() and inspect output().
class HardwareSerial : public Stream {
 public:
  void begin(unsigned long) {}
  void end() {}
  size_t write(uint8_t c) override { out_ += static_cast<char>(c); return 1; }
  using Print::write;
  int available() override { return static_cast<int>(in_.size() - pos_); }
  int read() override { return pos_ < in_.size() ? static_cast<unsigned char>(in_[pos_++]) : -1; }
  int peek() override { return pos_ < in_.size() ? static_cast<unsigned char>(in_[pos_]) : -1; }
  void flush() {}
  operator bool() const { return true; }

  // ---- test helpers ----
  void feed(const std::string& s) { in_ += s; }
  const std::string& output() const { return out_; }
  void reset() { in_.clear(); out_.clear(); pos_ = 0; }

 private:
  std::string in_, out_;
  size_t pos_ = 0;
};

extern HardwareSerial Serial;

inline void delay(unsigned long) {}
unsigned long millis();
inline void yield() {}

#endif
