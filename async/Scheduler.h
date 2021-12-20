#include <chrono>
#include <coroutine>
#include <deque>
#include <vector>

struct Scheduler {
  static Scheduler &getInstance() {
    static Scheduler instance;
    return instance;
  }

  void enqueue(std::coroutine_handle<> coro) { queue.push_back(coro); }

  std::coroutine_handle<> pop_front() {
    auto front = queue.front();
    queue.pop_front();
    return front;
  }

  auto yield() {
    struct Awaitable {

      Awaitable(Scheduler &_scheduler) : scheduler{_scheduler} {}

      bool await_ready() { return false; }

      std::coroutine_handle<>
      await_suspend(std::coroutine_handle<> awaitingCoro) {
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

      void await_suspend(std::coroutine_handle<> awaitingCoro) {
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

      if (done) {
        break;
      }
    }
  }

private:
  std::deque<std::coroutine_handle<>> queue;
  std::vector<
      std::pair<std::chrono::steady_clock::time_point, std::coroutine_handle<>>>
      sleepingCoros;
};
