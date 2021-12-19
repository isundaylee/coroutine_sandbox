#include <coroutine>

template <typename T> struct Task {
  struct Promise {
    friend struct Task<T>;

    using CoroutineHandle = std::coroutine_handle<Promise>;

    Task<T> get_return_object() {
      return {CoroutineHandle::from_promise(*this)};
    }
    std::suspend_never initial_suspend() noexcept { return {}; }
    std::suspend_never final_suspend() noexcept { return {}; }
    void return_value(int _result) { result = _result; }
    void unhandled_exception() { had_exception = true; }

  private:
    std::optional<T> result;
    bool had_exception = false;
  };

  using promise_type = Promise;

  Task(Promise::CoroutineHandle _coro) : coro{_coro} {}

  Task(const Task &) = delete;
  Task(Task &&) = default;
  Task &operator=(const Task &) = delete;
  Task &operator=(Task &&) = delete;

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

private:
  Promise::CoroutineHandle coro;
};
