#include <experimental/coroutine>
#include <iostream>
#include <optional>

// TODO: add void support

template <typename T> struct Task {
  struct UsageError : std::exception {};

  struct Promise {
    friend struct Task<T>;

    using CoroutineHandle = std::experimental::coroutine_handle<Promise>;

    Promise() {}
    ~Promise() { std::cout << "Promise destructed" << std::endl; }

    Task<T> get_return_object() {
      return {CoroutineHandle::from_promise(*this)};
    }
    std::experimental::suspend_never initial_suspend() noexcept { return {}; }
    std::experimental::suspend_always final_suspend() noexcept { return {}; }
    void return_value(int _result) {
      std::cout << "return_value: " << _result << std::endl;
      result = _result;
      run_continuation();
    }
    void unhandled_exception() {
      had_exception = true;
      run_continuation();
    }

    bool is_ready() const { return !!result || had_exception; }

  private:
    std::optional<T> result;
    bool had_exception = false;

    std::optional<std::experimental::coroutine_handle<>> continuation;

    void run_continuation() {
      if (continuation) {
        std::cout << "Running continuation" << std::endl;
        continuation->resume();
      }
    }
  };

  using promise_type = Promise;

  typename Promise::CoroutineHandle coro;

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

  Task(typename Promise::CoroutineHandle _coro) : coro{_coro} {}

  ~Task() {
    if (coro) {
      coro.destroy();
    }
  }

  bool is_ready() const {
    check_coro();
    Promise &promise = coro.promise();
    return !!promise.result || promise.had_exception;
  }

  T &get_result() {
    check_coro();

    Promise &promise = coro.promise();

    if (promise.result) {
      return *promise.result;
    }

    if (promise.had_exception) {
      throw std::runtime_error("Task had exception.");
    }

    throw std::runtime_error("Task result not ready yet.");
  }

  // Awaitable interface

  bool await_ready() const { return is_ready(); }

  void await_suspend(std::experimental::coroutine_handle<> awaitingCoro) {
    // TODO check if a continuation already exists?
    coro.promise().continuation = awaitingCoro;
  }

  T &await_resume() { return get_result(); }

private:
  void check_coro() const {
    if (!coro) {
      throw UsageError{};
    }
  }
};
