// fs::File and friends over the browser's in-memory file system (plain stdio and dirent).
#pragma once
#include <Arduino.h>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>

void *emuOpenDir(const char *path);
bool emuReadDir(void *dir, char *name, size_t len);
void emuCloseDir(void *dir);

namespace fs {
class File : public Print {
 public:
  File() {}
  bool openPath(const String &path, const char *mode) {
    path_ = path;
    struct stat st;
    if (*mode == 'r' && stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
      void *d = emuOpenDir(path.c_str());
      dir_ = std::shared_ptr<void>(d, [](void *p) { if (p) emuCloseDir(p); });
      isDir_ = true;
      return d != nullptr;
    }
    FILE *f = fopen(path.c_str(), *mode == 'r' ? "rb" : *mode == 'a' ? "ab" : "wb");
    if (!f) return false;
    fp_ = std::shared_ptr<FILE>(f, [](FILE *p) { fclose(p); });
    return true;
  }
  operator bool() const { return fp_ || dir_; }
  size_t write(uint8_t c) override { return fp_ ? fwrite(&c, 1, 1, fp_.get()) : 0; }
  size_t write(const uint8_t *b, size_t n) override { return fp_ ? fwrite(b, 1, n, fp_.get()) : 0; }
  using Print::write;
  int read() { uint8_t c; return fp_ && fread(&c, 1, 1, fp_.get()) == 1 ? c : -1; }
  int read(uint8_t *b, size_t n) { return fp_ ? (int)fread(b, 1, n, fp_.get()) : -1; }
  size_t readBytes(char *b, size_t n) { return fp_ ? fread(b, 1, n, fp_.get()) : 0; }
  int available() { return fp_ ? (int)(size() - position()) : 0; }
  size_t size() {
    if (!fp_) return 0;
    struct stat st;
    fflush(fp_.get());
    return fstat(fileno(fp_.get()), &st) == 0 ? (size_t)st.st_size : 0;
  }
  size_t position() { return fp_ ? (size_t)ftell(fp_.get()) : 0; }
  bool seek(size_t p) { return fp_ && fseek(fp_.get(), (long)p, SEEK_SET) == 0; }
  void flush() override { if (fp_) fflush(fp_.get()); }
  void close() { fp_.reset(); dir_.reset(); }
  bool isDirectory() const { return isDir_; }
  const char *name() const { return name_.c_str(); }
  File openNextFile() {
    File f;
    if (!dir_) return f;
    char name[256];
    if (emuReadDir(dir_.get(), name, sizeof name)) {
      f.openPath(path_ + (path_.endsWith("/") ? "" : "/") + name, "r");
      f.name_ = name;
    }
    return f;
  }

 private:
  std::shared_ptr<FILE> fp_;
  std::shared_ptr<void> dir_;
  String path_, name_;
  bool isDir_ = false;
};

class FS {
 public:
  File open(const String &path, const char *mode = "r") {
    File f;
    f.openPath(path, mode);
    return f;
  }
  bool exists(const String &p) { struct stat st; return stat(p.c_str(), &st) == 0; }
  bool remove(const String &p) { return ::remove(p.c_str()) == 0; }
  bool rename(const String &a, const String &b) { return ::rename(a.c_str(), b.c_str()) == 0; }
  bool mkdir(const String &p) { return ::mkdir(p.c_str(), 0777) == 0; }
};
}  // namespace fs
using fs::File;
