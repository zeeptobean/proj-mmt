#pragma once

#include <thread>
#include <future>
#include <functional>
#include <atomic>

/**
 * I don't like std::thread implicitly running the thread upon creation that it would
 * need some more synchonization variables, so here's the convenience wrapper
 */
class ThreadWrapper {
    static_assert(__cplusplus >= 201703L);
    private:
    std::thread thread_;
    std::promise<void> startPromise_;
    std::atomic<bool> didRun{false};

    public:
    ThreadWrapper(const ThreadWrapper&) = delete;
    ThreadWrapper& operator=(const ThreadWrapper&) = delete;

    ThreadWrapper() noexcept = default;

    ThreadWrapper(ThreadWrapper&& rhs) noexcept;

    // Move assignment operator
    ThreadWrapper& operator=(ThreadWrapper&& rhs) noexcept;

    bool run() noexcept;

    template <typename F, typename... Args>
    explicit ThreadWrapper(F&& f, Args&&... args) {
        thread_ = std::thread([f_ = std::forward<F>(f), args_ = std::make_tuple(std::forward<Args>(args)...)](std::future<void> startSignal_) mutable {
            startSignal_.wait();
            std::apply(f_, args_);  //c++17
        }, startPromise_.get_future());
    }

    ~ThreadWrapper();
};