// Host-side in-memory mock of the ESP8266/ESP32 SPIFFS filesystem.
// Files live in a std::map so tests can seed, inspect and corrupt them,
// and fault-injection hooks let tests simulate mount/open failures.
#ifndef QUARKDB_MOCK_FS_H
#define QUARKDB_MOCK_FS_H

#include <Arduino.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace fs {

class MockFS;

class File : public Stream {
 public:
  File() {}

  size_t write(uint8_t c) override;
  using Print::write;
  int available() override {
    if (!readable()) return 0;
    return static_cast<int>(content_->size() - pos_);
  }
  int read() override {
    if (!readable() || pos_ >= content_->size()) return -1;
    return static_cast<unsigned char>((*content_)[pos_++]);
  }
  int peek() override {
    if (!readable() || pos_ >= content_->size()) return -1;
    return static_cast<unsigned char>((*content_)[pos_]);
  }
  void flush() {}
  size_t size() const { return content_ ? content_->size() : 0; }
  bool seek(uint32_t pos) {
    if (!content_ || pos > content_->size()) return false;
    pos_ = pos;
    return true;
  }
  void close() { content_.reset(); dirEntries_.clear(); isDir_ = false; open_ = false; }
  operator bool() const { return open_; }

  const char* path() const { return path_.c_str(); }
  const char* name() const {
    size_t p = path_.find_last_of('/');
    return p == std::string::npos ? path_.c_str() : path_.c_str() + p + 1;
  }
  bool isDirectory() const { return open_ && isDir_; }
  File openNextFile(const char* mode = "r");

 private:
  friend class MockFS;
  bool readable() const { return open_ && !isDir_ && content_ && mode_ == "r"; }
  bool writable() const { return open_ && !isDir_ && content_ && (mode_ == "w" || mode_ == "a"); }

  bool open_ = false;
  bool isDir_ = false;
  std::string path_;
  std::string mode_;
  std::shared_ptr<std::string> content_;
  size_t pos_ = 0;
  MockFS* fs_ = nullptr;
  std::vector<std::string> dirEntries_;
  size_t dirPos_ = 0;
};

// ESP8266-style directory iterator.
class Dir {
 public:
  Dir() {}
  explicit Dir(std::vector<std::string> entries) : entries_(std::move(entries)) {}
  bool next() { return ++idx_ < static_cast<int>(entries_.size()); }
  String fileName() const { return String(entries_[idx_]); }

 private:
  std::vector<std::string> entries_;
  int idx_ = -1;
};

class MockFS {
 public:
  // SPIFFS_OBJ_NAME_LEN is 32 on ESP8266 and ESP32, including the NUL.
  static constexpr size_t kMaxPathLen = 31;

  // ---- Arduino FS API ----
  bool begin() { return begin(false); }
  bool begin(bool /*formatOnFail*/) {
    beginCalls_++;
    mounted_ = beginResult_;
    return beginResult_;
  }
  void end() { mounted_ = false; }
  bool format() { files_.clear(); return true; }

  bool exists(const char* path) const { return files_.count(path) > 0; }
  bool exists(const String& path) const { return exists(path.c_str()); }

  bool remove(const char* path) {
    if (poweredOff_) return false;
    if (removePowerLoss_ != RemovePowerLoss::None && files_.count(path)) {
      // Power is cut while the file is being removed. With Partial, some of
      // its pages are already gone but the file still exists.
      if (removePowerLoss_ == RemovePowerLoss::Partial) {
        auto& content = *files_[path];
        content.resize(content.size() / 2);
      }
      removePowerLoss_ = RemovePowerLoss::None;
      poweredOff_ = true;
      return false;
    }
    return files_.erase(path) > 0;
  }
  bool remove(const String& path) { return remove(path.c_str()); }

  File open(const String& path, const char* mode = "r") { return open(path.c_str(), mode); }
  // SPIFFS semantics: fails if `from` is missing or `to` already exists.
  bool rename(const char* from, const char* to) {
    if (poweredOff_ || !files_.count(from) || files_.count(to) || strlen(to) > kMaxPathLen) return false;
    files_[to] = files_[from];
    files_.erase(from);
    return true;
  }
  bool rename(const String& from, const String& to) { return rename(from.c_str(), to.c_str()); }

