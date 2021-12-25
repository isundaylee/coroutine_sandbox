#include <chrono>

#include "gtest/gtest.h"

#include <async/Scheduler.h>
#include <async/Task.h>

using namespace std::chrono_literals;

struct TestException : std::exception {};

Task<int> noopInt() { co_return 42; }

Task<int &> noopRef() {
  static int *a = new int{42};
  co_return *a;
}

Task<void> noopVoid() { co_return; }

Task<int> sleep1ms() {
  co_await Scheduler::getInstance().sleep(1ms);
  co_return 42;
}

Task<int> callingSleep1ms() { co_return(co_await sleep1ms()); }

template <typename T> T runSingleTask(Task<T> &task) {
  Scheduler::getInstance().enqueue(task.coro);
  Scheduler::getInstance().run();
  return task.getResult();
}

TEST(TaskTests, Basic) {
  Task<int> task = noopInt();
  ASSERT_EQ(runSingleTask(task), 42);
}

TEST(TaskTests, Ref) {
  Task<int &> taskA = noopRef();
  Task<int &> taskB = noopRef();
  int &resultA = runSingleTask(taskA);
  int &resultB = runSingleTask(taskB);
  ASSERT_EQ(resultA, 42);
  ASSERT_EQ(resultB, 42);
  ASSERT_EQ(&resultA, &resultB);
}

TEST(TaskTests, Void) {
  Task<void> task = noopVoid();
  runSingleTask(task);
}

TEST(TaskTests, NestedWithSuspension) {
  Task<int> task = callingSleep1ms();
  ASSERT_EQ(runSingleTask(task), 42);
}

struct NonDefaultConstructible {
  int value;
  NonDefaultConstructible(int value_) : value{value_} {}

  bool operator==(const NonDefaultConstructible &rhs) const noexcept {
    return value == rhs.value;
  }
};

Task<NonDefaultConstructible> noopNonDefaultConstructibleResult() {
  co_return NonDefaultConstructible{42};
}

TEST(TaskTests, NonDefaultConstructible) {
  Task<NonDefaultConstructible> task = noopNonDefaultConstructibleResult();
  ASSERT_EQ(runSingleTask(task), NonDefaultConstructible{42});
}

Task<void> syncLoop() {
  for (size_t i = 0; i < 1000000; i++) {
    co_await noopVoid();
  }
}

TEST(TaskTests, SyncLoop) {
  // Tests that we don't run out stack
  Task<void> task = syncLoop();
  runSingleTask(task);
}

TEST(TaskExitTests, GetResultWhenNotReady) {
  Task<int> task = callingSleep1ms();
  ASSERT_EXIT(task.getResult(), testing::KilledBySignal(SIGABRT),
              "Assertion failed: \\(ready && \"getResult\\(\\) called on a "
              "TaskPromise that's not ready\\.\"\\), function getResult.+");
}

////////////////////////////////////////////////////////////////////////////////
// Exception handling tests

Task<int> throwingInt() {
  throw TestException{};
  co_return 42;
}

Task<int &> throwingRef() {
  throw TestException{};
  int *a = new int{42};
  co_return *a;
}

Task<void> throwingVoid() {
  throw TestException{};
  co_return;
}

TEST(TaskExitTests, ExceptionBasic) {
  Task<int> noop = throwingInt();
  ASSERT_THROW(runSingleTask(noop), TestException);
}

TEST(TaskExitTests, ExceptionVoid) {
  Task<void> noop = throwingVoid();
  ASSERT_THROW(runSingleTask(noop), TestException);
}

TEST(TaskExitTests, ExceptionRef) {
  Task<int &> noop = throwingRef();
  ASSERT_THROW(runSingleTask(noop), TestException);
}
