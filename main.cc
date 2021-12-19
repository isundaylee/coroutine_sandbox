#include <coroutine>
#include <iostream>
#include <optional>

#include "async/Task.h"

Task<int> f() {
  std::cout << 1 << std::endl;
  throw std::runtime_error("NONONO");
  co_return 42;
}

int main() try {
  Task<int> task_f = f();
  std::cout << "Result: " << task_f.get_result() << std::endl;
  return 0;
} catch (const std::exception &ex) {
  std::cout << "Got exception: " << ex.what() << std::endl;
}