  File open(const char* path, const char* mode = "r") {
    File f;
    std::string p(path), m(mode);
    if (poweredOff_ || failModes_.count(m)) return f;
    if (p.size() > kMaxPathLen && m != "r") return f;  // name too long to create
    if (isDirectoryPath(p) && m == "r") {
      f.open_ = true;
      f.isDir_ = true;
      f.path_ = p;
      f.fs_ = this;
      f.dirEntries_ = listUnder(p);
      return f;
    }
    if (m == "r") {
      auto it = files_.find(p);
      if (it == files_.end()) return f;
      f.content_ = it->second;
    } else if (m == "w") {
      auto& slot = files_[p];
      if (!slot) slot = std::make_shared<std::string>();
      slot->clear();
      f.content_ = slot;
    } else if (m == "a") {
      auto& slot = files_[p];
      if (!slot) slot = std::make_shared<std::string>();
      f.content_ = slot;
    } else {
      return f;
    }
    f.open_ = true;
    f.path_ = p;
    f.mode_ = m;
    f.fs_ = this;
    // Power is cut right after this open (so a "w" open has already
    // truncated). From here on nothing reaches the flash.
    if (powerLossModes_.count(m)) poweredOff_ = true;
    return f;
  }

  Dir openDir(const char* path) { return Dir(listUnder(path)); }
  Dir openDir(const String& path) { return openDir(path.c_str()); }

  // ---- test helpers ----
  void reset() {
    files_.clear();
    failModes_.clear();
    powerLossModes_.clear();
    removePowerLoss_ = RemovePowerLoss::None;
    poweredOff_ = false;
    beginResult_ = true;
    mounted_ = false;
    beginCalls_ = 0;
  }
  // Simulate a power cut immediately after the next open() with this mode.
  // Every later write/remove/rename/open is dropped until powerOn().
  void powerLossAfterOpen(const std::string& mode) { powerLossModes_.insert(mode); }
  // Simulate a power cut during the next remove(). The file survives: intact
  // (cut before any page was erased) or with partial=true half-erased.
  void powerLossOnRemove(bool partial) {
    removePowerLoss_ = partial ? RemovePowerLoss::Partial : RemovePowerLoss::Before;
  }
  bool poweredOff() const { return poweredOff_; }
  // "Reboot": flash keeps whatever was persisted before the power cut.
  void powerOn() {
    powerLossModes_.clear();
    removePowerLoss_ = RemovePowerLoss::None;
    poweredOff_ = false;
  }
  void setBeginResult(bool ok) { beginResult_ = ok; }
  int beginCalls() const { return beginCalls_; }
  // Make every open() with this mode ("r", "w", "a") fail until cleared.
  void failOpen(const std::string& mode, bool fail = true) {
    if (fail) failModes_.insert(mode); else failModes_.erase(mode);
  }
  void writeRaw(const std::string& path, const std::string& content) {
    files_[path] = std::make_shared<std::string>(content);
  }
  std::string readRaw(const std::string& path) const {
    auto it = files_.find(path);
    return it == files_.end() ? std::string() : *it->second;
  }
  std::vector<std::string> paths() const {
    std::vector<std::string> v;
    for (auto& kv : files_) v.push_back(kv.first);
    return v;
  }

 private:
  bool isDirectoryPath(const std::string& p) const {
    if (p == "/") return true;
    std::string prefix = p.back() == '/' ? p : p + "/";
    for (auto& kv : files_)
      if (kv.first.compare(0, prefix.size(), prefix) == 0) return true;
    return false;
  }
  std::vector<std::string> listUnder(const std::string& p) const {
    std::string prefix = p.empty() || p.back() != '/' ? p + "/" : p;
    std::vector<std::string> v;
    for (auto& kv : files_)
      if (kv.first.compare(0, prefix.size(), prefix) == 0) v.push_back(kv.first);
    return v;
  }

  std::map<std::string, std::shared_ptr<std::string>> files_;
  std::set<std::string> failModes_;
  std::set<std::string> powerLossModes_;
  enum class RemovePowerLoss { None, Before, Partial };
  RemovePowerLoss removePowerLoss_ = RemovePowerLoss::None;
  bool poweredOff_ = false;
  bool beginResult_ = true;
  bool mounted_ = false;
  int beginCalls_ = 0;
};

inline size_t File::write(uint8_t c) {
  if (!writable() || (fs_ && fs_->poweredOff())) return 0;
  content_->push_back(static_cast<char>(c));
  return 1;
}

inline File File::openNextFile(const char* mode) {
  if (!isDirectory() || !fs_ || dirPos_ >= dirEntries_.size()) return File();
  return fs_->open(dirEntries_[dirPos_++].c_str(), mode);
}

}  // namespace fs

using fs::Dir;
using fs::File;

extern fs::MockFS SPIFFS;

#endif
