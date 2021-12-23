#include <chrono>

#include "gtest/gtest.h"

#include <async/Task.h>
#include <async/Scheduler.h>

using namespace std::chrono_literals;

Task<int> noop() {
    co_return 42;
    }

Task<int> sleep_1ms() {
  co_await Scheduler::getInstance().sleep(1ms);
  co_return 42;
}

Task<int> calling_sleep_1ms() { co_return(co_await sleep_1ms()); }

TEST(TaskTests, Basic) {
    Task<int> task = noop();
    ASSERT_EQ(task.get_result(), 42);
}

TEST(TaskTests, NestedWithSuspension) {
    Task<int> task = calling_sleep_1ms();
    EXPECT_THROW(task.get_result(), std::runtime_error);

    Scheduler::getInstance().run();
    EXPECT_EQ(task.get_result(), 42);
}
