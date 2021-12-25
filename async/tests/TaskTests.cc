#include <chrono>

#include "gtest/gtest.h"

#include <async/Scheduler.h>
#include <async/Task.h>

using namespace std::chrono_literals;

struct TestException : std::exception {};

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

template <typename T> T runSingleTask(Task<T> &task) {
  Scheduler::getInstance().enqueue(task.coro);
  Scheduler::getInstance().run();
  return task.getResult();
}

TEST(TaskTests, Basic) {
  Task<int> task = noop_int();
  ASSERT_EQ(runSingleTask(task), 42);
}

TEST(TaskTests, Ref) {
  Task<int &> task_a = noop_ref();
  Task<int &> task_b = noop_ref();
  int &result_a = runSingleTask(task_a);
  int &result_b = runSingleTask(task_b);
  ASSERT_EQ(result_a, 42);
  ASSERT_EQ(result_b, 42);
  ASSERT_EQ(&result_a, &result_b);
}

TEST(TaskTests, Void) {
  Task<void> task = noop_void();
  runSingleTask(task);
}

TEST(TaskTests, NestedWithSuspension) {
  Task<int> task = calling_sleep_1ms();
  ASSERT_EQ(runSingleTask(task), 42);
}

TEST(TaskExitTests, GetResultWhenNotReady) {
  Task<int> task = calling_sleep_1ms();
  ASSERT_EXIT(task.getResult(), testing::KilledBySignal(SIGABRT),
              "Assertion failed: \\(ready && \"getResult\\(\\) called on a "
              "TaskPromise that's not ready\\.\"\\), function getResult.+");
}

////////////////////////////////////////////////////////////////////////////////
// Exception handling tests

Task<int> throwing_int() {
  throw TestException{};
  co_return 42;
}

Task<int &> throwing_ref() {
  throw TestException{};
  int *a = new int{42};
  co_return *a;
}

Task<void> throwing_void() {
  throw TestException{};
  co_return;
}

TEST(TaskExitTests, ExceptionBasic) {
  Task<int> noop = throwing_int();
  ASSERT_THROW(runSingleTask(noop), TestException);
}

TEST(TaskExitTests, ExceptionVoid) {
  Task<void> noop = throwing_void();
  ASSERT_THROW(runSingleTask(noop), TestException);
}

TEST(TaskExitTests, ExceptionRef) {
  Task<int &> noop = throwing_ref();
  ASSERT_THROW(runSingleTask(noop), TestException);
}
