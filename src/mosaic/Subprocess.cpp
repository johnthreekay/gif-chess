#include "mosaic/Subprocess.h"

#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace mosaic {

namespace {

[[noreturn]] void throwErrno(char const* what) {
  throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
}

}  // namespace

Subprocess::Subprocess(std::vector<std::string> args)
    : Subprocess(std::move(args), Options{}) {}

Subprocess::Subprocess(std::vector<std::string> args, Options opts) {
  if (args.empty()) throw std::invalid_argument("Subprocess: args empty");

  int outPipe[2] = {-1, -1};
  int inPipe[2]  = {-1, -1};

  if (opts.captureStdout && ::pipe(outPipe) < 0) throwErrno("pipe(stdout)");
  if (opts.captureStdin  && ::pipe(inPipe)  < 0) throwErrno("pipe(stdin)");

  pid_t pid = ::fork();
  if (pid < 0) throwErrno("fork");

  if (pid == 0) {
    if (opts.captureStdout) {
      ::dup2(outPipe[1], STDOUT_FILENO);
      ::close(outPipe[0]);
      ::close(outPipe[1]);
    }
    if (opts.captureStdin) {
      ::dup2(inPipe[0], STDIN_FILENO);
      ::close(inPipe[0]);
      ::close(inPipe[1]);
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);

    ::execvp(argv[0], argv.data());
    std::fprintf(stderr, "execvp %s: %s\n", argv[0], std::strerror(errno));
    _exit(127);
  }

  pid_ = pid;
  if (opts.captureStdout) {
    stdoutFd_ = outPipe[0];
    ::close(outPipe[1]);
  }
  if (opts.captureStdin) {
    stdinFd_ = inPipe[1];
    ::close(inPipe[0]);
  }
}

Subprocess::~Subprocess() {
  if (stdinFd_  != -1) ::close(stdinFd_);
  if (stdoutFd_ != -1) ::close(stdoutFd_);
  if (pid_ == -1) return;

  // Closed pipes make a well-behaved child exit; grace period then escalate
  // so the destructor can never block on a misbehaving process.
  int status;
  auto tryReap = [&]() -> bool { return ::waitpid(pid_, &status, WNOHANG) == pid_; };

  bool done = false;
  for (int i = 0; i < 20 && !done; ++i) {     // ~200ms grace for a clean exit
    done = tryReap();
    if (!done) ::usleep(10'000);
  }
  if (!done) {
    ::kill(pid_, SIGTERM);
    for (int i = 0; i < 20 && !done; ++i) {   // ~200ms more after SIGTERM
      done = tryReap();
      if (!done) ::usleep(10'000);
    }
  }
  if (!done) {
    ::kill(pid_, SIGKILL);
    ::waitpid(pid_, &status, 0);               // SIGKILL'd: reaped almost immediately
  }
  pid_ = -1;
}

std::size_t Subprocess::read(std::uint8_t* buf, std::size_t n) {
  if (stdoutFd_ == -1) throw std::logic_error("Subprocess: stdout not captured");
  std::size_t total = 0;
  while (total < n) {
    ssize_t r = ::read(stdoutFd_, buf + total, n - total);
    if (r < 0) {
      if (errno == EINTR) continue;
      throwErrno("read");
    }
    if (r == 0) break;
    total += static_cast<std::size_t>(r);
  }
  return total;
}

std::size_t Subprocess::readSome(std::uint8_t* buf, std::size_t n) {
  if (stdoutFd_ == -1) throw std::logic_error("Subprocess: stdout not captured");
  if (n == 0) return 0;
  while (true) {
    ssize_t r = ::read(stdoutFd_, buf, n);
    if (r < 0) {
      if (errno == EINTR) continue;
      throwErrno("read");
    }
    return static_cast<std::size_t>(r);  // 0 == EOF
  }
}

int Subprocess::pollReadable(int timeoutMs) {
  if (stdoutFd_ == -1) throw std::logic_error("Subprocess: stdout not captured");
  struct pollfd pfd{};
  pfd.fd = stdoutFd_;
  pfd.events = POLLIN;
  while (true) {
    int r = ::poll(&pfd, 1, timeoutMs);
    if (r < 0) {
      if (errno == EINTR) continue;
      return -1;
    }
    if (r == 0) return 0;  // timeout
    return 1;              // readable, or POLLHUP/POLLERR (readSome() sees EOF)
  }
}

void Subprocess::write(std::uint8_t const* buf, std::size_t n) {
  if (stdinFd_ == -1) throw std::logic_error("Subprocess: stdin not captured");
  std::size_t total = 0;
  while (total < n) {
    ssize_t w = ::write(stdinFd_, buf + total, n - total);
    if (w < 0) {
      if (errno == EINTR) continue;
      throwErrno("write");
    }
    total += static_cast<std::size_t>(w);
  }
}

void Subprocess::closeStdin() {
  if (stdinFd_ != -1) {
    ::close(stdinFd_);
    stdinFd_ = -1;
  }
}

int Subprocess::wait() {
  if (pid_ == -1) return -1;
  int status;
  pid_t r;
  do {
    r = ::waitpid(pid_, &status, 0);
  } while (r < 0 && errno == EINTR);
  pid_ = -1;
  if (WIFEXITED(status)) return WEXITSTATUS(status);
  return -1;
}

std::string Subprocess::exitDescription(int graceMs) {
  if (pid_ == -1) return "already reaped";
  int status = 0;
  pid_t r = 0;
  for (int waited = 0; ; waited += 10) {
    do { r = ::waitpid(pid_, &status, WNOHANG); } while (r < 0 && errno == EINTR);
    if (r == pid_ || r < 0) break;
    if (waited >= graceMs) return "still running";
    ::usleep(10'000);
  }
  if (r < 0) return "wait() failed";
  pid_ = -1;
  if (WIFEXITED(status)) return "exited with code " + std::to_string(WEXITSTATUS(status));
  if (WIFSIGNALED(status)) {
    int const sig = WTERMSIG(status);
    char const* name = nullptr;
    switch (sig) {
      case SIGILL:  name = "illegal instruction"; break;
      case SIGSEGV: name = "segfault";            break;
      case SIGABRT: name = "abort";               break;
      case SIGBUS:  name = "bus error";           break;
      case SIGFPE:  name = "FPE";                 break;
      case SIGKILL: name = "killed";              break;
      default: break;
    }
    return "killed by signal " + std::to_string(sig) +
           (name ? std::string(" (") + name + ")" : "");
  }
  return "ended abnormally";
}

std::string Subprocess::runCapture(std::vector<std::string> args) {
  Subprocess proc(std::move(args), {.captureStdout = true, .captureStdin = false});
  std::string out;
  std::uint8_t buf[4096];
  while (true) {
    std::size_t r = proc.read(buf, sizeof(buf));
    if (r == 0) break;
    out.append(reinterpret_cast<char const*>(buf), r);
  }
  int rc = proc.wait();
  if (rc != 0) {
    throw std::runtime_error("subprocess exited with code " + std::to_string(rc));
  }
  return out;
}

}  // namespace mosaic
