#include <cassert>

#include <experimental/coroutine>
#include <iostream>
#include <optional>

template <typename T> struct Task;

namespace detail {
struct TaskPromiseBase {
  TaskPromiseBase() {}

  friend struct FinalAwaitable;
  struct FinalAwaitable {
    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    std::experimental::coroutine_handle<>
    await_suspend(std::experimental::coroutine_handle<PromiseType> awaitingCoro)
        const noexcept {
      TaskPromiseBase &promise = awaitingCoro.promise();
      return promise.continuation ? promise.continuation
                                  : std::experimental::noop_coroutine();
    }

    void await_resume() const noexcept {}
  };

  std::experimental::suspend_always initial_suspend() noexcept { return {}; }
  FinalAwaitable final_suspend() noexcept { return {}; }

  void setContinuation(std::experimental::coroutine_handle<> _continuation) {
    assert(!continuation);
    continuation = _continuation;
  }

private:
  std::experimental::coroutine_handle<> continuation = nullptr;
};

template <typename T> struct TaskPromise : TaskPromiseBase {
  friend struct Task<T>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<T> get_return_object() { return {CoroutineHandle::from_promise(*this)}; }

  void return_value(T _result) {
    result = _result;
    ready = true;
  }
  void unhandled_exception() {
    exception = std::current_exception();
    ready = true;
  }

  T &getResult() {
    assert(ready && "getResult() called on a TaskPromise that's not ready.");

    if (exception) {
      std::rethrow_exception(exception);
    }

    return result;
  }

private:
  bool ready = false;
  T result;
  std::exception_ptr exception;
};

template <> struct TaskPromise<void> : TaskPromiseBase {
  friend struct Task<void>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<void> get_return_object();

  void return_void() {
    ready = true;
  }
  void unhandled_exception() {
    exception = std::current_exception();
    ready = true;
  }

  void getResult() {
    assert(ready && "getResult() called on a TaskPromise that's not ready.");

    if (exception) {
      std::rethrow_exception(exception);
    }
  }

private:
  bool ready = false;
  std::exception_ptr exception;
};

template <typename T> struct TaskPromise<T &> : TaskPromiseBase {
  friend struct Task<T &>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<T &> get_return_object();

  void return_value(T &value) {
    result = &value;
    ready = true;
  }
  void unhandled_exception() {
    exception = std::current_exception();
    ready = true;
  }

  T &getResult() {
    assert(ready && "getResult() called on a TaskPromise that's not ready.");

    if (exception) {
      std::rethrow_exception(exception);
    }

    return *result;
  }

private:
  bool ready = false;
  T *result;
  std::exception_ptr exception;
};
} // namespace detail

template <typename T> struct Task {
  using promise_type = detail::TaskPromise<T>;

  // TODO: This should be private (currently public for use in tests)
  typename promise_type::CoroutineHandle coro;

  Task(const Task &) = delete;
  Task(Task &&rhs) : coro{rhs.coro} { rhs.coro = nullptr; }
  Task &operator=(const Task &) = delete;
  Task &operator=(Task &&rhs) {
    if (std::addressof(rhs) != this) {
      if (coro) {
        coro.destroy();
      }

      coro = rhs.coro;
      rhs.coro = nullptr;
    }

    return *this;
  }

  Task(typename promise_type::CoroutineHandle _coro) : coro{_coro} {}

  ~Task() {
    if (coro) {
      coro.destroy();
    }
  }

  decltype(auto) getResult() {
    assert(coro);
    promise_type &promise = coro.promise();
    return promise.getResult();
  }

  // Awaitable interface

  bool await_ready() const {
    assert(coro);
    return coro.done();
  }

  void await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
    coro.promise().setContinuation(awaitingCoro);
    coro.resume();
  }

  decltype(auto) await_resume() { return getResult(); }
};

namespace detail {
inline Task<void> TaskPromise<void>::get_return_object() {
  return {CoroutineHandle::from_promise(*this)};
}

template <typename T> Task<T &> TaskPromise<T &>::get_return_object() {
  return {CoroutineHandle::from_promise(*this)};
}
} // namespace detail
