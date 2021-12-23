#include <chrono>

#include "gtest/gtest.h"

#include <async/Scheduler.h>
#include <async/Task.h>

using namespace std::chrono_literals;

Task<int> noop_int() { co_return 42; }
Task<void> noop_void() { co_return; }

Task<int> sleep_1ms() {
  co_await Scheduler::getInstance().sleep(1ms);
  co_return 42;
}

Task<int> calling_sleep_1ms() { co_return(co_await sleep_1ms()); }

TEST(TaskTests, Basic) {
  Task<int> task = noop_int();
  ASSERT_EQ(task.getResult(), 42);
}

TEST(TaskTests, Void) {
  Task<void> task = noop_void();
  task.getResult();
}

TEST(TaskTests, NestedWithSuspension) {
  Task<int> task = calling_sleep_1ms();
  EXPECT_THROW(task.getResult(), TaskUsageError);

  Scheduler::getInstance().run();
  EXPECT_EQ(task.getResult(), 42);
}
