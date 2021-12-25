#include <chrono>
#include <deque>
#include <experimental/coroutine>
#include <iostream>
#include <vector>

#include "async/FDService.h"

struct Scheduler {
  Scheduler() : fdService{*this} {}

  static Scheduler &getInstance() {
    static Scheduler instance;
    return instance;
  }

  void enqueue(std::experimental::coroutine_handle<> coro) {
    queue.push_back(coro);
  }

  std::experimental::coroutine_handle<> pop_front() {
    auto front = queue.front();
    queue.pop_front();
    return front;
  }

  auto yield() {
    struct Awaitable {

      Awaitable(Scheduler &_scheduler) : scheduler{_scheduler} {}

      bool await_ready() { return false; }

      // TODO: maybe return void here instead?
      std::experimental::coroutine_handle<>
      await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
        scheduler.enqueue(awaitingCoro);
        return scheduler.pop_front();
      }

      void await_resume() {}

    private:
      Scheduler &scheduler;
    };

    return Awaitable{*this};
  }

  auto sleep(std::chrono::steady_clock::duration duration) {
    struct Awaitable {
      Awaitable(Scheduler &_scheduler,
                std::chrono::steady_clock::duration _duration)
          : scheduler{_scheduler}, duration{_duration} {}

      bool await_ready() { return false; }

      void await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
        scheduler.sleepingCoros.emplace_back(
            std::chrono::steady_clock::now() + duration, awaitingCoro);
      }

      void await_resume() {}

    private:
      Scheduler &scheduler;
      std::chrono::steady_clock::duration duration;
    };

    return Awaitable{*this, duration};
  }

  // TODO: reentrance check
  void run() {
    while (true) {
      bool done = true;

      if (!queue.empty()) {
        done = false;

        auto next = queue.front();
        queue.pop_front();
        std::cout << "Resuming normal" << std::endl;
        next.resume();
      }

      if (!sleepingCoros.empty()) {
        auto now = std::chrono::steady_clock::now();

        for (auto it = sleepingCoros.begin(); it != sleepingCoros.end(); it++) {
          auto [due, coro] = *it;

          if (now >= due) {
            sleepingCoros.erase(it);
            std::cout << "Resuming sleep" << std::endl;
            coro.resume();
            break;
          }
        }

        done = false;
      }

      if (fdService.poll()) {
        done = false;
      }

      if (done) {
        break;
      }
    }
  }

  FDService &fd() { return fdService; }

private:
  std::deque<std::experimental::coroutine_handle<>> queue;
  std::vector<std::pair<std::chrono::steady_clock::time_point,
                        std::experimental::coroutine_handle<>>>
      sleepingCoros;

  FDService fdService;
};
