#include "ThreadWrapper.hpp"

ThreadWrapper::ThreadWrapper(ThreadWrapper&& rhs) noexcept 
    : thread_(std::move(rhs.thread_)),
      startPromise_(std::move(rhs.startPromise_)),
      didRun(rhs.didRun.load()) 
{
    rhs.didRun.store(false);
}

ThreadWrapper& ThreadWrapper::operator=(ThreadWrapper&& rhs) noexcept {
    if (this != &rhs) {
        if (thread_.joinable()) {
            thread_.detach();
        }
        
        thread_ = std::move(rhs.thread_);
        startPromise_ = std::move(rhs.startPromise_);
        didRun.store(rhs.didRun.load());
        rhs.didRun.store(false);
    }
    return *this;
}

bool ThreadWrapper::run() noexcept {
    if(didRun.load()) return false;
    startPromise_.set_value();
    didRun.store(true);
    return true;
}

// template <typename F, typename... Args>
// ThreadWrapper::ThreadWrapper(F&& f, Args&&... args) {
//     thread_ = std::thread([f_ = std::forward<F>(f), args_ = std::make_tuple(std::forward<Args>(args)...)](std::future<void> startSignal_) mutable {
//         startSignal_.wait();
//         std::apply(f_, args_);  //c++17
//     }, startPromise_.get_future());
// }

ThreadWrapper::~ThreadWrapper() {
    if(thread_.joinable()) {
        thread_.detach();
    }
}