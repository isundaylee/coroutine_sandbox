#include <cassert>

#include <experimental/coroutine>
#include <iostream>
#include <optional>

template <typename T> struct Task;

namespace detail {
struct TaskPromiseBase {
  TaskPromiseBase() {}
  ~TaskPromiseBase() { std::cout << "TaskPromise destructed" << std::endl; }

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

  std::experimental::suspend_never initial_suspend() noexcept { return {}; }
  FinalAwaitable final_suspend() noexcept { return {}; }

  void setContinuation(std::experimental::coroutine_handle<> _continuation) {
    continuation = _continuation;
  }

private:
  std::experimental::coroutine_handle<> continuation = nullptr;
};

template <typename T> struct TaskPromise : TaskPromiseBase {
  friend struct Task<T>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<T> get_return_object() { return {CoroutineHandle::from_promise(*this)}; }

  void return_value(int _result) {
    std::cout << "return_value: " << _result << std::endl;
    result = _result;
  }
  void unhandled_exception() { hadException = true; }

  T &getResult() {
    if (result) {
      return *result;
    }

    if (hadException) {
      throw std::runtime_error("Task had exception.");
    }

    assert(false && "getResult() called on a TaskPromise that's not ready.");
  }

private:
  std::optional<T> result;
  bool hadException = false;
};

template <> struct TaskPromise<void> : TaskPromiseBase {
  friend struct Task<void>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<void> get_return_object();

  void return_void() {
    std::cout << "return_void" << std::endl;
    hadResult = true;
  }
  void unhandled_exception() { hadException = true; }

  void getResult() {
    if (hadResult) {
      return;
    }

    if (hadException) {
      throw std::runtime_error("Task had exception.");
    }

    assert(false && "getResult() called on a TaskPromise that's not ready.");
  }

private:
  bool hadResult = false;
  bool hadException = false;
};

template <typename T> struct TaskPromise<T &> : TaskPromiseBase {
  friend struct Task<T &>;

  using CoroutineHandle = std::experimental::coroutine_handle<TaskPromise>;

  Task<T &> get_return_object();

  void return_value(T &value) {
    std::cout << "return_value" << std::endl;
    result = &value;
  }
  void unhandled_exception() { hadException = true; }

  T &getResult() {
    if (result) {
      return **result;
    }

    if (hadException) {
      throw std::runtime_error("Task had exception.");
    }

    assert(false && "getResult() called on a TaskPromise that's not ready.");
  }

private:
  std::optional<T *> result;
  bool hadException = false;
};
} // namespace detail

template <typename T> struct Task {
  using promise_type = detail::TaskPromise<T>;

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

  bool isReady() const {
    assert(coro);
    return coro.done();
  }

  decltype(auto) getResult() {
    assert(coro);
    promise_type &promise = coro.promise();
    return promise.getResult();
  }

  // Awaitable interface

  bool await_ready() const { return isReady(); }

  void await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
    // TODO check if a continuation already exists?
    coro.promise().setContinuation(awaitingCoro);
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
