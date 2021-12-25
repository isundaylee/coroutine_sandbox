#include <chrono>
#include <experimental/coroutine>
#include <iostream>
#include <optional>

#include "async/Scheduler.h"
#include "async/Task.h"

using namespace std::chrono_literals;

Task<void> readFile() {
  FILE *f = fopen("/tmp/a", "r");
  co_await Scheduler::getInstance().fd().waitToRead(fileno(f));
  std::cout << "Ready to be read" << std::endl;
  co_return;
}

int main() try {
  Task<void> task = readFile();
  Scheduler::getInstance().enqueue(task.coro);
  Scheduler::getInstance().run();
  return 0;
} catch (const std::exception &ex) {
  std::cout << "Got exception: " << ex.what() << std::endl;
}
