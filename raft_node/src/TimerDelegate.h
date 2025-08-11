#pragma once
#include "ITimerStrategy.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

class TimerDelegate {
public:
    TimerDelegate();
    ~TimerDelegate();

    void start(std::unique_ptr<ITimerStrategy> strategy);
    void stop();
    void reset();
    void changeStrategy(std::unique_ptr<ITimerStrategy> strategy);

private:
    void runLoop();

    std::mutex mutex_;
    std::condition_variable cv_;
    std::thread worker_;
    std::shared_ptr<ITimerStrategy> current_strategy_;

    std::atomic<bool> stop_flag_{false};
    std::atomic<bool> reset_flag_{false};
    std::atomic<bool> strategy_changed_{false};
};