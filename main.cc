#include <chrono>
#include <experimental/coroutine>
#include <iostream>
#include <optional>

#include "async/Scheduler.h"
#include "async/Task.h"

using namespace std::chrono_literals;

Task<void> u() { co_await Scheduler::getInstance().sleep(1s); }
Task<void> g() { co_await u(); }
Task<void> f() { co_await g(); }

Task<void> noop() { co_return; }

Task<void> syncLoop() {
  for (size_t i = 0; i < 100; i++) {
    co_await noop();
  }
}

int main() try {
  syncLoop();
  // Task<void> task_f = f();

  // Scheduler &scheduler = Scheduler::getInstance();
  // scheduler.run();

  // task_f.getResult();
  return 0;
} catch (const std::exception &ex) {
  std::cout << "Got exception: " << ex.what() << std::endl;
}
