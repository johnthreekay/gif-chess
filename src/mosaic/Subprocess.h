#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mosaic {

class Subprocess {
public:
  struct Options {
    bool captureStdout = true;
    bool captureStdin = false;
  };

  explicit Subprocess(std::vector<std::string> args);
  Subprocess(std::vector<std::string> args, Options opts);
  ~Subprocess();

  Subprocess(Subprocess const&) = delete;
  Subprocess& operator=(Subprocess const&) = delete;

  // Reads exactly n bytes (blocking) unless EOF first; returns bytes read
  // (n, or fewer only on EOF). For fixed-size payloads; otherwise readSome().
  std::size_t read(std::uint8_t* buf, std::size_t n);

  // Single read of up to n bytes (one ::read() call). Returns bytes read
  // (1..n), or 0 on EOF. Blocks for the first byte but does NOT wait to fill
  // the buffer. For stream protocols of unknown message length (e.g. UCI).
  std::size_t readSome(std::uint8_t* buf, std::size_t n);

  // Waits up to timeoutMs for the child's stdout to become readable (or hit
  // EOF). Returns 1 if so, 0 on timeout, -1 on error. A negative timeout
  // blocks indefinitely.
  int pollReadable(int timeoutMs);

  // Blocking write of n bytes to the child's stdin.
  void write(std::uint8_t const* buf, std::size_t n);

  void closeStdin();

  // Waits for the child to exit; returns its exit code (or -1 if signalled).
  int wait();

  // Diagnostics: waits up to graceMs, then returns e.g. "exited with code 0",
  // "killed by signal 11 (segfault)", or "still running". Reaps the child if
  // it has exited so a later wait()/destructor won't block on it.
  std::string exitDescription(int graceMs);

  // Convenience: spawn, read all stdout to a string, wait, throw on nonzero exit.
  static std::string runCapture(std::vector<std::string> args);

private:
  int pid_ = -1;
  int stdoutFd_ = -1;
  int stdinFd_ = -1;
};

}  // namespace mosaic
