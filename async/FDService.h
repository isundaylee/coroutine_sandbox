#include <poll.h>

#include <experimental/coroutine>
#include <tuple>
#include <vector>

struct Scheduler;

struct FDService {
  FDService(Scheduler &_scheduler) : scheduler{_scheduler} {}

  struct PollAwaitable {
    friend class FDService;

    PollAwaitable(FDService &_fdService, int _fd, int _pollMask)
        : fdService{_fdService}, fd{_fd}, pollMask{_pollMask} {}

    bool await_ready() { return false; }
    void await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
      fdService.awaitingCoros.emplace_back(fd, pollMask, awaitingCoro);
    }
    void await_resume() {}

  private:
    FDService &fdService;
    int fd;
    int pollMask;
  };

  friend struct WaitToReadAwaitable;

  PollAwaitable waitToRead(int fd) {
    return {*this, fd, POLLIN | POLLPRI | POLLHUP};
  }
  PollAwaitable waitToWrite(int fd) { return {*this, fd, POLLOUT}; }

  // Returns true iff any remaining work is left.
  bool poll();

private:
  Scheduler &scheduler;

  // (fd, pollMask, coroutine)
  std::vector<std::tuple<int, int, std::experimental::coroutine_handle<>>>
      awaitingCoros;
};
