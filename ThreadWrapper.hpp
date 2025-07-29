
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
    bool didRun = false;

    public:
    ThreadWrapper(const ThreadWrapper&) = delete;
    ThreadWrapper& operator=(const ThreadWrapper&) = delete;

    bool run() noexcept {
        if(didRun) return false;
        startPromise_.set_value();
        didRun = true;
        return didRun;
    }

    template <typename F, typename... Args>
    explicit ThreadWrapper(F&& f, Args&&... args) {
        thread_ = std::thread([f_ = std::forward<F>(f), args_ = std::make_tuple(std::forward<Args>(args)...)](std::future<void> startSignal_) mutable {
            startSignal_.wait();
            std::apply(f_, args_);  //c++17
        }, startPromise_.get_future());
    }

    ThreadWrapper(ThreadWrapper&& rhs) noexcept : thread_(std::move(rhs.thread_)) {}

    ~ThreadWrapper() {
        if(thread_.joinable()) {
            thread_.detach();
        }
    }
};

/*
class ThreadWrapper {
    static_assert(__cplusplus >= 201703L);
private:
    std::thread thread_;
    std::promise<void> startPromise_;
    std::atomic<bool> shouldStop_{false};
    std::atomic<bool> running_{false};

public:
    // Deleted copy operations
    ThreadWrapper(const ThreadWrapper&) = delete;
    ThreadWrapper& operator=(const ThreadWrapper&) = delete;

    // Main constructor (unchanged from original)
    template <typename F, typename... Args>
    explicit ThreadWrapper(F&& f, Args&&... args) {
        thread_ = std::thread([this, f_ = std::forward<F>(f), 
                             args_ = std::make_tuple(std::forward<Args>(args)...)]
                            (std::future<void> startSignal) mutable {
            startSignal.wait();
            running_ = true;
            std::apply(f_, args_);  // Execute the function
            running_ = false;
        }, startPromise_.get_future());
    }

    // Move operations
    ThreadWrapper(ThreadWrapper&& rhs) noexcept :
        thread_(std::move(rhs.thread_)),
        shouldStop_(rhs.shouldStop_.load()),
        running_(rhs.running_.load())
    {}

    // Control methods
    bool run() noexcept {
        if(running_) return false;
        startPromise_.set_value();
        return true;
    }

    void requestStop() noexcept {
        shouldStop_ = true;
    }

    bool shouldStop() const noexcept {
        return shouldStop_;
    }

    bool isRunning() const noexcept {
        return running_;
    }

    ~ThreadWrapper() {
        requestStop();
        if(thread_.joinable()) {
            if(running_) {
                thread_.join();
            } else {
                thread_.detach();
            }
        }
    }
};
*/