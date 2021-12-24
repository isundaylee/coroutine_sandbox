#include <chrono>

#include "gtest/gtest.h"

#include <async/Scheduler.h>
#include <async/Task.h>

using namespace std::chrono_literals;

Task<int> noop_int() { co_return 42; }
Task<int &> noop_ref() {
  static int *a = new int{42};
  co_return *a;
}
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

TEST(TaskTests, Ref) {
  Task<int &> task_a = noop_ref();
  Task<int &> task_b = noop_ref();
  ASSERT_EQ(task_a.getResult(), 42);
  ASSERT_EQ(task_b.getResult(), 42);
  ASSERT_EQ(&task_a.getResult(), &task_b.getResult());
}

TEST(TaskTests, Void) {
  Task<void> task = noop_void();
  task.getResult();
}

TEST(TaskTests, NestedWithSuspension) {
  Task<int> task = calling_sleep_1ms();
  Scheduler::getInstance().run();
  ASSERT_EQ(task.getResult(), 42);
}

TEST(TaskExitTests, GetResultWhenNotReady) {
  Task<int> task = calling_sleep_1ms();
  ASSERT_EXIT(task.getResult(), testing::KilledBySignal(SIGABRT),
              "Assertion failed: \\(false && \"getResult\\(\\) called on a "
              "TaskPromise that's not ready\\.\"\\), function getResult.+");
}
