#include <experimental/coroutine>
#include <iostream>
#include <optional>

// TODO: add void support

template <typename T> struct Task;

struct TaskUsageError : std::exception {};

namespace detail {
struct PromiseBase {
  PromiseBase() {}
  ~PromiseBase() { std::cout << "Promise destructed" << std::endl; }

  friend struct FinalAwaitable;
  struct FinalAwaitable {
    bool await_ready() const noexcept { return false; }

    template <typename PromiseType>
    std::experimental::coroutine_handle<>
    await_suspend(std::experimental::coroutine_handle<PromiseType> awaitingCoro)
        const noexcept {
      PromiseBase &promise = awaitingCoro.promise();
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

template <typename T> struct Promise : PromiseBase {
  friend struct Task<T>;

  using CoroutineHandle = std::experimental::coroutine_handle<Promise>;

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

    throw TaskUsageError{};
  }

private:
  std::optional<T> result;
  bool hadException = false;
};

template <> struct Promise<void> : PromiseBase {
  friend struct Task<void>;

  using CoroutineHandle = std::experimental::coroutine_handle<Promise>;

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

    throw TaskUsageError{};
  }

private:
  bool hadResult = false;
  bool hadException = false;
};

template <typename T> struct Promise<T &> : PromiseBase {
  friend struct Task<T &>;

  using CoroutineHandle = std::experimental::coroutine_handle<Promise>;

  Task<T &> get_return_object();

  void return_value(T& value) {
    std::cout << "return_value" << std::endl;
    result = &value;
  }
  void unhandled_exception() { hadException = true; }

  T& getResult() {
    if (result) {
      return **result;
    }

    if (hadException) {
      throw std::runtime_error("Task had exception.");
    }

    throw TaskUsageError{};
  }

private:
  std::optional<T*> result;
  bool hadException = false;
};
} // namespace detail

template <typename T> struct Task {
  using promise_type = detail::Promise<T>;

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
    check_coro();
    return coro.done();
  }

  decltype(auto) getResult() {
    check_coro();
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

private:
  void check_coro() const {
    if (!coro) {
      throw TaskUsageError{};
    }
  }
};

namespace detail {
inline Task<void> Promise<void>::get_return_object() {
  return {CoroutineHandle::from_promise(*this)};
}

template <typename T> Task<T &> Promise<T &>::get_return_object() {
  return {CoroutineHandle::from_promise(*this)};
}
} // namespace detail
