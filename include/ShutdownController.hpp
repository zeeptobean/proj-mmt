#pragma once
#include <atomic>
#include <mutex>
#include <condition_variable>

class ShutdownController {
public:
    static ShutdownController& getInstance();

    // Request shutdown and wait for completion
    void requestShutdown();
    
    // Check if shutdown was requested
    bool isShutdownRequested() const;
    
    // Register/Unregister resources
    void registerResource();
    void unregisterResource();
    
    // Wait for all resources to complete
    void waitForShutdown();

private:
    ShutdownController() = default;
    ~ShutdownController() = default;

    std::mutex m_mutex;
    std::condition_variable m_cv;
    std::atomic<int> m_activeResources{0};
    std::atomic<bool> m_shutdownRequested{false};
};

ShutdownController& ShutdownController::getInstance() {
    static ShutdownController instance;
    return instance;
}

void ShutdownController::requestShutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shutdownRequested = true;
    m_cv.notify_all();
}

bool ShutdownController::isShutdownRequested() const {
    return m_shutdownRequested.load();
}

void ShutdownController::registerResource() {
    std::lock_guard<std::mutex> lock(m_mutex);
    ++m_activeResources;
}

void ShutdownController::unregisterResource() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (--m_activeResources == 0 && m_shutdownRequested) {
        m_cv.notify_all();
    }
}

void ShutdownController::waitForShutdown() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] {
        return m_activeResources == 0 && m_shutdownRequested;
    });
}