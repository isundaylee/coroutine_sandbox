#include <chrono>
#include <coroutine>
#include <iostream>
#include <optional>

#include "async/Scheduler.h"
#include "async/Task.h"

using namespace std::chrono_literals;

Task<int> g() {
  co_await Scheduler::getInstance().sleep(1s);
  co_return 42;
}

Task<int> f() { co_return(co_await g()); }

int main() try {
  Task<int> task_f = f();

  Scheduler &scheduler = Scheduler::getInstance();
  scheduler.run();

  std::cout << "Result: " << task_f.get_result() << std::endl;

  return 0;
} catch (const std::exception &ex) {
  std::cout << "Got exception: " << ex.what() << std::endl;
}
