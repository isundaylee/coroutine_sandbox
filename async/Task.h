#include <coroutine>
#include <optional>

// TODO: add void support

template <typename T> struct Task {
  struct Promise {
    friend struct Task<T>;

    using CoroutineHandle = std::coroutine_handle<Promise>;

    Promise() {}
    ~Promise() { std::cout << "Promise destructed" << std::endl; }

    Task<T> get_return_object() {
      return {CoroutineHandle::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_always final_suspend() noexcept { return {}; }
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
    // FIXME: this does not work, gets destroyed too early
    std::optional<T> result;
    bool had_exception = false;

    std::optional<std::coroutine_handle<>> continuation;

    void run_continuation() {
      if (continuation) {
        std::cout << "Running continuation" << std::endl;
        continuation->resume();
      }
    }
  };

  using promise_type = Promise;

  Promise::CoroutineHandle coro;

  Task(const Task &) = delete;
  Task(Task &&) = delete;
  Task &operator=(const Task &) = delete;
  Task &operator=(Task &&) = delete;

  Task(Promise::CoroutineHandle _coro) : coro{_coro} {}

  ~Task() { coro.destroy(); }

  bool is_ready() const {
    Promise &promise = coro.promise();
    return !!promise.result || promise.had_exception;
  }

  T &get_result() {
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

  void await_suspend(std::coroutine_handle<> awaitingCoro) {
    // TODO check if a continuation already exists?
    coro.promise().continuation = awaitingCoro;
  }

  T &await_resume() { return get_result(); }
};
