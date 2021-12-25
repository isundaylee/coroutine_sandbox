#include "async/FDService.h"

#include <string>

static const int kPollTimeout = 10;

bool FDService::poll() {
  if (awaitingCoros.empty()) {
    return false;
  }

  // Let's poll!
  size_t numCoros = awaitingCoros.size();
  struct pollfd polls[numCoros];
  for (size_t i = 0; i < numCoros; i++) {
    auto [fd, pollMask, coro] = awaitingCoros[i];
    polls[i].fd = fd;
    polls[i].events = pollMask;
    polls[i].revents = 0;
  }
  int ret = ::poll(polls, numCoros, kPollTimeout);

  if (ret < 0) {
    if (errno == EINTR) {
      // We have probably been interrupted by signals set by another condition
      // type. We should return and give the runloop a chance to proceed.
      return true;
    }

    throw std::runtime_error("Error encountered while poll()-ing: " +
                             std::string(strerror(errno)));
  }

  // Enable connections according to poll result
  for (size_t i = 0; i < numCoros; i++) {
    if (polls[i].revents & POLLNVAL) {
      throw std::runtime_error("Invalid file descriptor: " +
                               std::to_string(polls[i].fd));
    }

    if (polls[i].revents & polls[i].events) {
      std::experimental::coroutine_handle<> coroToResume =
          std::get<2>(awaitingCoros[i]);
      awaitingCoros.erase(awaitingCoros.begin() + i);
      coroToResume.resume();
      return true;
    }
  }

  // TODO we can prob return false here if no awaitingCoros is left
  return true;
}
